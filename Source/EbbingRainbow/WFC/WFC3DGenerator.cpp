// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DGenerator.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"

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
	TargetLib->AirSocketName = FName(TEXT("Air"));

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

		// 回合制战斗需要: 射线拾取/视线检测 (碰撞) 与 NavMesh 生成 (导航相关)
		NewComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		NewComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		NewComp->SetCanEverAffectNavigation(true);

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

void AWFC3DGenerator::SetupModularKitPreset()
{
	CellSize = FVector(200.0f, 200.0f, 200.0f);
	bIncludeAir = true;
	AirWeight = 2.0f;
	bEnforceGroundFloor = false;

	if (!ModuleLibrary)
	{
		ModuleLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("ModularKitWFCLibrary"));
	}

	ModuleLibrary->Modules.Empty();
	ModuleLibrary->CustomCompatibilityPairs.Empty();

	auto LoadMesh = [](const TCHAR* Path) -> UStaticMesh*
	{
		return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, Path));
	};

	UStaticMesh* FloorFlat = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Flat_01.SM_Floor_Flat_01"));
	UStaticMesh* FloorGrating = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Grating_01.SM_Floor_Grating_01"));
	UStaticMesh* FloorPathway = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Pathway_01.SM_Floor_Pathway_01"));

	UStaticMesh* WallSolid = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Solid_01.SM_Wall_Solid_01"));
	UStaticMesh* WallDoorway = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Doorway_01.SM_Wall_Doorway_01"));
	UStaticMesh* WallWindow = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Window_01.SM_Wall_Window_01"));

	UStaticMesh* CornerOuter = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Outer_01.SM_Corner_Wall_Outer_01"));
	UStaticMesh* CornerInner = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Inner_01.SM_Corner_Wall_Inner_01"));

	UStaticMesh* ColumnCyl = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Cylinder_Center_01.SM_Column_Cylinder_Center_01"));
	UStaticMesh* ColumnOct = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Octagonal_01.SM_Column_Octagonal_01"));

	if (!FloorFlat || !WallSolid)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] SetupModularKitPreset failed: Could not load FBX_Export assets from /Game/FBX_Export/!"));
		return;
	}

	// 共享底层地板部件 (保证墙体、门洞、立柱下方全部拥有平整无缝地面)
	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = FloorFlat;
	FloorBasePart.RelativeTransform = FTransform::Identity;

	// 1. 标准开阔房间地面 (Floor Flat)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Flat"));
		Mod.Mesh = FloorFlat;
		Mod.Weight = 3.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 2. 通道/过道地面 (Floor Pathway)
	if (FloorPathway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Pathway"));
		Mod.Mesh = FloorPathway;
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 3. 金属格栅地面 (Floor Grating)
	if (FloorGrating)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Grating"));
		Mod.Mesh = FloorGrating;
		Mod.Weight = 0.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 4. 实心墙体 (Wall Solid)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Solid"));
		Mod.Mesh = WallSolid;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 2.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 5. 窗框墙体 (Wall Window)
	if (WallWindow)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Window"));
		Mod.Mesh = WallWindow;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 6. 门洞墙体 (Wall Doorway)
	if (WallDoorway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Doorway"));
		Mod.Mesh = WallDoorway;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.2f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room"))); // 门洞通透连接两侧空间
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 7. 外转角墙 (Corner Wall Outer)
	if (CornerOuter)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Corner_Outer"));
		Mod.Mesh = CornerOuter;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 8. 居中圆柱立柱 (Column Cylinder Center)
	if (ColumnCyl)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Column_Cylinder"));
		Mod.Mesh = ColumnCyl;
		Mod.AdditionalParts.Add(FloorBasePart);
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

	// 9. 八角立柱 (Column Octagonal)
	if (ColumnOct)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Column_Octagonal"));
		Mod.Mesh = ColumnOct;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 0.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	ClearGenerated();
	UE_LOG(LogTemp, Log, TEXT("[WFC] Loaded FBX_Export Modular Kit Preset with %d modules! CellSize=(200, 200, 200)"), ModuleLibrary->Modules.Num());
}

void AWFC3DGenerator::SetupThreeStoryPreset()
{
	GridSize = FIntVector(8, 8, 3);
	CellSize = FVector(200.0f, 200.0f, 200.0f);
	bIncludeAir = true;
	AirWeight = 1.5f;
	bEnforceGroundFloor = false;
	bEnsureStairConnectivity = true;
	StairsPerFloorTransition = 1;
	bIncludeCeilings = true;

	if (!ModuleLibrary)
	{
		ModuleLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("ThreeStoryWFCLibrary"));
	}

	ModuleLibrary->Modules.Empty();
	ModuleLibrary->AirSocketName = FName(TEXT("Air"));
	ModuleLibrary->CustomCompatibilityPairs.Empty();
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Exterior")), FName(TEXT("Air")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Exterior")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Air")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Air")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Exterior")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Exterior")), true });

	auto LoadMesh = [](const TCHAR* Path) -> UStaticMesh*
	{
		return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, Path));
	};

	UStaticMesh* FloorFlat = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Flat_01.SM_Floor_Flat_01"));
	UStaticMesh* FloorPathway = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Pathway_01.SM_Floor_Pathway_01"));
	UStaticMesh* FloorGrating = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Grating_01.SM_Floor_Grating_01"));

	UStaticMesh* WallSolid = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Solid_01.SM_Wall_Solid_01"));
	UStaticMesh* WallDoorway = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Doorway_01.SM_Wall_Doorway_01"));
	UStaticMesh* WallWindow = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Window_01.SM_Wall_Window_01"));

	UStaticMesh* CornerOuter = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Outer_01.SM_Corner_Wall_Outer_01"));
	UStaticMesh* CornerInner = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Inner_01.SM_Corner_Wall_Inner_01"));

	UStaticMesh* ColumnCyl = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Cylinder_Center_01.SM_Column_Cylinder_Center_01"));
	UStaticMesh* ColumnOct = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Octagonal_01.SM_Column_Octagonal_01"));

	UStaticMesh* CeilingFlat = LoadMesh(TEXT("/Game/FBX_Export/SM_Ceiling_Flat_01.SM_Ceiling_Flat_01"));
	UStaticMesh* StairsMesh = LoadMesh(TEXT("/Game/FBX_Export/SM_Stairs_Straight_01.SM_Stairs_Straight_01"));

	if (!FloorFlat || !WallSolid || !StairsMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] SetupThreeStoryPreset failed: Missing required static mesh assets in /Game/FBX_Export/!"));
		return;
	}

	// 共享底层地板部件 (Z=0)
	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = FloorFlat;
	FloorBasePart.RelativeTransform = FTransform::Identity;

	// 共享顶层天花/上一层地板部件 (SM_Ceiling_Flat_01 网格自然高度即为 Z=184~200cm)
	FWFC3DMeshPart CeilingPart;
	if (bIncludeCeilings && CeilingFlat)
	{
		CeilingPart.Mesh = CeilingFlat;
		CeilingPart.RelativeTransform = FTransform::Identity;
	}

	// 1. 标准开阔房间地面 (Floor Flat) - 自带底板与顶板
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Flat"));
		Mod.Mesh = FloorFlat;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 3.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 2. 通道/过道地面 (Floor Pathway)
	if (FloorPathway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Pathway"));
		Mod.Mesh = FloorPathway;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 3. 金属格栅地面 (Floor Grating)
	if (FloorGrating)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Grating"));
		Mod.Mesh = FloorGrating;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 0.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 4. 实心墙体 (Wall Solid)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Solid"));
		Mod.Mesh = WallSolid;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 2.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 5. 窗框墙体 (Wall Window)
	if (WallWindow)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Window"));
		Mod.Mesh = WallWindow;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 6. 门洞墙体 (Wall Doorway)
	if (WallDoorway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Doorway"));
		Mod.Mesh = WallDoorway;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 7. 外转角墙 (Corner Wall Outer)
	if (CornerOuter)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Corner_Outer"));
		Mod.Mesh = CornerOuter;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 8. 居中圆柱立柱 (Column Cylinder Center)
	if (ColumnCyl)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Column_Cylinder"));
		Mod.Mesh = ColumnCyl;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 0.8f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 9. 八角立柱 (Column Octagonal)
	if (ColumnOct)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Column_Octagonal"));
		Mod.Mesh = ColumnOct;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 0.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 10. 楼梯爬升主体 (Stairs Ascend) - 顶部不带天花板，垂直向上通往楼梯井
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Stairs_Ascend"));
		Mod.Mesh = StairsMesh;
		Mod.AdditionalParts.Add(FloorBasePart); // 楼梯脚平整地基
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Stair_Shaft"))); // 垂直导向上一层楼梯井
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 11. 楼梯井上层挑空 (Stair Wellhole) - 底部不带地板，让玩家可从下层顺畅走上
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Stair_Wellhole"));
		Mod.Mesh = nullptr; // 纯空气挑空
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart); // 挑空上方依然有本层天花封顶
		}
		Mod.Weight = 0.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Stair_Shaft"))); // 严密咬合下层楼梯顶部
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	ClearGenerated();
	UE_LOG(LogTemp, Log, TEXT("[WFC] Loaded 3-Story 4-Floor Preset with %d modules! GridSize=(8, 8, 3), CellSize=(200, 200, 200)"), ModuleLibrary->Modules.Num());
}

