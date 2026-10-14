// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Arena/BattleArena.h"
#include "WFC3DGenerator.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogBattleArena);

ABattleArena::ABattleArena()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ABattleArena::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoGenerateOnBeginPlay)
	{
		GenerateArena();
	}
}

FBox ABattleArena::GetArenaBounds() const
{
	const FVector Origin = GetActorLocation();
	const FVector Size(GridCells.X * CellWorldSize, GridCells.Y * CellWorldSize, GridCells.Z * CellWorldSize);
	return FBox(Origin, Origin + Size);
}

AWFC3DGenerator* ABattleArena::EnsureGenerator()
{
	if (Generator && IsValid(Generator))
	{
		return Generator;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float Scale = CellWorldSize / 100.0f; // 内置 Dungeon 预设单元格为 100cm
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Generator = World->SpawnActor<AWFC3DGenerator>(
		AWFC3DGenerator::StaticClass(),
		FTransform(FRotator::ZeroRotator, GetActorLocation(), FVector(Scale)),
		Params);

	if (Generator)
	{
		Generator->bAutoGenerateOnBeginPlay = false;
		Generator->SetupDungeonPreset();
		Generator->GridSize = GridCells; // 预设默认 8x8x1,覆盖为竞技场尺寸
		Generator->AirWeight = 1.0f;
		if (Generator->ModuleLibrary)
		{
			for (FWFC3DModuleDefinition& Mod : Generator->ModuleLibrary->Modules)
			{
				if (Mod.ModuleId == FName(TEXT("Room_Floor")))
				{
					Mod.Weight = 6.0f; // 提升开阔地面权重
				}
			}
		}
	}
	return Generator;
}

bool ABattleArena::GenerateArena()
{
	bArenaReady = false;
	PlayerSpawnPoints.Reset();
	EnemySpawnPoints.Reset();

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogBattleArena, Error, TEXT("No navigation system. Cannot validate arena."));
		OnArenaGenerated.Broadcast(false);
		return false;
	}

	if (!EnsureGenerator())
	{
		UE_LOG(LogBattleArena, Error, TEXT("Failed to create WFC generator."));
		OnArenaGenerated.Broadcast(false);
		return false;
	}

	const int32 StartSeed = (BaseSeed != 0) ? BaseSeed : FMath::RandRange(1, MAX_int32 / 2);

	for (int32 Attempt = 0; Attempt < MaxGenerationAttempts; ++Attempt)
	{
		const int32 Seed = StartSeed + Attempt;
		if (TryBuildAndValidate(NavSys, Seed))
		{
			UsedSeed = Seed;
			bArenaReady = true;
			UE_LOG(LogBattleArena, Log, TEXT("Arena ready. Seed=%d, attempts=%d, player spawns=%d, enemy spawns=%d"),
				Seed, Attempt + 1, PlayerSpawnPoints.Num(), EnemySpawnPoints.Num());

			if (bDrawDebugSpawns)
			{
				for (const FVector& P : PlayerSpawnPoints)
				{
					DrawDebugSphere(GetWorld(), P + FVector(0, 0, 60), 50.0f, 12, FColor::Blue, true);
				}
				for (const FVector& P : EnemySpawnPoints)
				{
					DrawDebugSphere(GetWorld(), P + FVector(0, 0, 60), 50.0f, 12, FColor::Red, true);
				}
			}
			OnArenaGenerated.Broadcast(true);
			return true;
		}
		UE_LOG(LogBattleArena, Log, TEXT("Arena attempt %d (seed %d) rejected."), Attempt + 1, Seed);
	}

	UE_LOG(LogBattleArena, Error, TEXT("Arena generation failed after %d attempts."), MaxGenerationAttempts);
	OnArenaGenerated.Broadcast(false);
	return false;
}

