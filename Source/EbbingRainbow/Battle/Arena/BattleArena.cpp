// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Arena/BattleArena.h"
#include "WFC3DArenaHost.h"
#include "WFC3DStyleTable.h"
#include "Arch/WFC3DArenaGenerator_Atrium.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
	const FVector Cell = UsedCellWorldSize.IsNearlyZero() ? FVector(CellWorldSize) : UsedCellWorldSize;
	const FIntVector BoundsCells = (UsedGridCells.X > 0) ? UsedGridCells : GridCells;
	const FVector Size(BoundsCells.X * Cell.X, BoundsCells.Y * Cell.Y, BoundsCells.Z * Cell.Z);
	return FBox(Origin, Origin + Size);
}

AWFC3DArenaHost* ABattleArena::EnsureHost()
{
	if (ArenaHost && IsValid(ArenaHost))
	{
		return ArenaHost;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 格子世界尺寸由风格生成器决定，Host/生成器本身不允许再被 Actor 缩放
	FTransform HostTransform = GetActorTransform();
	HostTransform.SetScale3D(FVector::OneVector);

	ArenaHost = World->SpawnActor<AWFC3DArenaHost>(
		AWFC3DArenaHost::StaticClass(),
		HostTransform,
		Params);

	if (ArenaHost)
	{
		ArenaHost->StyleTable = StyleTable;
		ArenaHost->FallbackGeneratorClass = AWFC3DArenaGenerator_Atrium::StaticClass();
		ArenaHost->bAutoGenerateOnBeginPlay = false;

		// 编辑器预览参数与竞技场默认保持一致
		ArenaHost->PreviewRequest.Style = Style;
		ArenaHost->PreviewRequest.Scale = GridCells;
	}

	return ArenaHost;
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

	if (!EnsureHost())
	{
		UE_LOG(LogBattleArena, Error, TEXT("Failed to create WFC arena host."));
		OnArenaGenerated.Broadcast(false);
		return false;
	}

	int32 StartSeed = (BaseSeed != 0) ? BaseSeed : FMath::RandRange(1, MAX_int32 / 2);
	// 命令行 -ArenaSeed=N 覆盖, 便于可复现的离线截图/诊断
	FParse::Value(FCommandLine::Get(), TEXT("ArenaSeed="), StartSeed);

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
	FWFCGenerationRequest Request;
	Request.Style = Style;
	Request.Scale = GridCells;
	Request.Seed = Seed;

	if (!ArenaHost->Generate(Request))
	{
		UE_LOG(LogBattleArena, Warning, TEXT("WFC generation failed for seed %d (style %d)."),
			Seed, static_cast<int32>(Style));
		return false;
	}

	const FWFCGenerationResult& GenResult = ArenaHost->GetLastResult();
	UsedCellWorldSize = GenResult.CellWorldSize.IsNearlyZero()
		? FVector(CellWorldSize)
		: GenResult.CellWorldSize;
	UsedGridCells = (GenResult.UsedScale.X > 0) ? GenResult.UsedScale : GridCells;

	// 生成器可能忽略请求的 GridCells（Atrium 固定 19x19），把关卡里的
	// NavMeshBoundsVolume 同步放大，避免出生/可达性只覆盖旧版 18x14 区域。
	const FBox NavBoundsBox = GetArenaBounds();
	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
	{
		ANavMeshBoundsVolume* NavVolume = *It;
		const FVector Center = NavBoundsBox.GetCenter();
		const FVector Extent = NavBoundsBox.GetExtent() + FVector(1000.0f);
		// The volume is moved/scaled at runtime, so its brush must be Movable;
		// otherwise PIE logs a mobility warning and the message log covers the
		// viewport on every start.
		if (USceneComponent* Root = NavVolume->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
		NavVolume->SetActorLocation(FVector(Center.X, Center.Y, 0.0f));
		NavVolume->SetActorScale3D(FVector(Extent.X / 100.0f, Extent.Y / 100.0f, 10.0f));
		if (UBrushComponent* Brush = NavVolume->GetBrushComponent())
		{
			Brush->SetMobility(EComponentMobility::Movable);
			Brush->UpdateBounds();
			Brush->MarkRenderStateDirty();
		}
		NavSys->OnNavigationBoundsUpdated(NavVolume);
	}

	// 同步重建导航网格,保证后续查询基于最新地形
	NavSys->Build();
	// Recast 的 tile 烘焙是异步的,编辑器失焦时可能拖数分钟;
	// EnsureBuildCompletion 强制同步完成,否则下面的探测会撞上未建好的 tile。
	if (ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		NavData->EnsureBuildCompletion();
	}
	if (!NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		UE_LOG(LogBattleArena, Error, TEXT("No NavData. Add a NavMeshBoundsVolume covering the arena (and set Runtime Generation = Dynamic)."));
		return false;
	}

	// Mezzanine connectivity probe. Graph layouts are single-level for now and
	// report a zero probe; cross layouts report the L1 walkway beside a stair.
	if (ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()))
	{
		UE_LOG(LogBattleArena, Log,
			TEXT("Nav config: step=%.1f radius=%.1f height=%.1f cell=%.1f cellH=%.1f slope=%.1f"),
			Recast->AgentMaxStepHeight, Recast->AgentRadius, Recast->AgentHeight,
			Recast->CellSize, Recast->CellHeight, Recast->AgentMaxSlope);
	}

	if (!GenResult.UpperNavProbe.IsNearlyZero())
	{
		const FVector UpperProbe = GenResult.UpperNavProbe;
		FNavLocation UpperNav;
		const bool bUpperNav = NavSys->ProjectPointToNavigation(
			UpperProbe, UpperNav, FVector(200.0f, 200.0f, 200.0f));
		UE_LOG(LogBattleArena, Log, TEXT("Upper mezzanine nav: %s at %s"),
			bUpperNav ? TEXT("ok") : TEXT("MISSING"), *UpperNav.Location.ToString());

		// 楼梯中段是否有 navmesh (没有就说明台阶碰撞没被烘焙进去)
		if (!GenResult.StairMidProbe.IsNearlyZero())
		{
			FNavLocation StairNav;
			const bool bStairNav = NavSys->ProjectPointToNavigation(
				GenResult.StairMidProbe, StairNav, FVector(120.0f, 120.0f, 160.0f));
			UE_LOG(LogBattleArena, Log, TEXT("Stair mid nav: %s at %s (probe %s)"),
				bStairNav ? TEXT("ok") : TEXT("MISSING"),
				*StairNav.Location.ToString(), *GenResult.StairMidProbe.ToString());
		}

		// Full connectivity probe from the nave floor to the reported L1 walkway.
		if (UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
			this, FVector(3800.0f, 6000.0f, 20.0f), UpperProbe))
		{
			UE_LOG(LogBattleArena, Log,
				TEXT("Mezzanine path: valid=%s partial=%s length=%.0f points=%d"),
				Path->IsValid() ? TEXT("yes") : TEXT("no"),
				Path->IsPartial() ? TEXT("yes") : TEXT("no"),
				Path->GetPathLength(), Path->PathPoints.Num());
			for (int32 PointIndex = 0; PointIndex < Path->PathPoints.Num(); ++PointIndex)
			{
				UE_LOG(LogBattleArena, Log, TEXT("  path[%d] = %s"),
					PointIndex, *Path->PathPoints[PointIndex].ToString());
			}
		}

		// 逐楼梯落点验证: 所有夹层落点都应从地面可达。
		// 编辑器 nav tile 烘焙滞后于生成,先立即测一轮,未全通则用定时器每 2s 重试。
		const TArray<FVector> LandingProbes = GenResult.LandingProbes;
		auto ProbeOnce = [this, NavSys, &LandingProbes](const TCHAR* Tag) -> int32
		{
			int32 Ok = 0;
			for (int32 LIdx = 0; LIdx < LandingProbes.Num(); ++LIdx)
			{
				UNavigationPath* LPath = NavSys->FindPathToLocationSynchronously(
					this, FVector(3800.0f, 6000.0f, 20.0f), LandingProbes[LIdx]);
				const bool bOk = LPath && LPath->IsValid() && !LPath->IsPartial();
				if (bOk) { ++Ok; }
				else
				{
					UE_LOG(LogBattleArena, Log, TEXT("  landing[%d] at %s: UNREACHABLE (%s)"),
						LIdx, *LandingProbes[LIdx].ToString(), Tag);
				}
			}
			return Ok;
		};
		if (LandingProbes.Num() > 0)
		{
			int32 Ok = ProbeOnce(TEXT("initial"));
			UE_LOG(LogBattleArena, Log, TEXT("Landings reachable: %d/%d"),
				Ok, LandingProbes.Num());
			if (Ok < LandingProbes.Num())
			{
				TWeakObjectPtr<UNavigationSystemV1> WeakNav = NavSys;
				TWeakObjectPtr<ABattleArena> WeakSelf(this);
				TSharedPtr<int32> Tries = MakeShared<int32>(0);
				// 编辑器里 nav tile 烘焙滞后于生成且无法同步等完,用 FTicker 轮询
				// GetNumRemainingBuildTasks,队列清空后再做决定性的可达性报告。
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
					[WeakSelf, WeakNav, LandingProbes, Tries](float) -> bool
					{
						ABattleArena* Self = WeakSelf.Get();
						UNavigationSystemV1* NS = WeakNav.Get();
						if (!NS || !Self) { return false; }
						++(*Tries);
						// 编辑器 nav 烘焙不进 build-task 队列且按时间片慢速推进,
						// 放大时间片配额并同步泵完成;全通即提前收敛
						if (*Tries == 1)
						{
							NS->GetMutableNavRegenTimeSliceManager().SetMaxTimeSliceDuration(30.0);
						}
						if (ANavigationData* D = NS->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
						{
							D->EnsureBuildCompletion();
						}
						int32 Ok = 0;
						for (const FVector& LP : LandingProbes)
						{
							UNavigationPath* P = NS->FindPathToLocationSynchronously(
								Self, FVector(3800.0f, 6000.0f, 20.0f), LP);
							if (P && P->IsValid() && !P->IsPartial()) { ++Ok; }
						}
						const bool bDone = (Ok == LandingProbes.Num()) || (*Tries) >= 300;
						if (bDone || (*Tries % 30) == 0)
						{
							UE_LOG(LogBattleArena, Log,
								TEXT("Landings reachable (deferred, try %d): %d/%d%s"),
								*Tries, Ok, LandingProbes.Num(), bDone ? TEXT(" FINAL") : TEXT(""));
						}
						return !bDone;
					}), 2.0f);
			}
		}
	}
	else
	{
		UE_LOG(LogBattleArena, Log, TEXT("No upper level in this layout (room graph)."));
	}

	const FBox Bounds = GetArenaBounds();
	const float SearchRadius = Bounds.GetSize().Size2D();
	const float CellDedupe = FMath::Max(50.0f, static_cast<float>(UsedCellWorldSize.X));

	// 多个锚点中选可达面积最大的一块,避开孤立小口袋
	TArray<FVector> BestPoints;
	int32 BestCells = 0;
	for (int32 AnchorTry = 0; AnchorTry < 8; ++AnchorTry)
	{
		FNavLocation Anchor;
		if (!NavSys->GetRandomPoint(Anchor) || !Bounds.IsInsideOrOn(Anchor.Location))
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
				if (!Bounds.IsInsideOrOn(P.Location))
				{
					continue;
				}
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