void AWFC3DGenerator::SetupMegaAtriumPreset()
{
	// 44 x 44 x 10 = 19,360 cells (体积达到原有 8x8x3=192 单元格的 100.8 倍！)
	// 空间物理跨度达到 88 米 x 88 米 x 20 米高，宛如真实北京鸟巢或现代化巨型商场中庭
	GridSize = FIntVector(44, 44, 10);
	CellSize = FVector(200.0f, 200.0f, 200.0f);
	bMegaAtriumMode = true;
	AtriumWalkwayDepth = 4;
	bOvalAtriumShape = true;
	bIncludeAir = true;
	AirWeight = 2.0f;
	bEnforceGroundFloor = false; // 由中庭系统统一规划中央广场与外环回廊地面
	bEnsureStairConnectivity = true;
	StairsPerFloorTransition = 2; // 环线每层保证 2 处观景楼梯相连
	bIncludeCeilings = true;
	MaxRetryAttempts = 50;

	if (!ModuleLibrary)
	{
		ModuleLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("MegaAtriumWFCLibrary"));
	}

	ModuleLibrary->Modules.Empty();
	ModuleLibrary->AirSocketName = FName(TEXT("Air"));
	ModuleLibrary->CustomCompatibilityPairs.Empty();

	// 建筑相容规则：
	// 1. Solid_Ground 与 Air 上下相容 (地面上方可为空气，空气下方可为地面)
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	// 2. Exterior 与 Air 相容
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Exterior")), FName(TEXT("Air")), true });
	// 3. Wall_Joint 与 Air 相容 (墙体在回廊挑空边缘自然收口为扶手/短墙断点)
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Air")), true });
	// 4. Room 与 Air 相容 (回廊与房间可直接面向巨大中庭挑空开敞观景，绝不被截断)
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Air")), true });
	// 5. 防御性相容对 (确保任意情况下插槽名为 Exterior 时仍完全连通)
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Exterior")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Exterior")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Exterior")), true });

	auto LoadMesh = [](const TCHAR* Path) -> UStaticMesh*
	{
		return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, Path));
	};

	UStaticMesh* FloorFlat = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Flat_01.SM_Floor_Flat_01"));
	UStaticMesh* FloorPathway = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Pathway_01.SM_Floor_Pathway_01"));
	UStaticMesh* FloorGrating = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Grating_01.SM_Floor_Grating_01"));

	UStaticMesh* WallSolid = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Solid_01.SM_Wall_Solid_01"));
	UStaticMesh* WallDoorway = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Doorway_01.SM_Wall_Doorway_01"));
	UStaticMesh* WallWindow = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Window_01.SM_Wall_Window_01"));

	UStaticMesh* CornerOuter = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Outer_01.SM_Corner_Wall_Outer_01"));
	UStaticMesh* CornerInner = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Inner_01.SM_Corner_Wall_Inner_01"));

	UStaticMesh* ColumnCyl = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Cylinder_Center_01.SM_Column_Cylinder_Center_01"));
	UStaticMesh* ColumnOct = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Octagonal_01.SM_Column_Octagonal_01"));

	UStaticMesh* CeilingFlat = LoadMesh(TEXT("/Game/FBX_Export/SM_Ceiling_Flat_01.SM_Ceiling_Flat_01"));
	UStaticMesh* StairsMesh = LoadMesh(TEXT("/Game/FBX_Export/SM_Stairs_Straight_01.SM_Stairs_Straight_01"));

	if (!FloorFlat || !WallSolid || !StairsMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] SetupMegaAtriumPreset failed: Missing required static mesh assets in /Game/FBX_Export/!"));
		return;
	}

	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = FloorFlat;
	FloorBasePart.RelativeTransform = FTransform::Identity;

	FWFC3DMeshPart CeilingPart;
	if (bIncludeCeilings && CeilingFlat)
	{
		CeilingPart.Mesh = CeilingFlat;
		CeilingPart.RelativeTransform = FTransform::Identity;
	}

	// 0. 中央开阔广场大理石地面 (Plaza Floor) - 向上彻底挑空无天花封顶
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Plaza_Floor"));
		Mod.Mesh = FloorFlat;
		Mod.Weight = 4.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air"))); // 向上直接开放通往开阔中庭
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 1. 标准开阔房间地面 (Floor Flat)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Flat"));
		Mod.Mesh = FloorFlat;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 3.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 2. 通道/漫游过道地面 (Floor Pathway)
	if (FloorPathway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Pathway"));
		Mod.Mesh = FloorPathway;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 2.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 3. 金属格栅地面 (Floor Grating)
	if (FloorGrating)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Grating"));
		Mod.Mesh = FloorGrating;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 0.8f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 4. 实心墙体 (Wall Solid)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Solid"));
		Mod.Mesh = WallSolid;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 5. 观景窗框墙体 (Wall Window)
	if (WallWindow)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Window"));
		Mod.Mesh = WallWindow;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.8f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 6. 连通门洞墙体 (Wall Doorway)
	if (WallDoorway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Doorway"));
		Mod.Mesh = WallDoorway;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 2.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 7. 外转角墙 (Corner Wall Outer)
	if (CornerOuter)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Corner_Outer"));
		Mod.Mesh = CornerOuter;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.2f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 8. 居中圆柱立柱 (Column Cylinder Center)
	if (ColumnCyl)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Column_Cylinder"));
		Mod.Mesh = ColumnCyl;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 1.2f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 9. 八角立柱 (Column Octagonal)
	if (ColumnOct)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Column_Octagonal"));
		Mod.Mesh = ColumnOct;
		Mod.AdditionalParts.Add(FloorBasePart);
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 0.8f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 10. 楼梯爬升主体 (Stairs Ascend)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Stairs_Ascend"));
		Mod.Mesh = StairsMesh;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Stair_Shaft")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		ModuleLibrary->Modules.Add(Mod);
	}

	// 11. 楼梯井上层挑空 (Stair Wellhole)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Stair_Wellhole"));
		Mod.Mesh = nullptr;
		if (bIncludeCeilings && CeilingFlat)
		{
			Mod.AdditionalParts.Add(CeilingPart);
		}
		Mod.Weight = 0.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Stair_Shaft")));
		Mod.bAutoGenerateRotations = false;
		ModuleLibrary->Modules.Add(Mod);
	}

	ClearGenerated();
	UE_LOG(LogTemp, Log, TEXT("[WFC] Loaded Mega Atrium Preset with %d modules! GridSize=(%d, %d, %d), CellSize=(200, 200, 200)"),
		ModuleLibrary->Modules.Num(), GridSize.X, GridSize.Y, GridSize.Z);
}

