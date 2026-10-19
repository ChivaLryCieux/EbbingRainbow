// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_Style0.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"

AWFC3DArenaGenerator_Style0::AWFC3DArenaGenerator_Style0()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_Style0::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	if (bBasicRoomLayout)
	{
		return BuildBasicRoomModules(OutModules, OutCompatibility);
	}

	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->CustomCompatibilityPairs.Empty();
	Library->bIncludeAirModule = true;
	Library->AirModuleWeight = 1.0f;       // 保持竞技场开阔，但留出少量负空间
	Library->AirSocketName = FName(TEXT("Air"));

	// Style 0 的模型放在 /Game/Style/0/Meshes；迁移期间允许回退到旧的 FBX_Export 目录。
	auto LoadMesh = [](const TCHAR* MeshName) -> UStaticMesh*
	{
		const FString StylePath = FString::Printf(
			TEXT("/Game/Style/0/Meshes/%s.%s"), MeshName, MeshName);
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *StylePath)))
		{
			return Mesh;
		}

		const FString LegacyPath = FString::Printf(
			TEXT("/Game/FBX_Export/%s.%s"), MeshName, MeshName);
		return Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *LegacyPath));
	};

	UStaticMesh* FloorFlat    = LoadMesh(TEXT("SM_Floor_Flat_01"));
	UStaticMesh* FloorPathway = LoadMesh(TEXT("SM_Floor_Pathway_01"));
	UStaticMesh* FloorGrating = LoadMesh(TEXT("SM_Floor_Grating_01"));

	UStaticMesh* WallSolid    = LoadMesh(TEXT("SM_Wall_Solid_01"));
	UStaticMesh* WallDoorway  = LoadMesh(TEXT("SM_Wall_Doorway_01"));
	UStaticMesh* WallWindow   = LoadMesh(TEXT("SM_Wall_Window_01"));

	UStaticMesh* CornerOuter  = LoadMesh(TEXT("SM_Corner_Wall_Outer_01"));

	UStaticMesh* ColumnCyl    = LoadMesh(TEXT("SM_Column_Cylinder_Center_01"));
	UStaticMesh* ColumnOct    = LoadMesh(TEXT("SM_Column_Octagonal_01"));

	if (!FloorFlat || !WallSolid)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Style0] Missing required meshes in /Game/Style/0/Meshes (Floor_Flat / Wall_Solid)."));
		return false;
	}

	// 所有墙体/立柱共享同一块平整地板，保证没有缝隙
	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = FloorFlat;
	FloorBasePart.RelativeTransform = FTransform::Identity;

	// 1) 标准开阔房间地面
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Flat"));
		Mod.Mesh = FloorFlat;
		Mod.Weight = 6.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	// 2) 通道/过道地面
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
		Library->Modules.Add(Mod);
	}

	// 3) 金属格栅地面
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
		Library->Modules.Add(Mod);
	}

	// 4) 实心墙体
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
		Library->Modules.Add(Mod);
	}

	// 5) 窗框墙体
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
		Library->Modules.Add(Mod);
	}

	// 6) 门洞墙体
	if (WallDoorway)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Doorway"));
		Mod.Mesh = WallDoorway;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.2f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 7) 外转角墙
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
		Library->Modules.Add(Mod);
	}

	// 8) 居中圆柱立柱
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
		Library->Modules.Add(Mod);
	}

	// 9) 八角立柱
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
		Library->Modules.Add(Mod);
	}

	return Library->CompileLibrary(OutModules, OutCompatibility);
}

bool AWFC3DArenaGenerator_Style0::BuildBasicRoomModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->CustomCompatibilityPairs.Empty();
	Library->bIncludeAirModule = false; // 基础房间不允许洞

	auto LoadMesh = [](const TCHAR* MeshName) -> UStaticMesh*
	{
		const FString StylePath = FString::Printf(
			TEXT("/Game/Style/0/Meshes/%s.%s"), MeshName, MeshName);
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *StylePath)))
		{
			return Mesh;
		}

		const FString LegacyPath = FString::Printf(
			TEXT("/Game/FBX_Export/%s.%s"), MeshName, MeshName);
		return Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *LegacyPath));
	};

	BasicFloorMesh = LoadMesh(TEXT("SM_Floor_Flat_01"));
	BasicWallMesh = LoadMesh(TEXT("SM_Wall_Solid_01"));
	UStaticMesh* CornerMesh = LoadMesh(TEXT("SM_Corner_Wall_Outer_01"));

	BasicTreeMesh = Cast<UStaticMesh>(StaticLoadObject(
		UStaticMesh::StaticClass(),
		nullptr,
		TEXT("/Game/Style/0/Meshes/Trees/SM_Fir_Tree_09.SM_Fir_Tree_09")));

	if (!BasicFloorMesh || !BasicWallMesh || !CornerMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Style0] Basic room requires Floor_Flat / Wall_Solid / Corner_Outer."));
		return false;
	}

	if (!BasicTreeMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC Style0] Basic room tree not found: /Game/Style/0/Meshes/Trees/SM_Fir_Tree_09. Generating planter only."));
	}

	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = BasicFloorMesh;
	FloorBasePart.RelativeTransform = FTransform::Identity;

	// 1) 地板
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Floor_Flat"));
		Mod.Mesh = BasicFloorMesh;
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	// 2) 直墙（四向派生）
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Solid"));
		Mod.Mesh = BasicWallMesh;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 3) 外转角（四向派生）
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Corner_Outer"));
		Mod.Mesh = CornerMesh;
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	return Library->CompileLibrary(OutModules, OutCompatibility);
}

