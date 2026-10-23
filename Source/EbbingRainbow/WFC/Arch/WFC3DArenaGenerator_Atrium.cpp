// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DArenaGenerator_Atrium.h"
#include "WFCArchGrammar.h"
#include "Engine/StaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "HAL/PlatformTime.h"

AWFC3DArenaGenerator_Atrium::AWFC3DArenaGenerator_Atrium()
{
	PrimaryActorTick.bCanEverTick = false;
}

UStaticMesh* AWFC3DArenaGenerator_Atrium::LoadTileMesh(const FName& TileId)
{
	if (TileId.IsNone())
	{
		return nullptr;
	}
	if (const TObjectPtr<UStaticMesh>* Found = MeshCache.Find(TileId))
	{
		return *Found;
	}

	const FString Id = TileId.ToString();
	// The kit's open cell is intentionally mesh-less.
	if (Id == TEXT("wall_oooo") || Id == TEXT("wall_oooo_b"))
	{
		MeshCache.Add(TileId, nullptr);
		return nullptr;
	}

	FString Folder;
	if (Id.StartsWith(TEXT("wall_")))
	{
		Folder = TEXT("Walls");
	}
	else if (Id.StartsWith(TEXT("floor_")))
	{
		Folder = TEXT("Floors");
	}
	else if (Id.StartsWith(TEXT("ceil_")))
	{
		Folder = TEXT("Ceilings");
	}
	else if (Id.StartsWith(TEXT("prop_")))
	{
		Folder = TEXT("Props");
	}
	else
	{
		MeshCache.Add(TileId, nullptr);
		return nullptr;
	}

	const FString AssetName = FString::Printf(TEXT("SM_%s"), *Id);
	const FString Path = FString::Printf(TEXT("/Game/WFCKit/Meshes/%s/%s.%s"),
		*Folder, *AssetName, *AssetName);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Arch] Missing kit mesh %s"), *Path);
	}
	MeshCache.Add(TileId, Mesh);
	return Mesh;
}

void AWFC3DArenaGenerator_Atrium::AddTile(const FName& TileId, int32 X, int32 Y, float Z)
{
	if (TileId.IsNone())
	{
		return;
	}
	UStaticMesh* Mesh = LoadTileMesh(TileId);
	if (!Mesh)
	{
		return;
	}
	if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(Mesh))
	{
		// The kit is authored with the cell on -Y in UE; the plan's +Y (north)
		// maps to decreasing world Y, so a 19x19 arena stays in [0,19*400].
		const FVector Location(
			static_cast<float>(X) * CellSize,
			static_cast<float>(GridH - Y) * CellSize,
			Z);
		// The MCP mesh importer did not apply the FBX UnitScaleFactor (100):
		// the imported assets are 4 uu instead of 400 uu. Scale each instance
		// back to the 4 m kit module; collision boxes are built separately.
		const FVector Scale(100.0f, 100.0f, 100.0f);
		Comp->AddInstance(FTransform(FRotator::ZeroRotator, Location, Scale), false);
	}
}

