// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DGenerator.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AWFC3DGenerator::AWFC3DGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AWFC3DGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoGenerateOnBeginPlay)
	{
		Generate3DSpace();
	}
}

void AWFC3DGenerator::ScanDirectoryAssets()
{
	UWFC3DModuleLibrary* TargetLib = ModuleLibrary;
	if (!TargetLib)
	{
		if (!RuntimeLibrary)
		{
			RuntimeLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("RuntimeWFCLibrary"));
		}
		TargetLib = RuntimeLibrary;
	}

	TargetLib->ScanDirectoryForStaticMeshes(
		ScanFolderPath,
		DefaultHorizontalSocket,
		DefaultVerticalSocket,
		bAutoRotationsOnScan,
		/* bClearExisting = */ false
	);

	UE_LOG(LogTemp, Log, TEXT("[WFC Generator] Scanned '%s', target library now has %d modules."), *ScanFolderPath, TargetLib->Modules.Num());
}

bool AWFC3DGenerator::PrepareLibrary()
{
	UWFC3DModuleLibrary* TargetLib = ModuleLibrary ? ModuleLibrary.Get() : RuntimeLibrary.Get();

	if (!TargetLib || TargetLib->Modules.Num() == 0)
	{
		// 自动从指定目录即时载入
		if (!RuntimeLibrary)
		{
			RuntimeLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("RuntimeWFCLibrary"));
		}
		TargetLib = RuntimeLibrary;

		int32 Scanned = TargetLib->ScanDirectoryForStaticMeshes(
			ScanFolderPath,
			DefaultHorizontalSocket,
			DefaultVerticalSocket,
			bAutoRotationsOnScan,
			/* bClearExisting = */ false
		);

		if (Scanned == 0 && TargetLib->Modules.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[WFC Generator] Failed to find any StaticMesh assets in '%s' or assigned library!"), *ScanFolderPath);
			return false;
		}
	}

	// 同步空气负空间配置
	TargetLib->bIncludeAirModule = bIncludeAir;
	TargetLib->AirModuleWeight = AirWeight;

	if (!TargetLib->CompileLibrary(CompiledModules, CompatibilityTable))
	{
		return false;
	}

	// 初始化求解器网格
	Solver.Initialize(GridSize, CompiledModules, CompatibilityTable);
	bSolverInitialized = true;
	return true;
}

UInstancedStaticMeshComponent* AWFC3DGenerator::GetOrCreateInstancedComponent(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}

	if (TObjectPtr<UInstancedStaticMeshComponent>* FoundComp = InstancedMeshComponents.Find(Mesh))
	{
		if (*FoundComp && IsValid(*FoundComp))
		{
			return *FoundComp;
		}
	}

	// 创建新的 HISM 或 ISM 组件
	UInstancedStaticMeshComponent* NewComp = nullptr;
	const FName CompName = *FString::Printf(TEXT("HISM_%s_%d"), *Mesh->GetName(), InstancedMeshComponents.Num());

	if (bUseHISM)
	{
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, CompName);
		NewComp = HISM;
	}
	else
	{
		NewComp = NewObject<UInstancedStaticMeshComponent>(this, CompName);
	}

	if (NewComp)
	{
		NewComp->SetStaticMesh(Mesh);
		NewComp->SetupAttachment(GetRootComponent());
		NewComp->RegisterComponent();
		InstancedMeshComponents.Add(Mesh, NewComp);
	}

	return NewComp;
}

void AWFC3DGenerator::ClearGenerated()
{
	for (auto& Pair : InstancedMeshComponents)
	{
		if (Pair.Value && IsValid(Pair.Value))
		{
			Pair.Value->ClearInstances();
		}
	}
	bSolverInitialized = false;
}

