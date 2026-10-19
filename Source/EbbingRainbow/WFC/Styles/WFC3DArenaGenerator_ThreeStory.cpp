// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_ThreeStory.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"

AWFC3DArenaGenerator_ThreeStory::AWFC3DArenaGenerator_ThreeStory()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_ThreeStory::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->AirSocketName = FName(TEXT("Air"));
	Library->bIncludeAirModule = true;
	Library->AirModuleWeight = 1.5f;
	Library->CustomCompatibilityPairs.Empty();
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Exterior")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Exterior")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Exterior")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Exterior")), true });

	auto LoadMesh = [](const TCHAR* Path) -> UStaticMesh*
	{
		return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, Path));
	};

	UStaticMesh* FloorFlat    = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Flat_01.SM_Floor_Flat_01"));
	UStaticMesh* FloorPathway = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Pathway_01.SM_Floor_Pathway_01"));
	UStaticMesh* FloorGrating = LoadMesh(TEXT("/Game/FBX_Export/SM_Floor_Grating_01.SM_Floor_Grating_01"));

	UStaticMesh* WallSolid    = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Solid_01.SM_Wall_Solid_01"));
	UStaticMesh* WallDoorway  = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Doorway_01.SM_Wall_Doorway_01"));
	UStaticMesh* WallWindow   = LoadMesh(TEXT("/Game/FBX_Export/SM_Wall_Window_01.SM_Wall_Window_01"));

	UStaticMesh* CornerOuter  = LoadMesh(TEXT("/Game/FBX_Export/SM_Corner_Wall_Outer_01.SM_Corner_Wall_Outer_01"));
	UStaticMesh* ColumnCyl    = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Cylinder_Center_01.SM_Column_Cylinder_Center_01"));
	UStaticMesh* ColumnOct    = LoadMesh(TEXT("/Game/FBX_Export/SM_Column_Octagonal_01.SM_Column_Octagonal_01"));

	UStaticMesh* CeilingFlat  = LoadMesh(TEXT("/Game/FBX_Export/SM_Ceiling_Flat_01.SM_Ceiling_Flat_01"));
	UStaticMesh* StairsMesh   = LoadMesh(TEXT("/Game/FBX_Export/SM_Stairs_Straight_01.SM_Stairs_Straight_01"));

	if (!FloorFlat || !WallSolid || !StairsMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC ThreeStory] Missing required meshes in /Game/FBX_Export/."));
		return false;
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

	// 1) 标准房间地面
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
		Library->Modules.Add(Mod);
	}

	// 2) 通道地面
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
		Library->Modules.Add(Mod);
	}

	// 3) 格栅地面
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
		Library->Modules.Add(Mod);
	}

	// 4) 实心墙体
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
		Library->Modules.Add(Mod);
	}

	// 5) 窗框墙体
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
		Library->Modules.Add(Mod);
	}

	// 6) 门洞墙体
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
		Library->Modules.Add(Mod);
	}

	// 7) 外转角墙
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
		Library->Modules.Add(Mod);
	}

	// 8) 圆柱立柱
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
		Library->Modules.Add(Mod);
	}

	// 9) 八角立柱
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
		Library->Modules.Add(Mod);
	}

	// 10) 楼梯爬升主体
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
		Library->Modules.Add(Mod);
	}

	// 11) 楼梯井上层挑空
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
		Library->Modules.Add(Mod);
	}

	return Library->CompileLibrary(OutModules, OutCompatibility);
}

bool AWFC3DArenaGenerator_ThreeStory::BuildConstraints(
	const FIntVector& GridSize,
	int32 Seed,
	TMap<int32, int32>& OutClamps)
{
	const int32 FloorModuleIndex = FindModuleIndex(FName(TEXT("Floor_Flat")));
	const int32 StairModuleIndex = FindModuleIndex(FName(TEXT("Stairs_Ascend")));
	const int32 WellholeModuleIndex = FindModuleIndex(FName(TEXT("Stair_Wellhole")));

	if (bEnforceGroundFloor && FloorModuleIndex != INDEX_NONE)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			for (int32 Y = 0; Y < GridSize.Y; ++Y)
			{
				AddClamp(OutClamps, X, Y, 0, FloorModuleIndex);
			}
		}
	}

	if (bEnsureStairConnectivity
		&& GridSize.Z >= 2
		&& StairModuleIndex != INDEX_NONE
		&& WellholeModuleIndex != INDEX_NONE)
	{
		const int32 SafeMinX = FMath::Clamp(2, 0, GridSize.X - 1);
		const int32 SafeMaxX = FMath::Clamp(GridSize.X - 3, SafeMinX, GridSize.X - 1);
		const int32 SafeMinY = FMath::Clamp(2, 0, GridSize.Y - 1);
		const int32 SafeMaxY = FMath::Clamp(GridSize.Y - 3, SafeMinY, GridSize.Y - 1);

		FRandomStream Rnd(Seed);

		for (int32 FloorZ = 0; FloorZ < GridSize.Z - 1; ++FloorZ)
		{
			for (int32 StairCount = 0; StairCount < StairsPerFloorTransition; ++StairCount)
			{
				int32 PickX = 0;
				int32 PickY = 0;

				// 交错不同层级的楼梯位置：偶数层偏一侧，奇数层偏对角侧
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

				AddClamp(OutClamps, PickX, PickY, FloorZ, StairModuleIndex);
				AddClamp(OutClamps, PickX, PickY, FloorZ + 1, WellholeModuleIndex);

				UE_LOG(LogTemp, Log, TEXT("[WFC ThreeStory] Floor %d -> %d: stair at (%d, %d, %d), wellhole at (%d, %d, %d)."),
					FloorZ, FloorZ + 1, PickX, PickY, FloorZ, PickX, PickY, FloorZ + 1);
			}
		}
	}

	return true;
}

FVector AWFC3DArenaGenerator_ThreeStory::GetCellWorldSize() const
{
	return FVector(200.0f, 200.0f, 200.0f);
}
