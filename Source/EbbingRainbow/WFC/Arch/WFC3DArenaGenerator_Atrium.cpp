// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DArenaGenerator_Atrium.h"
#include "WFCArchGrammar.h"
#include "Engine/StaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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
	else if (Id.StartsWith(TEXT("stair_")))
	{
		Folder = TEXT("Stairs");
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

void AWFC3DArenaGenerator_Atrium::AddStairs(const FArchPlan& Plan)
{
	UStaticMesh* Mesh = LoadTileMesh(FName(TEXT("stair_core")));
	if (!Mesh)
	{
		return;
	}

	UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(Mesh);
	if (!Comp)
	{
		return;
	}

	// The stair mesh is authored in centimetres (400 x 800 x 508 uu), unlike
	// the kit modules which import at 4 uu. It is placed at scale 1 and its
	// collision is handled by the explicit ramp/landing boxes below.
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);

	for (const FArchStair& S : Plan.Stairs)
	{
		// The FBX import flips Y, so yaw 180 makes the stair rise toward -Y
		// (north) and places its footprint in [X, X+1] x [Y-1, Y] cells.
		const FVector Location(
			static_cast<float>(S.X) * CellSize + CellSize,
			static_cast<float>(GridH - S.Y) * CellSize,
			0.0f);
		Comp->AddInstance(
			FTransform(FRotator(0.0f, 180.0f, 0.0f), Location, FVector(1.0f)),
			false);
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

			// Floor slab at z=8 cm (16 cm thick); supports NavMesh. Void cells
			// are left without a slab so irregular room-graph layouts do not
			// get a full 19x19 base plate.
			if (C.Kind != EArchCellKind::Void)
			{
				CollisionHISM->AddInstance(FTransform(
					FRotator::ZeroRotator,
					FVector(BaseX + 200.0f, BaseY - 200.0f, 8.0f),
					FloorScale), false);
			}

			// Mezzanine slab at +4 m, only on cells that actually carry an
			// upper floor tile. The open cross remains a two-storey void.
			if (!C.UFloor.IsNone())
			{
				CollisionHISM->AddInstance(FTransform(
					FRotator::ZeroRotator,
					FVector(BaseX + 200.0f, BaseY - 200.0f, 408.0f),
					FloorScale), false);
			}

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

	// Stair ramps: one invisible walkable slab per stair, plus a 1 m top
	// landing and side rails. The visual stair mesh itself has no collision.
	// 20 steps x 35 cm = 700 cm of run, then a 100 cm flat top landing.
	constexpr float StairRun = 700.0f;
	constexpr float StairRise = 400.0f;
	const float StairLen = FMath::Sqrt(StairRun * StairRun + StairRise * StairRise);
	// The slab is long along Y, so the slope is a roll about X (pitch would
	// tilt the 4 m width instead of the 8 m run).
	const float StairRoll = FMath::RadiansToDegrees(FMath::Atan2(StairRise, StairRun));

	for (const FArchStair& S : Plan.Stairs)
	{
		const float BaseX = static_cast<float>(S.X) * CellSize;
		const float BaseY = static_cast<float>(GridH - S.Y) * CellSize;

		// Ramp from (BaseY, z=16) up to (BaseY-800, z=416), as a dedicated
		// StaticMeshComponent: rotated HISM instances did not generate navmesh
		// reliably, while unrotated HISM slabs did.
		UStaticMeshComponent* Ramp = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("StairRamp_%d"), StairRamps.Num()));
		Ramp->SetupAttachment(GetRootComponent());
		Ramp->SetStaticMesh(Cube);
		Ramp->SetMobility(EComponentMobility::Movable);
		Ramp->SetRelativeLocation(FVector(BaseX + 200.0f, BaseY - 350.0f, 216.0f));
		Ramp->SetRelativeRotation(FRotator(0.0f, 0.0f, StairRoll));
		Ramp->SetRelativeScale3D(FVector(4.0f, StairLen / 100.0f, 0.2f));
		Ramp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Ramp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Ramp->SetCanEverAffectNavigation(true);
		Ramp->SetVisibility(false);
		Ramp->RegisterComponent();
		StairRamps.Add(Ramp);

		// Low side rails follow the same 700 cm run; the 100 cm top landing is
		// open left/right so units can step onto the mezzanine.
		for (float SideX : { BaseX + 10.0f, BaseX + 390.0f })
		{
			CollisionHISM->AddInstance(FTransform(
				FRotator(0.0f, 0.0f, StairRoll),
				FVector(SideX, BaseY - 350.0f, 216.0f),
				FVector(0.2f, StairLen / 100.0f, 1.0f)), false);
		}

		// 1 m top landing at z=400, flush with the mezzanine slab.
		CollisionHISM->AddInstance(FTransform(
			FRotator::ZeroRotator,
			FVector(BaseX + 200.0f, BaseY - 750.0f, 408.0f),
			FVector(4.0f, 1.0f, 0.16f)), false);
	}
}

void AWFC3DArenaGenerator_Atrium::ClearGenerated()
{
	Super::ClearGenerated();
	if (CollisionHISM)
	{
		CollisionHISM->ClearInstances();
	}
	for (TObjectPtr<UStaticMeshComponent>& Ramp : StairRamps)
	{
		if (Ramp)
		{
			Ramp->DestroyComponent();
		}
	}
	StairRamps.Reset();
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

	// The L1 walkway beside the first stair is the stable upper-level probe;
	// its X depends on the randomized aisle width.
	if (Plan.Stairs.Num() > 0)
	{
		const FArchStair& S = Plan.Stairs[0];
		LastResult.UpperNavProbe = FVector(
			static_cast<float>(S.X - 1) * CellSize + 200.0f,
			static_cast<float>(GridH - (S.Y + 1)) * CellSize - 200.0f,
			420.0f);
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

	AddStairs(Plan);

	// Visual HISM keeps no collision: the explicit collision boxes below are
	// the single source of truth. Otherwise an imported wall mesh's own simple
	// collision can seal a door opening.
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& Pair
		: InstancedMeshComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pair.Value->SetCanEverAffectNavigation(false);
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

