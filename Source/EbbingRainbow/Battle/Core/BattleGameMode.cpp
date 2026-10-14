// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Core/BattleGameMode.h"
#include "Battle/Arena/BattleArena.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/UI/BattleHUD.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

ABattleGameMode::ABattleGameMode()
{
	DefaultPawnClass = ATacticalCameraPawn::StaticClass();
	PlayerControllerClass = ABattlePlayerController::StaticClass();
	HUDClass = ABattleHUD::StaticClass();
	ArenaClass = ABattleArena::StaticClass();
	UnitClass = ABattleUnit::StaticClass();

	// 默认名单 (HP, 行动值, 速度)。数值可在编辑器中调整
	PlayerRoster = {
		FBattleUnitStats(TEXT("Warrior"), 40, 10.0f, 8),
		FBattleUnitStats(TEXT("Ranger"), 28, 12.0f, 14),
		FBattleUnitStats(TEXT("Mage"), 22, 10.0f, 10),
		FBattleUnitStats(TEXT("Cleric"), 26, 10.0f, 9),
	};
	EnemyRoster = {
		FBattleUnitStats(TEXT("Goblin"), 20, 10.0f, 12),
		FBattleUnitStats(TEXT("Orc"), 30, 10.0f, 6),
		FBattleUnitStats(TEXT("Archer"), 20, 12.0f, 11),
		FBattleUnitStats(TEXT("Shaman"), 22, 10.0f, 9),
	};
}

void ABattleGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// 在玩家 Pawn 出生之前创建竞技场 Actor (生成逻辑在其 BeginPlay 中执行)
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Arena = World->SpawnActor<ABattleArena>(ArenaClass ? ArenaClass.Get() : ABattleArena::StaticClass(),
			FTransform(ArenaOrigin), Params);
		if (Arena)
		{
			Arena->OnArenaGenerated.AddDynamic(this, &ABattleGameMode::HandleArenaGenerated);
		}
	}
}

void ABattleGameMode::StartPlay()
{
	Super::StartPlay();

	if (Arena && Arena->IsArenaReady() && !bSetupComplete)
	{
		HandleArenaGenerated(true);
	}
}

void ABattleGameMode::HandleArenaGenerated(bool bSuccess)
{
	if (!bSuccess)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red,
				TEXT("Arena generation FAILED. Check LogBattleArena (is a NavMeshBoundsVolume present?)"));
		}
		return;
	}

	SpawnTeam(PlayerRoster, Arena->GetPlayerSpawnPoints(), EBattleTeam::Player, PlayerUnits);
	SpawnTeam(EnemyRoster, Arena->GetEnemySpawnPoints(), EBattleTeam::Enemy, EnemyUnits);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
			FString::Printf(TEXT("Arena ready (seed %d): %d player units, %d enemy units"),
				Arena->GetUsedSeed(), PlayerUnits.Num(), EnemyUnits.Num()));
	}

	bSetupComplete = true;

	// 创建并启动战斗管理器
	BattleManager = NewObject<UBattleManager>(this, TEXT("BattleManager"));
	BattleManager->InitializeBattle(this, PlayerUnits, EnemyUnits);

	OnBattleSetupComplete.Broadcast();
}

void ABattleGameMode::SpawnTeam(const TArray<FBattleUnitStats>& Roster, const TArray<FVector>& Points, EBattleTeam Team, TArray<ABattleUnit*>& OutUnits)
{
	OutUnits.Reset();
	UWorld* World = GetWorld();
	const int32 Count = FMath::Min(Roster.Num(), Points.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		const ABattleUnit* CDO = UnitClass ? GetDefault<ABattleUnit>(UnitClass) : GetDefault<ABattleUnit>();
		const float HalfHeight = CDO->GetCapsuleHalfHeight();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// 敌我双方朝向对方
		const FRotator Facing(0.0f, Team == EBattleTeam::Player ? 0.0f : 180.0f, 0.0f);
		const FVector Loc = Points[i] + FVector(0.0f, 0.0f, HalfHeight + 5.0f);

		if (ABattleUnit* Unit = World->SpawnActor<ABattleUnit>(UnitClass ? UnitClass.Get() : ABattleUnit::StaticClass(), Loc, Facing, Params))
		{
			Unit->InitUnit(Roster[i], Team);
			OutUnits.Add(Unit);
		}
	}
}