void AWFC3DGenerator::RenderResult()
{
	// 先清除旧实例
	for (auto& Pair : InstancedMeshComponents)
	{
		if (Pair.Value && IsValid(Pair.Value))
		{
			Pair.Value->ClearInstances();
		}
	}

	const TArray<FWFCSlot3D>& Slots = Solver.GetSlots();
	const int32 NumSlots = Slots.Num();

	for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
	{
		const FWFCSlot3D& Slot = Slots[SlotIdx];
		if (!Slot.bCollapsed || Slot.FinalModuleIndex == INDEX_NONE)
		{
			continue;
		}

		if (!CompiledModules.IsValidIndex(Slot.FinalModuleIndex))
		{
			continue;
		}

		const FCompiledWFC3DModule& Mod = CompiledModules[Slot.FinalModuleIndex];

		// 计算单元格物理坐标 (以网格原点为中心向内对齐)
		const FIntVector Coord = Solver.IndexToCoord(SlotIdx);
		const FVector CellLocalPos = FVector(
			Coord.X * CellSize.X,
			Coord.Y * CellSize.Y,
			Coord.Z * CellSize.Z
		);

		// 1. 渲染主网格体 (如果存在)
		if (UStaticMesh* Mesh = Mod.Mesh.Get())
		{
			if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(Mesh))
			{
				FTransform CellTransform = Mod.LocalTransform;
				CellTransform.AddToTranslation(CellLocalPos);
				Comp->AddInstance(CellTransform, /* bWorldSpace = */ false);
			}
		}

		// 2. 渲染复合部件 (例如每个模块自备的地板块底座、多段拐角墙体、立柱)
		for (const FCompiledWFC3DMeshPart& Part : Mod.AdditionalParts)
		{
			if (UStaticMesh* PartMesh = Part.Mesh.Get())
			{
				if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(PartMesh))
				{
					FTransform PartTransform = Part.LocalTransform;
					PartTransform.AddToTranslation(CellLocalPos);
					Comp->AddInstance(PartTransform, /* bWorldSpace = */ false);
				}
			}
		}
	}
}

void AWFC3DGenerator::SetupDungeonPreset()
{
	CellSize = FVector(100.0f, 100.0f, 100.0f);
	GridSize = FIntVector(8, 8, 1);
	bIncludeAir = true;
	AirWeight = 2.0f;
	bEnforceGroundFloor = false; // 地板已经完美内嵌在每个室内单元的底座中，天然无缝贴合

	if (!RuntimeLibrary)
	{
		RuntimeLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("RuntimeWFCLibrary"));
	}
	ModuleLibrary = RuntimeLibrary;
	ModuleLibrary->Modules.Empty();
	ModuleLibrary->bIncludeAirModule = true;
	ModuleLibrary->AirModuleWeight = 2.0f;
	ModuleLibrary->AirSocketName = FName(TEXT("Exterior"));

	// 载入基础网格体
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"));

	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] Failed to load SM_Cube for Dungeon Preset!"));
		return;
	}

	// 通用地面板底座部件 (每个室内单元的基底厚度 10cm，紧贴底部 Z=-45)
	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = CubeMesh;
	FloorBasePart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -45.0f), FVector(1.0f, 1.0f, 0.1f));

	// 1. 开阔房间地板 (Room Floor)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Room_Floor"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(1.0f, 1.0f, 0.1f);
		Mod.AdditionalOffset = FVector(0.0f, 0.0f, -45.0f);
		Mod.Weight = 3.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 2. 室内柱子大厅 (Room Pillar)
	if (CylinderMesh)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Room_Pillar"));
		Mod.Mesh = CylinderMesh;
		Mod.MeshScale = FVector(0.35f, 0.35f, 0.9f);
		Mod.AdditionalOffset = FVector(0.0f, 0.0f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart); // 自带地面底板
		Mod.Weight = 0.8f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 3. 实心直墙 (Wall Straight)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Straight"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 1.0f, 0.9f); // 厚度 15cm，高 90cm
		Mod.AdditionalOffset = FVector(42.5f, 0.0f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart); // 自带地面底座
		Mod.Weight = 2.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Exterior")));   // 外侧面向室外
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));       // 内侧面向房间
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint"))); // 左右接墙
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true; // 四向自动衍生出 4 个方向墙体
		ModuleLibrary->Modules.Add(Mod);
	}

	// 4. 90度 L型转角墙 (Wall Corner - 闭合房间的关键!)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Corner"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 0.85f, 0.9f); // +X 侧墙体
		Mod.AdditionalOffset = FVector(42.5f, -7.5f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart); // 自带地面底板

		// 复合部件：+Y 侧墙体
		FWFC3DMeshPart WallYPart;
		WallYPart.Mesh = CubeMesh;
		WallYPart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-7.5f, 42.5f, 5.0f), FVector(0.85f, 0.15f, 0.9f));
		Mod.AdditionalParts.Add(WallYPart);

		// 复合部件：转角角柱
		if (CylinderMesh)
		{
			FWFC3DMeshPart CornerPillarPart;
			CornerPillarPart.Mesh = CylinderMesh;
			CornerPillarPart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(42.5f, 42.5f, 5.0f), FVector(0.2f, 0.2f, 0.9f));
			Mod.AdditionalParts.Add(CornerPillarPart);
		}

		Mod.Weight = 2.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint"))); // 外转角接直墙
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint"))); // 外转角接直墙
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));       // 内侧面向室内
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));       // 内侧面向室内
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true; // 四向自动衍生出 4 个方向转角
		ModuleLibrary->Modules.Add(Mod);
	}

	// 5. 门洞通道 (Wall Doorway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Doorway"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 1.0f, 0.2f); // 门顶横梁
		Mod.AdditionalOffset = FVector(42.5f, 0.0f, 40.0f);
		Mod.AdditionalParts.Add(FloorBasePart); // 门下自带通畅地面

		// 门框左立柱
		FWFC3DMeshPart LeftPost;
		LeftPost.Mesh = CubeMesh;
		LeftPost.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(42.5f, -40.0f, 5.0f), FVector(0.15f, 0.2f, 0.9f));
		Mod.AdditionalParts.Add(LeftPost);

		// 门框右立柱
		FWFC3DMeshPart RightPost;
		RightPost.Mesh = CubeMesh;
		RightPost.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(42.5f, 40.0f, 5.0f), FVector(0.15f, 0.2f, 0.9f));
		Mod.AdditionalParts.Add(RightPost);

		Mod.Weight = 1.2f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));       // 门洞前后通透互通
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint"))); // 左右与墙体严密接缝
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 6. T型分叉隔断墙 (Wall T-Junction)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_TJunction"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 1.0f, 0.9f); // 主直墙
		Mod.AdditionalOffset = FVector(42.5f, 0.0f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart);

		// 垂直伸出的分支隔墙
		FWFC3DMeshPart BranchPart;
		BranchPart.Mesh = CubeMesh;
		BranchPart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-7.5f, 0.0f, 5.0f), FVector(0.85f, 0.15f, 0.9f));
		Mod.AdditionalParts.Add(BranchPart);

		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Exterior")));   // 外侧
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint"))); // 垂直向内延伸接隔墙
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint"))); // 左右顺延接墙
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	ClearGenerated();
	UE_LOG(LogTemp, Log, TEXT("[WFC] Loaded Full Architectural Dungeon Preset with 6 topological modules!"));
}