void AWFC3DGenerator::SetupSpiralGardenPreset()
{
	// =====================================================================
	// 螺旋花园高塔 - 单模块研究版 (Spiral Ribbon / Single-Mesh Study)
	// 设计意象：同一条大半径螺旋缎带，但全部由 WFC_Modules/Cube 一种网格构成。
	// 结构必需 6 种模块 (地面/栏杆/上行梯/梯顶/上行坡/坡顶) + 点缀 2 种 (拱门/花坛)，
	// 全部由同一种 Cube 拼装，让 "约束如何塑造形态" 一目了然。
	//
	// 拓扑 (由 Generate3DSpace 中的螺旋约束驱动)：
	// * 单元格尺寸自动读取 Cube 实际边长，保证立方体严丝合缝拼贴整个缎带
	// * 每层仅一段 49° 扇形缎带 (弧宽 = 25° 旋转 + 24° 重叠)，56 层盘旋约 3.9 圈
	// * 缎带宽 12 格 (24m)：内缘半径 9 格 (中央 36m 贯通天井)，外缘 21 格 (+耳台 5 格)
	// * 缎带内缘与外缘 (含随机外扩耳台) 全部为栏杆格
	// * 层间交错布设楼梯与坡道 (立方柱踏步)，落在相邻层缎带重叠区
	// * 缎带核心全部由约束直接铺设为地面格；缎带之外是贯通全塔的天空虚空
	// * 地面/栏杆/爬升段为全约束确定性输出；拱门与花坛由概率点缀 (见 Generate3DSpace 第 4 阶段)
	// =====================================================================
	GridSize = FIntVector(40, 40, 56);
	CellSize = FVector(200.0f, 200.0f, 200.0f);
	bSpiralGardenMode = true;
	bMegaAtriumMode = false;
	bIncludeAir = true;
	AirWeight = 1.0f;
	bEnforceGroundFloor = false;
	bEnsureStairConnectivity = false; // 螺旋爬升段由预设逻辑亲自布设
	bIncludeCeilings = false;         // 全程无天花板，向天空敞开
	MaxRetryAttempts = 20;
	SpiralTwistPerLayer = 25.0f;      // 56 层 x 25° ≈ 3.9 圈，每圈上升 28.8m (> 带宽，圈层次分明)
	SpiralArcOverlapDeg = 24.0f;
	SpiralRibbonInnerRadius = 9;
	SpiralRibbonOuterRadius = 21;
	SpiralRibbonWidenChance = 0.35f;

	if (!ModuleLibrary)
	{
		ModuleLibrary = NewObject<UWFC3DModuleLibrary>(this, TEXT("SpiralGardenWFCLibrary"));
	}

	ModuleLibrary->Modules.Empty();
	ModuleLibrary->AirSocketName = FName(TEXT("Air"));
	ModuleLibrary->bIncludeAirModule = true;
	ModuleLibrary->AirModuleWeight = 1.0f;
	ModuleLibrary->CustomCompatibilityPairs.Empty();

	// --- 相容规则 ---
	// 缎带是悬浮建筑：实心基座/房间均可与挑空空气相邻 (浮空花园的根基)
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	ModuleLibrary->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Air")), true });
	// 爬升段顶端仅能与梯顶/坡道台咬合 (同名插槽自动相容: Stair_Shaft / Ramp_Top)

	// 唯一网格：WFC_Modules 目录下的 Cube
	UStaticMesh* CubeMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/WFC_Modules/Cube.Cube")));
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] SetupSpiralGardenPreset failed: /Game/WFC_Modules/Cube not found!"));
		return;
	}

	// 自适应格子尺寸：直接读取 Cube 包围盒，让立方体严丝合缝
	float CubeEdge = 200.0f;
	{
		const FVector BoxExtent = CubeMesh->GetBounds().BoxExtent;
		const float MaxExtent = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
		if (MaxExtent > KINDA_SMALL_NUMBER)
		{
			CubeEdge = MaxExtent * 2.0f;
		}
	}
	CellSize = FVector(CubeEdge, CubeEdge, CubeEdge);
	// 保持巨构比例：目标总高约 112m (经典版 28m x 4)，层数随格子尺寸自适应
	const int32 AdaptiveLayers = FMath::Clamp(FMath::RoundToInt(11200.0f / CubeEdge), 8, 80);
	GridSize = FIntVector(40, 40, AdaptiveLayers);

	// 装饰形态 (拱门立柱 / 花坛围檐等) 的拼装偏移量按 200cm 格校准，再随实际格宽等比缩放
	// 这样即使 Cube 边长不是 200cm，拱门立柱也总能立在楼板顶面、花坛也总能坐正
	const float DecorUnit = CubeEdge / 200.0f;
	auto DO = [DecorUnit](float X, float Y, float Z)
	{
		return FVector(X * DecorUnit, Y * DecorUnit, Z * DecorUnit);
	};

	// 通用水平插槽助手：四向 Room / 上 Air (无天花) / 下 Solid_Ground
	auto SetGroundSockets = [](FWFC3DModuleDefinition& Mod)
	{
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
	};

	// 统一注册一个立方体模块 (地面/栏杆共用外观，仅靠插槽承担结构语义)
	auto AddCubeModule = [&](FName Id, FName PosZSocket, FName NegZSocket, float Weight)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = Id;
		Mod.Mesh = CubeMesh;
		Mod.Weight = Weight;
		SetGroundSockets(Mod);
		Mod.SetSocket(EWFC3DDirection::PosZ, PosZSocket);
		Mod.SetSocket(EWFC3DDirection::NegZ, NegZSocket);
		ModuleLibrary->Modules.Add(Mod);
	};

	// ============================ 1. 缎带地面 (可行走主体) ============================
	AddCubeModule(FName(TEXT("Floor_Flat")), FName(TEXT("Air")), FName(TEXT("Solid_Ground")), 1.0f);

	// ============================ 2. 栏杆格 (缎带边界) ============================
	// 单模块约束下与地面同外观 (都是 Cube)，但保留独立插槽语义，便于将来换装
	AddCubeModule(FName(TEXT("Railing_Window")), FName(TEXT("Air")), FName(TEXT("Solid_Ground")), 1.0f);

	// ============================ 3. 螺旋爬升段 (楼梯 + 坡道交错) ============================
	// 立方柱踏步：下行格 + 上端格 = 缎带上一对两格高的台阶/坡台
	AddCubeModule(FName(TEXT("Stair_Up")), FName(TEXT("Stair_Shaft")), FName(TEXT("Solid_Ground")), 1.0f);
	AddCubeModule(FName(TEXT("Stair_Well")), FName(TEXT("Air")), FName(TEXT("Stair_Shaft")), 1.0f);
	AddCubeModule(FName(TEXT("Ramp_Up")), FName(TEXT("Ramp_Top")), FName(TEXT("Solid_Ground")), 1.0f);
	AddCubeModule(FName(TEXT("Ramp_Well")), FName(TEXT("Air")), FName(TEXT("Ramp_Top")), 1.0f);

	// ============================ 7. 拱门 (门) ============================
	// 单一 Cube 拼装：全格楼板(主Mesh，保证动线连续) + 左右立柱 + 顶部横梁 + 顶石。
	// 中部门洞完全敞开，玩家沿切线方向径直穿过，绝不阻塞动线。
	// 开启四向旋转：Rot0 门洞沿 X 向 / Rot90 门洞沿 Y 向，由 Generate3DSpace 按切线朝向自动选向。
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Gate_Arch"));
		Mod.Mesh = CubeMesh;
		Mod.Weight = 1.0f;
		SetGroundSockets(Mod); // Room x4 / 上 Air / 下 Solid_Ground，与地面完全相容
		Mod.bAutoGenerateRotations = true;

		// 左立柱 (28x28x170cm，立于楼板顶面 z=+100 之上)
		FWFC3DMeshPart PillarA;
		PillarA.Mesh = CubeMesh;
		PillarA.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, -38.0f, 185.0f), FVector(0.14f, 0.14f, 0.85f));
		Mod.AdditionalParts.Add(PillarA);

		// 右立柱
		FWFC3DMeshPart PillarB;
		PillarB.Mesh = CubeMesh;
		PillarB.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 38.0f, 185.0f), FVector(0.14f, 0.14f, 0.85f));
		Mod.AdditionalParts.Add(PillarB);

		// 顶部横梁 (36x172x28cm，两端压住立柱顶端)
		FWFC3DMeshPart Lintel;
		Lintel.Mesh = CubeMesh;
		Lintel.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 0.0f, 276.0f), FVector(0.18f, 0.86f, 0.14f));
		Mod.AdditionalParts.Add(Lintel);

		// 顶石 (44x44x36cm，收口于横梁之上)
		FWFC3DMeshPart Capstone;
		Capstone.Mesh = CubeMesh;
		Capstone.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 0.0f, 308.0f), FVector(0.22f, 0.22f, 0.18f));
		Mod.AdditionalParts.Add(Capstone);

		ModuleLibrary->Modules.Add(Mod);
	}

	// ============================ 8. 花坛 ============================
	// 单一 Cube 拼装：全格楼板(主Mesh) + 抬升土床 + 四边围檐 + 3 簇花丛。
	// 坐落在路面上约 71cm 高，不遮挡动线；落在缎带边缘时自然形成临栏绿带。
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Flower_Bed"));
		Mod.Mesh = CubeMesh;
		Mod.Weight = 1.0f;
		SetGroundSockets(Mod);
		Mod.bAutoGenerateRotations = false;

		// 抬升土床 (124x124x52cm)
		FWFC3DMeshPart Soil;
		Soil.Mesh = CubeMesh;
		Soil.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 0.0f, 113.0f), FVector(0.62f, 0.62f, 0.26f));
		Mod.AdditionalParts.Add(Soil);

		// 围檐长边 (132x14x28cm)
		FWFC3DMeshPart RimA;
		RimA.Mesh = CubeMesh;
		RimA.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, -33.0f, 147.0f), FVector(0.66f, 0.07f, 0.14f));
		Mod.AdditionalParts.Add(RimA);

		FWFC3DMeshPart RimB;
		RimB.Mesh = CubeMesh;
		RimB.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 33.0f, 147.0f), FVector(0.66f, 0.07f, 0.14f));
		Mod.AdditionalParts.Add(RimB);

		// 围檐短边
		FWFC3DMeshPart RimC;
		RimC.Mesh = CubeMesh;
		RimC.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(-33.0f, 0.0f, 147.0f), FVector(0.07f, 0.66f, 0.14f));
		Mod.AdditionalParts.Add(RimC);

		FWFC3DMeshPart RimD;
		RimD.Mesh = CubeMesh;
		RimD.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(33.0f, 0.0f, 147.0f), FVector(0.07f, 0.66f, 0.14f));
		Mod.AdditionalParts.Add(RimD);

		// 花丛 x3 (22x22x32cm 小方簇，探出围檐)
		FWFC3DMeshPart BloomA;
		BloomA.Mesh = CubeMesh;
		BloomA.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(-16.0f, 12.0f, 163.0f), FVector(0.11f, 0.11f, 0.16f));
		Mod.AdditionalParts.Add(BloomA);

		FWFC3DMeshPart BloomB;
		BloomB.Mesh = CubeMesh;
		BloomB.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(14.0f, -14.0f, 161.0f), FVector(0.11f, 0.11f, 0.16f));
		Mod.AdditionalParts.Add(BloomB);

		FWFC3DMeshPart BloomC;
		BloomC.Mesh = CubeMesh;
		BloomC.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(6.0f, 20.0f, 166.0f), FVector(0.11f, 0.11f, 0.16f));
		Mod.AdditionalParts.Add(BloomC);

		ModuleLibrary->Modules.Add(Mod);
	}

	ClearGenerated();
	UE_LOG(LogTemp, Log, TEXT("[WFC] Loaded Spiral Ribbon (single Cube) Preset with %d modules! GridSize=(40, 40, %d), CubeEdge=%.1fcm, Ribbon R=9..21(+ear 5)"),
		ModuleLibrary->Modules.Num(), AdaptiveLayers, CubeEdge);
}
void AWFC3DGenerator::Generate3DSpace()
{
	bLastGenerationSucceeded = false;
	ClearGenerated();

	if (!PrepareLibrary())
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Generator] Generate3DSpace aborted: library preparation failed."));
		return;
	}

	// 查找地面板与爬升段模块索引 (螺旋花园缎带：地面/栏杆/楼梯/坡道)
	int32 PlazaFloorModuleIndex = INDEX_NONE;
	int32 FloorModuleIndex = INDEX_NONE;
	int32 StairModuleIndex = INDEX_NONE;
	int32 WellholeModuleIndex = INDEX_NONE;
	int32 AirModuleIndex = INDEX_NONE;
	int32 RailingModuleIndex = INDEX_NONE; // Railing_Window (螺旋缎带栏杆)
	int32 RampModuleIndex = INDEX_NONE;    // Ramp_Ascend (螺旋坡道)
	int32 LandingModuleIndex = INDEX_NONE; // Ramp_Landing (坡道顶端平台)
	int32 GateVariantRot0 = INDEX_NONE;    // Gate_Arch 0° (门洞沿 X 向)
	int32 GateVariantRot90 = INDEX_NONE;   // Gate_Arch 90° (门洞沿 Y 向)
	int32 PlanterModuleIndex = INDEX_NONE; // Flower_Bed (花坛)

	for (int32 ModIdx = 0; ModIdx < CompiledModules.Num(); ++ModIdx)
	{
		const FName SrcId = CompiledModules[ModIdx].SourceModuleId;
		if (SrcId == FName(TEXT("Plaza_Floor")))
		{
			PlazaFloorModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Floor_Flat")) || SrcId == FName(TEXT("Floor")))
		{
			if (FloorModuleIndex == INDEX_NONE) FloorModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Stairs_Ascend")) || SrcId == FName(TEXT("Stair_Up")))
		{
			if (StairModuleIndex == INDEX_NONE) StairModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Stair_Wellhole")) || SrcId == FName(TEXT("Stair_Well")))
		{
			if (WellholeModuleIndex == INDEX_NONE) WellholeModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Air")))
		{
			if (AirModuleIndex == INDEX_NONE) AirModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Railing_Window")))
		{
			if (RailingModuleIndex == INDEX_NONE) RailingModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Ramp_Ascend")) || SrcId == FName(TEXT("Ramp_Up")))
		{
			if (RampModuleIndex == INDEX_NONE) RampModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Ramp_Landing")) || SrcId == FName(TEXT("Ramp_Well")))
		{
			if (LandingModuleIndex == INDEX_NONE) LandingModuleIndex = ModIdx;
		}
		else if (SrcId == FName(TEXT("Gate_Arch")))
		{
			// 四向旋转变体：仅取 Rot0 (门洞沿 X) 与 Rot90 (门洞沿 Y)，供按行走进切线定朝向
			const FString VariantIdStr = CompiledModules[ModIdx].VariantId.ToString();
			if (VariantIdStr.EndsWith(TEXT("_Rot90")))
			{
				GateVariantRot90 = ModIdx;
			}
			else if (VariantIdStr.EndsWith(TEXT("_Rot0")))
			{
				GateVariantRot0 = ModIdx;
			}
		}
		else if (SrcId == FName(TEXT("Flower_Bed")))
		{
			if (PlanterModuleIndex == INDEX_NONE) PlanterModuleIndex = ModIdx;
		}
	}

	if (PlazaFloorModuleIndex == INDEX_NONE)
	{
		PlazaFloorModuleIndex = FloorModuleIndex;
	}

	const int32 EffectiveSeed = (RandomSeed != 0) ? RandomSeed : FMath::Rand();

	// 设置初始约束
	TMap<int32, int32> InitialClamps;

	// =====================================================================
	// 螺旋花园缎带约束 (Spiral Ribbon Constraints) - 巨构尺度
	// 剖面： [天空虚空 | 缎带(内栏杆|走道|外栏杆) | 天空虚空]
	//       每层仅一段扇形缎带，逐层旋转 25° 盘旋 3.9 圈；缎带之外是贯通全塔的虚空，
	//       中心是直上直下的 36m 宽天井 —— 是一条螺旋，而不是一个桶。
	// 形态分层：地面/栏杆/爬升段全约束钉死 (确定性)；拱门(门)与花坛按概率点缀 (第 4 阶段)。
	// =====================================================================
	if (bSpiralGardenMode && FloorModuleIndex != INDEX_NONE && AirModuleIndex != INDEX_NONE && RailingModuleIndex != INDEX_NONE && StairModuleIndex != INDEX_NONE)
	{
		const int32 GX = GridSize.X;
		const int32 GY = GridSize.Y;
		const int32 GZ = GridSize.Z;
		const float CX = (GX - 1) * 0.5f;
		const float CY = (GY - 1) * 0.5f;

		const float RIn = static_cast<float>(SpiralRibbonInnerRadius);
		const float ROut = static_cast<float>(SpiralRibbonOuterRadius);
		const float ArcWidthDeg = SpiralTwistPerLayer + SpiralArcOverlapDeg; // 缎带弧宽 = 旋转 + 重叠
		const float WidenExtent = 5.0f;  // 耳台外扩格数 (24m 宽缎带上的 10m 深悬浮挑台)
		const float CutDeg = 4.0f;       // 径向端头栏杆角宽 (约 1 格)
		const float EdgeBandDeg = 4.0f;  // 每层起止角扰动幅度 (小随机，避免扇形过于刻板)

		// 每层缎带布局：起始角 + 是否外扩耳台 (扰动刻意取小值，保证层间重叠区不被侵蚀)
		struct FRibbonLayer
		{
			float StartDeg = 0.0f;
			float CenterDeg = 0.0f;
			float OuterThis = 0.0f;
		};
		FRandomStream LayoutRnd(EffectiveSeed);
		TArray<FRibbonLayer> RibbonLayers;
		RibbonLayers.SetNum(GZ);
		for (int32 Z = 0; Z < GZ; ++Z)
		{
			RibbonLayers[Z].StartDeg = Z * SpiralTwistPerLayer + LayoutRnd.FRandRange(-EdgeBandDeg, EdgeBandDeg);
			RibbonLayers[Z].CenterDeg = RibbonLayers[Z].StartDeg + ArcWidthDeg * 0.5f;
			RibbonLayers[Z].OuterThis = (LayoutRnd.FRand() < SpiralRibbonWidenChance) ? (ROut + WidenExtent) : ROut;
		}

		auto InArcWindow = [](float AngleDeg, float CenterDeg, float WidthDeg) -> bool
		{
			return FMath::Abs(FMath::FindDeltaAngleDegrees(AngleDeg, CenterDeg)) <= WidthDeg * 0.5f;
		};

		// 判断某格是否为该层缎带的 "可行走核心格" (非栏杆、非虚空)
		auto IsCoreCell = [&](int32 X, int32 Y, const FRibbonLayer& L) -> bool
		{
			const float Dx = X - CX;
			const float Dy = Y - CY;
			const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
			if (R < RIn || R > L.OuterThis)
			{
				return false;
			}
			const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
			if (!InArcWindow(Ang, L.CenterDeg, ArcWidthDeg))
			{
				return false;
			}
			if (R <= RIn + 0.7f || R >= L.OuterThis - 0.7f)
			{
				return false; // 内外缘栏杆带
			}
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg)) <= CutDeg ||
				FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg + ArcWidthDeg)) <= CutDeg)
			{
				return false; // 径向端头栏杆带
			}
			return true;
		};

		auto SetClamp = [&](int32 CellIdx, int32 ModIdx)
		{
			if (ModIdx == INDEX_NONE)
			{
				return;
			}
			if (InitialClamps.Contains(CellIdx))
			{
				InitialClamps[CellIdx] = ModIdx;
			}
			else
			{
				InitialClamps.Add(CellIdx, ModIdx);
			}
		};

		int32 ClampedVoid = 0, ClampedRailing = 0, ClampedWiden = 0, ClampedStair = 0, ClampedRamp = 0;

		// ---------- 1) 虚空：缎带之外的一切 (中央天井 + 塔外天空 + 同层非缎带扇区) ----------
		for (int32 Z = 0; Z < GZ; ++Z)
		{
			const FRibbonLayer& L = RibbonLayers[Z];
			for (int32 X = 0; X < GX; ++X)
			{
				for (int32 Y = 0; Y < GY; ++Y)
				{
					const float Dx = X - CX;
					const float Dy = Y - CY;
					const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
					bool bVoid = (R < RIn) || (R > L.OuterThis); // 天井内 / 缎带外
					if (!bVoid)
					{
						const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
						if (!InArcWindow(Ang, L.CenterDeg, ArcWidthDeg))
						{
							bVoid = true; // 同层非缎带扇区
						}
					}
					if (bVoid)
					{
						SetClamp(Solver.CoordToIndex(X, Y, Z), AirModuleIndex);
						++ClampedVoid;
					}
				}
			}
		}

		// ---------- 2) 缎带栏杆：内缘 / 外缘(含耳台外缘) / 径向端头 ----------
		for (int32 Z = 0; Z < GZ; ++Z)
		{
			const FRibbonLayer& L = RibbonLayers[Z];
			for (int32 X = 0; X < GX; ++X)
			{
				for (int32 Y = 0; Y < GY; ++Y)
				{
					const float Dx = X - CX;
					const float Dy = Y - CY;
					const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
					if (R < RIn || R > L.OuterThis)
					{
						continue;
					}
					const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
					if (!InArcWindow(Ang, L.CenterDeg, ArcWidthDeg))
					{
						continue;
					}

					bool bRailing = false;
					if (R <= RIn + 0.7f)
					{
						bRailing = true; // 内缘栏杆 (俯瞰中央天井)
					}
					else if (R >= L.OuterThis - 0.7f)
					{
						bRailing = true; // 外缘栏杆 (含耳台外缘)
					}
					else if (FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg)) <= CutDeg ||
						FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg + ArcWidthDeg)) <= CutDeg)
					{
						bRailing = true; // 径向端头栏杆 (缎带两端的收口)
					}

					if (bRailing)
					{
						SetClamp(Solver.CoordToIndex(X, Y, Z), RailingModuleIndex);
						++ClampedRailing;
						if (R > ROut)
						{
							++ClampedWiden; // 耳台新增栏杆段
						}
					}
				}
			}
		}

		// ---------- 2.5) 缎带核心：全部由约束直接铺设为地面 ----------
		// 单模块研究的取舍：不求随机坍缩的花样，只让 "约束拓扑" 本身发声 ——
		// 缎带每一格都由约束决定，求解器退化为纯校验 + 确定性铺块 (永不出现缺口)。
		for (int32 Z = 0; Z < GZ; ++Z)
		{
			const FRibbonLayer& L = RibbonLayers[Z];
			for (int32 X = 0; X < GX; ++X)
			{
				for (int32 Y = 0; Y < GY; ++Y)
				{
					if (!IsCoreCell(X, Y, L))
					{
						continue;
					}
					SetClamp(Solver.CoordToIndex(X, Y, Z), FloorModuleIndex);
				}
			}
		}

		// ---------- 3) 螺旋爬升段：交错楼梯/坡道（立方柱踏步）落在层间重叠区 ----------
		// 落点严格限定为 "本层与下层缎带核心的交集格"，无论扰动如何绝不会落空
		struct FClimberSpot
		{
			FIntPoint Pos = FIntPoint::ZeroValue;
			int32 LowerZ = 0;
			int32 LowerModIdx = INDEX_NONE;
			int32 UpperModIdx = INDEX_NONE;
		};
		TArray<FClimberSpot> ClimberSpots;
		ClimberSpots.Reserve(GZ - 1);

		for (int32 Z = 0; Z < GZ - 1; ++Z)
		{
			const FRibbonLayer& Lz = RibbonLayers[Z];
			const FRibbonLayer& Lz1 = RibbonLayers[Z + 1];
			// 层间重叠区中心 (本层弧尾 与 下层弧头 的中间)
			const float OverlapCenter = (Lz1.StartDeg + (Lz.StartDeg + ArcWidthDeg)) * 0.5f;

			FIntPoint BestPos(-1, -1);
			float BestDev = MAX_FLT;
			for (int32 X = 0; X < GX; ++X)
			{
				for (int32 Y = 0; Y < GY; ++Y)
				{
					if (!IsCoreCell(X, Y, Lz) || !IsCoreCell(X, Y, Lz1))
					{
						continue; // 必须同时在本层与下层的可走缎带上
					}
					if (ClimberSpots.Num() > 0 && FIntPoint(X, Y) == ClimberSpots.Last().Pos)
					{
						continue; // 跳过上层落点，避免覆盖上层爬升段
					}
					const float Dx = X - CX;
					const float Dy = Y - CY;
					const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
					const float Dev = FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, OverlapCenter));
					if (Dev < BestDev)
					{
						BestDev = Dev;
						BestPos = FIntPoint(X, Y);
					}
				}
			}
			if (BestPos.X < 0)
			{
				continue;
			}

			FClimberSpot Spot;
			Spot.Pos = BestPos;
			Spot.LowerZ = Z;

			// 楼梯与坡道交错出现，让动线节奏多变
			const bool bCanRamp = (RampModuleIndex != INDEX_NONE && LandingModuleIndex != INDEX_NONE);
			const bool bCanStair = (StairModuleIndex != INDEX_NONE && WellholeModuleIndex != INDEX_NONE);
			if (bCanStair && (!bCanRamp || LayoutRnd.FRand() < 0.55f))
			{
				Spot.LowerModIdx = StairModuleIndex;
				Spot.UpperModIdx = WellholeModuleIndex;
			}
			else if (bCanRamp)
			{
				Spot.LowerModIdx = RampModuleIndex;
				Spot.UpperModIdx = LandingModuleIndex;
			}

			ClimberSpots.Add(Spot);

			// 直接铺块：缎带核心已被约束为地面，这里只把重叠区中心格换成爬升段。
			// (梯顶/坡道台为实心立方，与梯身合成两格高的 "踏步柱"，形态干净利落)
			if (Spot.LowerModIdx == INDEX_NONE || Spot.UpperModIdx == INDEX_NONE)
			{
				continue;
			}
			SetClamp(Solver.CoordToIndex(Spot.Pos.X, Spot.Pos.Y, Spot.LowerZ), Spot.LowerModIdx);
			SetClamp(Solver.CoordToIndex(Spot.Pos.X, Spot.Pos.Y, Spot.LowerZ + 1), Spot.UpperModIdx);
			if (Spot.LowerModIdx == StairModuleIndex)
			{
				++ClampedStair;
			}
			else
			{
				++ClampedRamp;
			}
		}

		// ---------- 4) 缎带点缀：概率化生成 "拱门(门)" 与 "花坛" (单一 Cube 拼装形态) ----------
		// 合逻辑原则：
		// * 只落在普通核心地面格上 (自动避开虚空/栏杆/楼梯/坡道)
		// * 同种形态间保持最小间距 (切比雪夫)，绝不出现门挨门、坛贴坛
		// * 拱门中部敞开、花坛仅约 71cm 高，任何情况下都不阻塞动线
		// * 拱门朝向按该格行走进切线自动吸附：Rot0 门洞沿 X / Rot90 门洞沿 Y
		// * 花坛优先落在缎带边缘格 (紧邻栏杆/虚空的格)，形成临栏绿带
		int32 DecorArchwayCount = 0;
		int32 DecorPlanterCount = 0;

		const bool bCanArchway = bGenerateSpiralArchways && (GateVariantRot0 != INDEX_NONE || GateVariantRot90 != INDEX_NONE);
		const bool bCanPlanter = bGenerateSpiralPlanters && (PlanterModuleIndex != INDEX_NONE);

		if (bCanArchway || bCanPlanter)
		{
			// 独立装饰随机流 (相对布局流固定偏移，保证同一种子结果可复现)
			FRandomStream DecorRnd(EffectiveSeed + 9091);

			// 查询某格当前已被钉死的模块 (越界或未钉死返回 INDEX_NONE)
			auto GetClampedModule = [&](int32 X, int32 Y, int32 Z) -> int32
			{
				if (X < 0 || X >= GX || Y < 0 || Y >= GY || Z < 0 || Z >= GZ)
				{
					return INDEX_NONE;
				}
				const int32* Found = InitialClamps.Find(Solver.CoordToIndex(X, Y, Z));
				return Found ? *Found : INDEX_NONE;
			};

			TArray<FIntVector> PlacedArchways;
			TArray<FIntVector> PlacedPlanters;

			for (int32 Z = 0; Z < GZ; ++Z)
			{
				for (int32 X = 0; X < GX; ++X)
				{
					for (int32 Y = 0; Y < GY; ++Y)
					{
						// 只允许替换普通缎带地面格 (已被爬升段钉死的格不参与点缀)
						if (GetClampedModule(X, Y, Z) != FloorModuleIndex)
						{
							continue;
						}

						// --- 拱门 (门): 单格概率判定 + 最小间距约束 ---
						if (bCanArchway && DecorRnd.FRand() < SpiralArchwayChance)
						{
							bool bTooCloseToArchway = false;
							for (const FIntVector& Placed : PlacedArchways)
							{
								if (FMath::Max3(
										FMath::Abs(Placed.X - X),
										FMath::Abs(Placed.Y - Y),
										FMath::Abs(Placed.Z - Z)) < SpiralArchwayMinSpacing)
								{
									bTooCloseToArchway = true;
									break;
								}
							}

							if (!bTooCloseToArchway)
							{
								// 行走进切线 T = (-Dy, Dx) (半径方向逆时针旋转 90°)
								// 切线更接近 X 轴 → 门洞应沿 X 向 → 取 Rot0；反之取 Rot90
								const float Dx = X - CX;
								const float Dy = Y - CY;
								int32 ArchwayModule = INDEX_NONE;
								if (FMath::Abs(Dy) >= FMath::Abs(Dx))
								{
									ArchwayModule = (GateVariantRot0 != INDEX_NONE) ? GateVariantRot0 : GateVariantRot90;
								}
								else
								{
									ArchwayModule = (GateVariantRot90 != INDEX_NONE) ? GateVariantRot90 : GateVariantRot0;
								}

								if (ArchwayModule != INDEX_NONE)
								{
									SetClamp(Solver.CoordToIndex(X, Y, Z), ArchwayModule);
									PlacedArchways.Add(FIntVector(X, Y, Z));
									++DecorArchwayCount;
									continue; // 一门一花坛不共格
								}
							}
						}

						// --- 花坛: 边缘高概率 / 中央低概率 + 最小间距约束 ---
						if (bCanPlanter)
						{
							// 边缘格 = 四个水平邻格中存在非核心地面格 (紧贴栏杆/虚空/爬升段/网格外)
							const bool bAtRibbonEdge =
								GetClampedModule(X + 1, Y, Z) != FloorModuleIndex ||
								GetClampedModule(X - 1, Y, Z) != FloorModuleIndex ||
								GetClampedModule(X, Y + 1, Z) != FloorModuleIndex ||
								GetClampedModule(X, Y - 1, Z) != FloorModuleIndex;

							const float PlanterChance = bAtRibbonEdge ? SpiralPlanterEdgeChance : SpiralPlanterChance;
							if (DecorRnd.FRand() < PlanterChance)
							{
								bool bTooCloseToPlanter = false;
								for (const FIntVector& Placed : PlacedPlanters)
								{
									if (FMath::Max3(
											FMath::Abs(Placed.X - X),
											FMath::Abs(Placed.Y - Y),
											FMath::Abs(Placed.Z - Z)) < SpiralPlanterMinSpacing)
									{
										bTooCloseToPlanter = true;
										break;
									}
								}

								if (!bTooCloseToPlanter)
								{
									SetClamp(Solver.CoordToIndex(X, Y, Z), PlanterModuleIndex);
									PlacedPlanters.Add(FIntVector(X, Y, Z));
									++DecorPlanterCount;
								}
							}
						}
					}
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[WFC SpiralGarden] Ribbon constraints ready: Void=%d, Railing=%d (incl. widen %d), FloorCore=auto, StairStack=%d, RampStack=%d, Archway=%d, Planter=%d, TotalClamps=%d"),
			ClampedVoid, ClampedRailing, ClampedWiden, ClampedStair, ClampedRamp, DecorArchwayCount, DecorPlanterCount, InitialClamps.Num());
	}
	else if (bMegaAtriumMode && FloorModuleIndex != INDEX_NONE && AirModuleIndex != INDEX_NONE)
	{
		const float CenterX = (GridSize.X - 1) * 0.5f;
		const float CenterY = (GridSize.Y - 1) * 0.5f;
		const float RadiusX = FMath::Max(1.0f, CenterX - AtriumWalkwayDepth);
		const float RadiusY = FMath::Max(1.0f, CenterY - AtriumWalkwayDepth);

		TArray<FIntPoint> WalkwayRingCells;

		for (int32 X = 0; X < GridSize.X; ++X)
		{
			for (int32 Y = 0; Y < GridSize.Y; ++Y)
			{
				bool bInAtrium = false;
				float DistSq = 0.0f;
				if (bOvalAtriumShape)
				{
					const float Dx = (X - CenterX) / RadiusX;
					const float Dy = (Y - CenterY) / RadiusY;
					DistSq = Dx * Dx + Dy * Dy;
					bInAtrium = (DistSq < 1.0f);
				}
				else
				{
					bInAtrium = (X >= AtriumWalkwayDepth && X < GridSize.X - AtriumWalkwayDepth &&
					             Y >= AtriumWalkwayDepth && Y < GridSize.Y - AtriumWalkwayDepth);
				}

				if (bInAtrium)
				{
					// 中庭贯通区域：底层 (Z=0) 为中央广场地面 (无天花遮挡)，上层 (Z>0) 全部挑空开敞 (Air)
					for (int32 Z = 0; Z < GridSize.Z; ++Z)
					{
						const int32 CellIdx = Solver.CoordToIndex(X, Y, Z);
						if (Z == 0)
						{
							InitialClamps.Add(CellIdx, PlazaFloorModuleIndex);
						}
						else
						{
							InitialClamps.Add(CellIdx, AirModuleIndex);
						}
					}
				}
				else
				{
					// 环形漫游回廊基底：底层 (Z=0) 保证坚实地面
					const int32 GroundIdx = Solver.CoordToIndex(X, Y, 0);
					InitialClamps.Add(GroundIdx, FloorModuleIndex);

					// 收集回廊走廊核心区 (靠近中庭边缘但又非最外死角)，用于布置观景楼梯
					if (X >= 1 && X < GridSize.X - 1 && Y >= 1 && Y < GridSize.Y - 1)
					{
						if (bOvalAtriumShape)
						{
							if (DistSq >= 1.0f && DistSq <= 1.45f)
							{
								WalkwayRingCells.Add(FIntPoint(X, Y));
							}
						}
						else
						{
							WalkwayRingCells.Add(FIntPoint(X, Y));
						}
					}
				}
			}
		}

		// 楼梯强连通保证：在回廊漫游区沿环线排布错落楼梯与挑空天井
		if (bEnsureStairConnectivity && GridSize.Z >= 2 && StairModuleIndex != INDEX_NONE && WellholeModuleIndex != INDEX_NONE && WalkwayRingCells.Num() > 0)
		{
			FRandomStream Rnd(EffectiveSeed);
			TSet<FIntPoint> UsedStairPositions;

			for (int32 FloorZ = 0; FloorZ < GridSize.Z - 1; ++FloorZ)
			{
				const int32 NumStairs = FMath::Clamp(StairsPerFloorTransition, 1, 4);
				for (int32 StairIdx = 0; StairIdx < NumStairs; ++StairIdx)
				{
					int32 BestPickIdx = INDEX_NONE;
					const int32 BasePick = (StairIdx * (WalkwayRingCells.Num() / (NumStairs + 1)) + FloorZ * 11 + Rnd.RandRange(0, 5)) % WalkwayRingCells.Num();

					for (int32 Attempt = 0; Attempt < WalkwayRingCells.Num(); ++Attempt)
					{
						const int32 CandidateIdx = (BasePick + Attempt) % WalkwayRingCells.Num();
						if (!UsedStairPositions.Contains(WalkwayRingCells[CandidateIdx]))
						{
							BestPickIdx = CandidateIdx;
							break;
						}
					}

					if (BestPickIdx == INDEX_NONE)
					{
						BestPickIdx = BasePick;
					}

					const FIntPoint StairPos = WalkwayRingCells[BestPickIdx];
					UsedStairPositions.Add(StairPos);

					const int32 LowerStairCell = Solver.CoordToIndex(StairPos.X, StairPos.Y, FloorZ);
					const int32 UpperWellholeCell = Solver.CoordToIndex(StairPos.X, StairPos.Y, FloorZ + 1);

					InitialClamps.Add(LowerStairCell, StairModuleIndex);
					InitialClamps.Add(UpperWellholeCell, WellholeModuleIndex);

					UE_LOG(LogTemp, Log, TEXT("[WFC Atrium] Floor %d -> %d: Stair at (%d, %d, %d), Wellhole at (%d, %d, %d)"),
						FloorZ, FloorZ + 1, StairPos.X, StairPos.Y, FloorZ, StairPos.X, StairPos.Y, FloorZ + 1);
				}
			}
		}
	}
	else
	{
		// 1. 强制底层地面 (如果开启)
		if (bEnforceGroundFloor && FloorModuleIndex != INDEX_NONE)
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

		// 2. 楼梯强连通保证：确保层与层之间 (FloorZ -> FloorZ+1) 必有至少一处楼梯相连
		if (bEnsureStairConnectivity && GridSize.Z >= 2 && StairModuleIndex != INDEX_NONE && WellholeModuleIndex != INDEX_NONE)
		{
			const int32 SafeMinX = FMath::Clamp(2, 0, GridSize.X - 1);
			const int32 SafeMaxX = FMath::Clamp(GridSize.X - 3, SafeMinX, GridSize.X - 1);
			const int32 SafeMinY = FMath::Clamp(2, 0, GridSize.Y - 1);
			const int32 SafeMaxY = FMath::Clamp(GridSize.Y - 3, SafeMinY, GridSize.Y - 1);

			FRandomStream Rnd(EffectiveSeed);

			for (int32 FloorZ = 0; FloorZ < GridSize.Z - 1; ++FloorZ)
			{
				for (int32 StairCount = 0; StairCount < StairsPerFloorTransition; ++StairCount)
				{
					int32 PickX = 0;
					int32 PickY = 0;

					// 巧妙交错不同层级的楼梯位置：0->1 层偏向一侧，1->2 层偏向对角侧，极具探索感
					if (FloorZ % 2 == 0)
					{
						PickX = Rnd.RandRange(SafeMinX, (SafeMinX + SafeMaxX) / 2);
						PickY = Rnd.RandRange(SafeMinY, (SafeMinY + SafeMaxY) / 2);
					}
					else
					{
						PickX = Rnd.RandRange((SafeMinX + SafeMaxX) / 2, SafeMaxX);
						PickY = Rnd.RandRange((SafeMinY + SafeMaxY) / 2, SafeMaxY);
					}

					const int32 LowerStairCell = Solver.CoordToIndex(PickX, PickY, FloorZ);
					const int32 UpperWellholeCell = Solver.CoordToIndex(PickX, PickY, FloorZ + 1);

					InitialClamps.Add(LowerStairCell, StairModuleIndex);
					InitialClamps.Add(UpperWellholeCell, WellholeModuleIndex);

					UE_LOG(LogTemp, Log, TEXT("[WFC] Floor %d -> %d: Guaranteed staircase clamped at (%d, %d, %d), Wellhole at (%d, %d, %d)"),
						FloorZ, FloorZ + 1, PickX, PickY, FloorZ, PickX, PickY, FloorZ + 1);
				}
			}
		}
	}

	const double StartTime = FPlatformTime::Seconds();
	const bool bSuccess = Solver.Solve(MaxRetryAttempts, EffectiveSeed, InitialClamps);
	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	if (bSuccess)
	{
		bLastGenerationSucceeded = true;
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
