// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_Style0.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"

AWFC3DArenaGenerator_Style0::AWFC3DArenaGenerator_Style0()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_Style0::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->CustomCompatibilityPairs.Empty();
	Library->bIncludeAirModule = true;
	Library->AirModuleWeight = 1.0f;       // 保持竞技场开阔，但留出少量负空间
	Library->AirSocketName = FName(TEXT("Air"));

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

	if (!FloorFlat || !WallSolid)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Style0] Missing required meshes in /Game/FBX_Export/ (Floor_Flat / Wall_Solid)."));
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

FVector AWFC3DArenaGenerator_Style0::GetCellWorldSize() const
{
	return FVector(200.0f, 200.0f, 200.0f);
}