void AWFC3DArenaGenerator_Atrium::BuildCollision(const FArchPlan& Plan)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Arch] No engine cube for collision boxes."));
		return;
	}

	if (!CollisionHISM)
	{
		CollisionHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("ArchCollision"));
		CollisionHISM->SetupAttachment(GetRootComponent());
		CollisionHISM->SetStaticMesh(Cube);
		CollisionHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionHISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		CollisionHISM->SetCanEverAffectNavigation(true);
		CollisionHISM->SetVisibility(false);
		CollisionHISM->RegisterComponent();
	}
	CollisionHISM->ClearInstances();

	const FVector WallScale(4.0f, 0.30f, 4.0f); // 400 x 30 x 400 uu
	const FVector FloorScale(4.0f, 4.0f, 0.16f);

	for (int32 Y = 0; Y < Plan.H; ++Y)
	{
		for (int32 X = 0; X < Plan.W; ++X)
		{
			const FArchCell& C = Plan.Cell(X, Y);
			const float BaseX = static_cast<float>(X) * CellSize;
			const float BaseY = static_cast<float>(GridH - Y) * CellSize;

			// Floor slab at z=8 cm (16 cm thick); supports NavMesh.
			CollisionHISM->AddInstance(FTransform(
				FRotator::ZeroRotator,
				FVector(BaseX + 200.0f, BaseY - 200.0f, 8.0f),
				FloorScale), false);

			// Walls, windows and half walls block; doors / passages / arches do not.
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const TCHAR Code = C.Special[Side];
				const bool bBlocked = (C.WallMask & (1 << Side)) != 0
					|| Code == TEXT('n') || Code == TEXT('h');
				if (!bBlocked)
				{
					continue;
				}

				FVector Loc(BaseX + 200.0f, BaseY - 200.0f, 200.0f);
				FVector Scale = WallScale;
				switch (Side)
				{
				case 0: // north (world -Y)
					Loc.Y = BaseY - 400.0f + 15.0f;
					Scale = FVector(4.0f, 0.30f, 4.0f);
					break;
				case 1: // east (+X)
					Loc.X = BaseX + 400.0f - 15.0f;
					Scale = FVector(0.30f, 4.0f, 4.0f);
					break;
				case 2: // south (world +Y)
					Loc.Y = BaseY - 15.0f;
					Scale = FVector(4.0f, 0.30f, 4.0f);
					break;
				default: // west (-X)
					Loc.X = BaseX + 15.0f;
					Scale = FVector(0.30f, 4.0f, 4.0f);
					break;
				}
				CollisionHISM->AddInstance(FTransform(FRotator::ZeroRotator, Loc, Scale), false);
			}

			if (C.bColumn)
			{
				CollisionHISM->AddInstance(FTransform(
					FRotator::ZeroRotator,
					FVector(BaseX + 200.0f, BaseY - 200.0f, 200.0f),
					FVector(1.0f, 1.0f, 4.0f)), false);
			}
		}
	}
}

void AWFC3DArenaGenerator_Atrium::ClearGenerated()
{
	Super::ClearGenerated();
	if (CollisionHISM)
	{
		CollisionHISM->ClearInstances();
	}
}

bool AWFC3DArenaGenerator_Atrium::Generate(const FWFCGenerationRequest& Request)
{
	LastResult = FWFCGenerationResult();
	ClearGenerated();
	MeshCache.Reset();

	if (Request.Scale.X != GridW || Request.Scale.Y != GridH || Request.Scale.Z != 2)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Arch] Atrium expects Scale 19x19x2, got %dx%dx%d; using 19x19x2."),
			Request.Scale.X, Request.Scale.Y, Request.Scale.Z);
	}

	const int32 Seed = (Request.Seed != 0) ? Request.Seed : FMath::Rand();
	const double StartTime = FPlatformTime::Seconds();

	FArchPlan Plan;
	if (!FWFCArchGrammar::Build(Seed, Plan))
	{
		UE_LOG(LogTemp, Error, TEXT("[Arch] Grammar failed for seed %d."), Seed);
		return false;
	}

	int32 InstanceCount = 0;
	for (int32 Y = 0; Y < Plan.H; ++Y)
	{
		for (int32 X = 0; X < Plan.W; ++X)
		{
			const FArchCell& C = Plan.Cell(X, Y);

			AddTile(C.WallTile, X, Y, 0.0f);
			AddTile(C.FloorTile, X, Y, 0.0f);
			AddTile(C.CeilTile, X, Y, 0.0f);
			AddTile(C.PropTile, X, Y, 0.0f);

			AddTile(C.UFloor, X, Y, 400.0f);
			AddTile(C.UWall, X, Y, 400.0f);
			AddTile(C.URailing, X, Y, 400.0f);
			AddTile(C.URailing2, X, Y, 400.0f);
			AddTile(C.UCeil, X, Y, 400.0f);
			if (C.bUColumn)
			{
				AddTile(FName(TEXT("wall_oooo_column")), X, Y, 400.0f);
			}
			++InstanceCount;
		}
	}

	BuildCollision(Plan);

	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	LastResult.bSuccess = true;
	LastResult.UsedSeed = Seed;
	LastResult.UsedScale = FIntVector(GridW, GridH, 2);
	LastResult.CellWorldSize = GetCellWorldSize();
	LastResult.ModuleCount = MeshCache.Num();
	LastResult.CellCount = GridW * GridH * 2;
	LastResult.DurationMs = DurationMs;

	UE_LOG(LogTemp, Log,
		TEXT("[Arch] Atrium generated seed=%d rooms=%d cells=%d tiles=%d in %.2f ms."),
		Seed, Plan.Rooms.Num(), InstanceCount, MeshCache.Num(), DurationMs);
	return true;
}