void AWFC3DGenerator::Generate3DSpace()
{
	ClearGenerated();

	if (!PrepareLibrary())
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Generator] Generate3DSpace aborted: library preparation failed."));
		return;
	}

	// 查找地面板模块索引
	int32 FloorModuleIndex = INDEX_NONE;
	if (bEnforceGroundFloor)
	{
		for (int32 ModIdx = 0; ModIdx < CompiledModules.Num(); ++ModIdx)
		{
			if (CompiledModules[ModIdx].SourceModuleId == FName(TEXT("Floor")))
			{
				FloorModuleIndex = ModIdx;
				break;
			}
		}
	}

	// 设置初始约束 (底层 Z=0 强制为地面板)
	TMap<int32, int32> InitialClamps;
	if (FloorModuleIndex != INDEX_NONE)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			for (int32 Y = 0; Y < GridSize.Y; ++Y)
			{
				const int32 CellIdx = Solver.CoordToIndex(X, Y, 0);
				InitialClamps.Add(CellIdx, FloorModuleIndex);
			}
		}
	}

	const int32 EffectiveSeed = (RandomSeed != 0) ? RandomSeed : FMath::Rand();
	const double StartTime = FPlatformTime::Seconds();

	const bool bSuccess = Solver.Solve(MaxRetryAttempts, EffectiveSeed, InitialClamps);
	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	if (bSuccess)
	{
		RenderResult();
		UE_LOG(LogTemp, Log, TEXT("[WFC Generator] Successfully generated 3D space (%d x %d x %d) in %.2f ms!"),
			GridSize.X, GridSize.Y, GridSize.Z, DurationMs);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC Generator] Failed to solve 3D WFC within %d retries. Try adjusting socket rules or weights."), MaxRetryAttempts);
	}
}

void AWFC3DGenerator::StepCollapse()
{
	if (!bSolverInitialized)
	{
		ClearGenerated();
		if (!PrepareLibrary())
		{
			return;
		}
	}

	const EWFCStepResult Result = Solver.Step();
	RenderResult();

	if (Result == EWFCStepResult::Completed)
	{
		UE_LOG(LogTemp, Log, TEXT("[WFC Generator] StepCollapse: Full 3D grid collapse completed!"));
	}
	else if (Result == EWFCStepResult::Contradiction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC Generator] StepCollapse: Hit contradiction! Resetting grid for next attempt."));
		Solver.ResetGrid();
	}
}