bool AWFC3DArenaGenerator_Style0::BuildConstraints(
	const FIntVector& GridSize,
	int32 Seed,
	TMap<int32, int32>& OutClamps)
{
	if (!bBasicRoomLayout)
	{
		return true;
	}

	if (GridSize.Z != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Style0] Basic room expects Scale.Z = 1, got %d."), GridSize.Z);
		return false;
	}

	if (GridSize.X < 3 || GridSize.Y < 3)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Style0] Basic room needs at least 3x3, got %dx%d."), GridSize.X, GridSize.Y);
		return false;
	}

	const int32 FloorIndex = FindModuleIndex(FName(TEXT("Floor_Flat")));
	const int32 WallIndex = FindModuleIndex(FName(TEXT("Wall_Solid")));
	const int32 CornerIndex = FindModuleIndex(FName(TEXT("Corner_Outer")));

	if (FloorIndex == INDEX_NONE || WallIndex == INDEX_NONE || CornerIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Style0] Basic room modules missing."));
		return false;
	}

	auto WallVariant = [&](int32 Yaw) -> int32
	{
		const int32 Index = FindModuleVariantIndex(FName(TEXT("Wall_Solid")), Yaw);
		return (Index != INDEX_NONE) ? Index : WallIndex;
	};

	auto CornerVariant = [&](int32 Yaw) -> int32
	{
		const int32 Index = FindModuleVariantIndex(FName(TEXT("Corner_Outer")), Yaw);
		return (Index != INDEX_NONE) ? Index : CornerIndex;
	};

	const int32 MaxX = GridSize.X - 1;
	const int32 MaxY = GridSize.Y - 1;

	for (int32 X = 0; X < GridSize.X; ++X)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			int32 ModuleIndex = INDEX_NONE;

			if (X == 0 && Y == 0)
			{
				ModuleIndex = CornerVariant(90);   // 左下：外侧 -X,-Y
			}
			else if (X == 0 && Y == MaxY)
			{
				ModuleIndex = CornerVariant(0);    // 左上：外侧 -X,+Y
			}
			else if (X == MaxX && Y == 0)
			{
				ModuleIndex = CornerVariant(180);  // 右下：外侧 +X,-Y
			}
			else if (X == MaxX && Y == MaxY)
			{
				ModuleIndex = CornerVariant(270);  // 右上：外侧 +X,+Y
			}
			else if (X == 0)
			{
				ModuleIndex = WallVariant(90);     // 左墙：外侧 -X
			}
			else if (X == MaxX)
			{
				ModuleIndex = WallVariant(270);    // 右墙：外侧 +X
			}
			else if (Y == 0)
			{
				ModuleIndex = WallVariant(180);    // 下墙：外侧 -Y
			}
			else if (Y == MaxY)
			{
				ModuleIndex = WallVariant(0);      // 上墙：外侧 +Y
			}
			else
			{
				ModuleIndex = FloorIndex;
			}

			AddClamp(OutClamps, X, Y, 0, ModuleIndex);
		}
	}

	return true;
}

void AWFC3DArenaGenerator_Style0::PostProcess(int32 Seed)
{
	if (bBasicRoomLayout)
	{
		BuildBasicRoomDecorations();
	}
}