bool ABattleArena::TryBuildAndValidate(UNavigationSystemV1* NavSys, int32 Seed)
{
	Generator->RandomSeed = Seed;
	Generator->Generate3DSpace();
	if (!Generator->WasLastGenerationSuccessful())
	{
		UE_LOG(LogBattleArena, Warning, TEXT("WFC solve failed for seed %d."), Seed);
		return false;
	}

	// 同步重建导航网格,保证后续查询基于最新地形
	NavSys->Build();
	if (!NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		UE_LOG(LogBattleArena, Error, TEXT("No NavData. Add a NavMeshBoundsVolume covering the arena (and set Runtime Generation = Dynamic)."));
		return false;
	}

	const FBox Bounds = GetArenaBounds();
	const float SearchRadius = Bounds.GetSize().Size2D() * 2.0f;
	constexpr float CellDedupe = 200.0f;

	// 多个锚点中选可达面积最大的一块,避开孤立小口袋
	TArray<FVector> BestPoints;
	int32 BestCells = 0;
	for (int32 AnchorTry = 0; AnchorTry < 8; ++AnchorTry)
	{
		FNavLocation Anchor;
		if (!NavSys->GetRandomPoint(Anchor))
		{
			continue;
		}

		TArray<FVector> Points;
		TSet<FIntPoint> Cells;
		for (int32 i = 0; i < ReachabilitySamples; ++i)
		{
			FNavLocation P;
			if (NavSys->GetRandomReachablePointInRadius(Anchor.Location, SearchRadius, P))
			{
				Points.Add(P.Location);
				Cells.Add(FIntPoint(FMath::FloorToInt(P.Location.X / CellDedupe), FMath::FloorToInt(P.Location.Y / CellDedupe)));
			}
		}
		if (Cells.Num() > BestCells)
		{
			BestCells = Cells.Num();
			BestPoints = MoveTemp(Points);
		}
	}

	if (BestCells < MinReachableCells)
	{
		UE_LOG(LogBattleArena, Log, TEXT("Seed %d: reachable area too small (%d cells < %d)."), Seed, BestCells, MinReachableCells);
		return false;
	}

	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	for (const FVector& P : BestPoints)
	{
		MinX = FMath::Min(MinX, (float)P.X);
		MaxX = FMath::Max(MaxX, (float)P.X);
	}
	const float Span = MaxX - MinX;
	if (Span < Bounds.GetSize().X * MinReachableSpanFraction)
	{
		UE_LOG(LogBattleArena, Log, TEXT("Seed %d: reachable X span too small (%.0f)."), Seed, Span);
		return false;
	}

	FRandomStream Rand(Seed);
	auto PickSpawns = [&](TFunctionRef<bool(const FVector&)> InZone, TArray<FVector>& Out) -> bool
	{
		TArray<FVector> Candidates = BestPoints.FilterByPredicate(InZone);
		for (int32 i = Candidates.Num() - 1; i > 0; --i)
		{
			Candidates.Swap(i, Rand.RandRange(0, i));
		}
		for (const FVector& C : Candidates)
		{
			bool bFarEnough = true;
			for (const FVector& Existing : Out)
			{
				if (FVector::Dist2D(C, Existing) < MinSpawnSpacing)
				{
					bFarEnough = false;
					break;
				}
			}
			if (bFarEnough)
			{
				Out.Add(C);
				if (Out.Num() >= UnitsPerTeam)
				{
					return true;
				}
			}
		}
		return false;
	};

	const float PlayerMaxX = MinX + Span * SpawnZoneFraction;
	const float EnemyMinX = MaxX - Span * SpawnZoneFraction;

	TArray<FVector> NewPlayer;
	TArray<FVector> NewEnemy;
	const bool bPlayerOk = PickSpawns([&](const FVector& P) { return P.X <= PlayerMaxX; }, NewPlayer);
	const bool bEnemyOk = PickSpawns([&](const FVector& P) { return P.X >= EnemyMinX; }, NewEnemy);
	if (!bPlayerOk || !bEnemyOk)
	{
		UE_LOG(LogBattleArena, Log, TEXT("Seed %d: not enough spawn points (player=%d, enemy=%d)."), Seed, NewPlayer.Num(), NewEnemy.Num());
		return false;
	}

	PlayerSpawnPoints = MoveTemp(NewPlayer);
	EnemySpawnPoints = MoveTemp(NewEnemy);
	return true;
}
