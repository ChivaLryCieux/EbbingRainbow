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

	// 楼梯: 4 m 宽 x 8 m 长, 按 Dir 旋转 (0=+Y 1=+X 2=-Y 3=-X)
	// 网格原点在局部 (0,0) 角上, 沿局部 +X/+Y 展开, 所以先把「脚点」
	// 换算到 2x1 格的足迹中心, 再按朝向反推原点位置。
	for (const FArchStair& S : Plan.Stairs)
	{
		const float DirX[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
		const float DirY[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
		const float Yaw[4] = { 180.0f, 270.0f, 0.0f, 90.0f };
		const int32 D = FMath::Clamp(S.Dir, 0, 3);

		// 底部格中心 (世界)
		const FVector2D BottomCenter(
			(static_cast<float>(S.X) + 0.5f) * CellSize,
			(static_cast<float>(GridH - S.Y) - 0.5f) * CellSize);
		// 世界方向: plan +Y = 世界 -Y
		const FVector2D DirWorld(DirX[D], -DirY[D]);
		const FVector2D Center = BottomCenter + DirWorld * (CellSize * 0.5f);

		const float Rad = FMath::DegreesToRadians(Yaw[D]);
		const float CosY = FMath::Cos(Rad);
		const float SinY = FMath::Sin(Rad);
		// R * (200, 400)
		const FVector2D Offset(
			CellSize * 0.5f * CosY - CellSize * SinY,
			CellSize * 0.5f * SinY + CellSize * CosY);
		const FVector2D Origin = Center - Offset;

		Comp->AddInstance(
			FTransform(FRotator(0.0f, Yaw[D], 0.0f),
				FVector(Origin.X, Origin.Y, 0.0f), FVector(1.0f)),
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

	// 楼梯碰撞: 用一串轴对齐台阶盒铺出可走坡道。
	// 台阶盒是「不旋转的 HISM 实例」, 导航系统能稳定烘焙 (旋转过的斜坡曾出现
	// 烘焙不出来的问题); 台阶高 20 cm, 远低于 AgentMaxStepHeight。
	// 地面面层在 z=16, 夹层面层在 z=416, 所以整段抬升 400 cm。
	constexpr float StairRise = 400.0f;
	constexpr float StairRun = 800.0f;
	constexpr int32 StairSteps = 20;
	const float StepRun = StairRun / StairSteps;
	const float StepRise = StairRise / StairSteps;

	for (const FArchStair& S : Plan.Stairs)
	{
		const float DirX[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
		const float DirY[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
		const int32 D = FMath::Clamp(S.Dir, 0, 3);

		const FVector2D BottomCenter(
			(static_cast<float>(S.X) + 0.5f) * CellSize,
			(static_cast<float>(GridH - S.Y) - 0.5f) * CellSize);
		const FVector2D DirWorld(DirX[D], -DirY[D]);
		// 从底部格的后缘开始, 走满 2 格
		const FVector2D BackEdge = BottomCenter - DirWorld * (CellSize * 0.5f);

		const bool bAlongY = (D == 0 || D == 2);
		for (int32 I = 0; I < StairSteps; ++I)
		{
			const float T = (static_cast<float>(I) + 0.5f) * StepRun;
			const FVector2D Pos = BackEdge + DirWorld * T;
			const float Top = 16.0f + (static_cast<float>(I) + 1.0f) * StepRise;
			const FVector Scale = bAlongY
				? FVector(4.0f, StepRun / 100.0f, Top / 100.0f)
				: FVector(StepRun / 100.0f, 4.0f, Top / 100.0f);
			CollisionHISM->AddInstance(FTransform(
				FRotator::ZeroRotator,
				FVector(Pos.X, Pos.Y, Top * 0.5f),
				Scale), false);
		}

		// 顶部 1 格平台, 与夹层面层齐平 (z=416 顶面)
		const FVector2D TopCenter = BottomCenter + DirWorld * (CellSize * 0.5f);
		CollisionHISM->AddInstance(FTransform(
			FRotator::ZeroRotator,
			FVector(TopCenter.X, TopCenter.Y, 408.0f),
			FVector(4.0f, 4.0f, 0.16f)), false);
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

	// 上层导航探针: 取第一部楼梯的**顶部格**中心 (那里必定是回廊)
	if (Plan.Stairs.Num() > 0)
	{
		static const int32 StairDX[4] = { 0, 1, 0, -1 };
		static const int32 StairDY[4] = { 1, 0, -1, 0 };
		const FArchStair& S = Plan.Stairs[0];
		const int32 D = FMath::Clamp(S.Dir, 0, 3);
		const int32 TopX = S.X + StairDX[D];
		const int32 TopY = S.Y + StairDY[D];
		LastResult.UpperNavProbe = FVector(
			(static_cast<float>(TopX) + 0.5f) * CellSize,
			(static_cast<float>(GridH - TopY) - 0.5f) * CellSize,
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

