// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_MegaAtrium.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"

AWFC3DArenaGenerator_MegaAtrium::AWFC3DArenaGenerator_MegaAtrium()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_MegaAtrium::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->AirSocketName = FName(TEXT("Air"));
	Library->bIncludeAirModule = true;
	Library->AirModuleWeight = 2.0f;
	Library->CustomCompatibilityPairs.Empty();
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Exterior")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Wall_Joint")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Exterior")), true });
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
		UE_LOG(LogTemp, Error, TEXT("[WFC MegaAtrium] Missing required meshes in /Game/FBX_Export/."));
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

	// 0) 中央开阔广场地面：向上彻底挑空
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Plaza_Floor"));
		Mod.Mesh = FloorFlat;
		Mod.Weight = 4.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
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
		Mod.Weight = 3.5f;
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
		Mod.Weight = 2.0f;
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
		Mod.Weight = 1.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Solid_Ground")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 5) 观景窗墙
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
		Library->Modules.Add(Mod);
	}

	// 6) 连通门洞
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
		Library->Modules.Add(Mod);
	}

	// 7) 外转角
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
		Library->Modules.Add(Mod);
	}

	// 8) 圆柱
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
		Library->Modules.Add(Mod);
	}

	// 9) 八角柱
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

bool AWFC3DArenaGenerator_MegaAtrium::BuildConstraints(
	const FIntVector& GridSize,
	int32 Seed,
	TMap<int32, int32>& OutClamps)
{
	int32 PlazaFloorModuleIndex = FindModuleIndex(FName(TEXT("Plaza_Floor")));
	const int32 FloorModuleIndex = FindModuleIndex(FName(TEXT("Floor_Flat")));
	const int32 AirModuleIndex = FindModuleIndex(FName(TEXT("Air")));
	const int32 StairModuleIndex = FindModuleIndex(FName(TEXT("Stairs_Ascend")));
	const int32 WellholeModuleIndex = FindModuleIndex(FName(TEXT("Stair_Wellhole")));

	if (PlazaFloorModuleIndex == INDEX_NONE)
	{
		PlazaFloorModuleIndex = FloorModuleIndex;
	}

	if (FloorModuleIndex == INDEX_NONE || AirModuleIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC MegaAtrium] Missing Floor_Flat or Air module; generating without atrium constraints."));
		return true;
	}

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
				// 中庭贯通区域：底层为广场，上层全部挑空
				for (int32 Z = 0; Z < GridSize.Z; ++Z)
				{
					AddClamp(OutClamps, X, Y, Z, (Z == 0) ? PlazaFloorModuleIndex : AirModuleIndex);
				}
			}
			else
			{
				// 回廊底层保证坚实地面
				AddClamp(OutClamps, X, Y, 0, FloorModuleIndex);

				// 收集中庭边缘的回廊核心格，用于布置观景楼梯
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

	// 楼梯强连通：沿回廊环线交错布置楼梯与挑空天井
	if (bEnsureStairConnectivity
		&& GridSize.Z >= 2
		&& StairModuleIndex != INDEX_NONE
		&& WellholeModuleIndex != INDEX_NONE
		&& WalkwayRingCells.Num() > 0)
	{
		FRandomStream Rnd(Seed);
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

				AddClamp(OutClamps, StairPos.X, StairPos.Y, FloorZ, StairModuleIndex);
				AddClamp(OutClamps, StairPos.X, StairPos.Y, FloorZ + 1, WellholeModuleIndex);

				UE_LOG(LogTemp, Log, TEXT("[WFC MegaAtrium] Floor %d -> %d: stair at (%d, %d, %d), wellhole at (%d, %d, %d)."),
					FloorZ, FloorZ + 1, StairPos.X, StairPos.Y, FloorZ, StairPos.X, StairPos.Y, FloorZ + 1);
			}
		}
	}

	return true;
}

FVector AWFC3DArenaGenerator_MegaAtrium::GetCellWorldSize() const
{
	return FVector(200.0f, 200.0f, 200.0f);
}
