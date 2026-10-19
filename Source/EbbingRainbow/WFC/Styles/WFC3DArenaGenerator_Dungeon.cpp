// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_Dungeon.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"

AWFC3DArenaGenerator_Dungeon::AWFC3DArenaGenerator_Dungeon()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_Dungeon::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->CustomCompatibilityPairs.Empty();
	Library->bIncludeAirModule = true;
	Library->AirModuleWeight = 2.0f;
	Library->AirSocketName = FName(TEXT("Air"));

	// 墙体外侧与空气/楼板顶面可以相邻
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Exterior")), FName(TEXT("Air")), true });

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"));

	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Dungeon] Failed to load SM_Cube."));
		return false;
	}

	// 通用地面板底座（每格底部 10cm 厚）
	FWFC3DMeshPart FloorBasePart;
	FloorBasePart.Mesh = CubeMesh;
	FloorBasePart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -45.0f), FVector(1.0f, 1.0f, 0.1f));

	// 1) 开阔房间地板
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
		Library->Modules.Add(Mod);
	}

	// 2) 室内柱厅
	if (CylinderMesh)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Room_Pillar"));
		Mod.Mesh = CylinderMesh;
		Mod.MeshScale = FVector(0.35f, 0.35f, 0.9f);
		Mod.AdditionalOffset = FVector(0.0f, 0.0f, 5.0f);
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

	// 3) 实心直墙
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Straight"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 1.0f, 0.9f);
		Mod.AdditionalOffset = FVector(42.5f, 0.0f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart);
		Mod.Weight = 2.5f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Exterior")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 4) 90 度 L 型转角墙
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Corner"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 0.85f, 0.9f);
		Mod.AdditionalOffset = FVector(42.5f, -7.5f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart);

		FWFC3DMeshPart WallYPart;
		WallYPart.Mesh = CubeMesh;
		WallYPart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-7.5f, 42.5f, 5.0f), FVector(0.85f, 0.15f, 0.9f));
		Mod.AdditionalParts.Add(WallYPart);

		if (CylinderMesh)
		{
			FWFC3DMeshPart CornerPillarPart;
			CornerPillarPart.Mesh = CylinderMesh;
			CornerPillarPart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(42.5f, 42.5f, 5.0f), FVector(0.2f, 0.2f, 0.9f));
			Mod.AdditionalParts.Add(CornerPillarPart);
		}

		Mod.Weight = 2.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 5) 门洞通道
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_Doorway"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 1.0f, 0.2f);
		Mod.AdditionalOffset = FVector(42.5f, 0.0f, 40.0f);
		Mod.AdditionalParts.Add(FloorBasePart);

		FWFC3DMeshPart LeftPost;
		LeftPost.Mesh = CubeMesh;
		LeftPost.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(42.5f, -40.0f, 5.0f), FVector(0.15f, 0.2f, 0.9f));
		Mod.AdditionalParts.Add(LeftPost);

		FWFC3DMeshPart RightPost;
		RightPost.Mesh = CubeMesh;
		RightPost.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(42.5f, 40.0f, 5.0f), FVector(0.15f, 0.2f, 0.9f));
		Mod.AdditionalParts.Add(RightPost);

		Mod.Weight = 1.2f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 6) T 型分叉隔断墙
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Wall_TJunction"));
		Mod.Mesh = CubeMesh;
		Mod.MeshScale = FVector(0.15f, 1.0f, 0.9f);
		Mod.AdditionalOffset = FVector(42.5f, 0.0f, 5.0f);
		Mod.AdditionalParts.Add(FloorBasePart);

		FWFC3DMeshPart BranchPart;
		BranchPart.Mesh = CubeMesh;
		BranchPart.RelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-7.5f, 0.0f, 5.0f), FVector(0.85f, 0.15f, 0.9f));
		Mod.AdditionalParts.Add(BranchPart);

		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Exterior")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Wall_Joint")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	return Library->CompileLibrary(OutModules, OutCompatibility);
}

FVector AWFC3DArenaGenerator_Dungeon::GetCellWorldSize() const
{
	return FVector(100.0f, 100.0f, 100.0f);
}