void AWFC3DArenaGenerator_Style0::BuildBasicRoomDecorations()
{
	if (!BasicFloorMesh || !BasicWallMesh)
	{
		return;
	}

	const FIntVector Dim = Solver.GetDimensions();
	const FVector CellSize = GetCellWorldSize();
	if (Dim.X < 5 || Dim.Y < 5 || CellSize.IsNearlyZero())
	{
		return;
	}

	// 中心 2x2 花坛
	const int32 MinCellX = (Dim.X - 2) / 2;
	const int32 MinCellY = (Dim.Y - 2) / 2;
	const FVector PlanterMin(MinCellX * CellSize.X, MinCellY * CellSize.Y, 0.0f);
	const FVector PlanterMax((MinCellX + 2) * CellSize.X, (MinCellY + 2) * CellSize.Y, 0.0f);
	const FVector RoomCenter(
		(PlanterMin.X + PlanterMax.X) * 0.5f,
		(PlanterMin.Y + PlanterMax.Y) * 0.5f,
		0.0f);

	const FBoxSphereBounds FloorBounds = BasicFloorMesh->GetBounds();
	const float FloorBottom = FloorBounds.Origin.Z - FloorBounds.BoxExtent.Z;
	const float FloorTop = FloorBounds.Origin.Z + FloorBounds.BoxExtent.Z;
	const float FloorThickness = FloorTop - FloorBottom;
	const float SoilTop = FloorTop + PlanterRise + FloorThickness;

	// 1) 花坛土床：4 块抬高的地板
	if (UInstancedStaticMeshComponent* FloorComp = GetOrCreateInstancedComponent(BasicFloorMesh))
	{
		for (int32 DX = 0; DX < 2; ++DX)
		{
			for (int32 DY = 0; DY < 2; ++DY)
			{
				const float CX = (MinCellX + DX + 0.5f) * CellSize.X;
				const float CY = (MinCellY + DY + 0.5f) * CellSize.Y;

				FTransform SoilTransform;
				SoilTransform.SetLocation(FVector(
					CX - FloorBounds.Origin.X,
					CY - FloorBounds.Origin.Y,
					FloorTop + PlanterRise - FloorBottom));
				SoilTransform.SetRotation(FQuat::Identity);
				SoilTransform.SetScale3D(FVector::OneVector);
				FloorComp->AddInstance(SoilTransform, /* bWorldSpace = */ false);
			}
		}
	}

	// 2) 花坛矮墙：围绕 2x2 的四条边，各两段
	if (UInstancedStaticMeshComponent* WallComp = GetOrCreateInstancedComponent(BasicWallMesh))
	{
		const FBoxSphereBounds WallBounds = BasicWallMesh->GetBounds();
		const float WallHeight = FMath::Max(1.0f, 2.0f * WallBounds.BoxExtent.Z);
		const float WallScaleZ = PlanterWallHeight / WallHeight;
		const float WallBottom = WallBounds.Origin.Z - WallBounds.BoxExtent.Z;
		const bool bWallLongAxisY = WallBounds.BoxExtent.Y >= WallBounds.BoxExtent.X;

		const float YawAlongY = bWallLongAxisY ? 0.0f : 90.0f;
		const float YawAlongX = bWallLongAxisY ? 90.0f : 0.0f;

		auto AddBorderPiece = [&](const FVector& TargetXY, float Yaw)
		{
			const FRotator Rot(0.0f, Yaw, 0.0f);
			const FVector Scale(1.0f, 1.0f, WallScaleZ);
			const FVector CenterOffset = Rot.RotateVector(FVector(
				WallBounds.Origin.X,
				WallBounds.Origin.Y,
				0.0f));

			FTransform BorderTransform;
			BorderTransform.SetLocation(FVector(
				TargetXY.X - CenterOffset.X,
				TargetXY.Y - CenterOffset.Y,
				FloorTop - WallBottom * WallScaleZ));
			BorderTransform.SetRotation(Rot.Quaternion());
			BorderTransform.SetScale3D(Scale);
			WallComp->AddInstance(BorderTransform, /* bWorldSpace = */ false);
		};

		for (int32 I = 0; I < 2; ++I)
		{
			const float Y = PlanterMin.Y + (I + 0.5f) * CellSize.Y;
			AddBorderPiece(FVector(PlanterMin.X, Y, 0.0f), YawAlongY);
			AddBorderPiece(FVector(PlanterMax.X, Y, 0.0f), YawAlongY);
		}

		for (int32 I = 0; I < 2; ++I)
		{
			const float X = PlanterMin.X + (I + 0.5f) * CellSize.X;
			AddBorderPiece(FVector(X, PlanterMin.Y, 0.0f), YawAlongX);
			AddBorderPiece(FVector(X, PlanterMax.Y, 0.0f), YawAlongX);
		}
	}

	// 3) 中心树
	if (BasicTreeMesh)
	{
		if (UInstancedStaticMeshComponent* TreeComp = GetOrCreateInstancedComponent(BasicTreeMesh))
		{
			const FBoxSphereBounds TreeBounds = BasicTreeMesh->GetBounds();
			const float Scale = FMath::Max(0.01f, TreeScale);
			const float TreeBottom = (TreeBounds.Origin.Z - TreeBounds.BoxExtent.Z) * Scale;
			const FVector CenterOffset(
				TreeBounds.Origin.X * Scale,
				TreeBounds.Origin.Y * Scale,
				0.0f);

			FTransform TreeTransform;
			TreeTransform.SetLocation(FVector(
				RoomCenter.X - CenterOffset.X,
				RoomCenter.Y - CenterOffset.Y,
				SoilTop - TreeBottom));
			TreeTransform.SetRotation(FQuat::Identity);
			TreeTransform.SetScale3D(FVector(Scale));
			TreeComp->AddInstance(TreeTransform, /* bWorldSpace = */ false);
		}
	}
}

FVector AWFC3DArenaGenerator_Style0::GetCellWorldSize() const
{
	return FVector(200.0f, 200.0f, 200.0f);
}
