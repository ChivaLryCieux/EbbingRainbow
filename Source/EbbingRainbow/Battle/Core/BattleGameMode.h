// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Battle/Core/BattleTypes.h"
#include "WFCGenerationTypes.h"
#include "BattleGameMode.generated.h"

class ABattleArena;
class ABattleUnit;
class UBattleManager;
class UWFC3DStyleTable;

/** 回合制战斗 GameMode: 负责在开局创建竞技场、按出生点生成双方单位并初始化战斗管理器 */
UCLASS()
class EBBINGRAINBOW_API ABattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABattleGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/** 竞技场类 (默认 ABattleArena,可在蓝图中替换以调整参数) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<ABattleArena> ArenaClass;

	/** 单位类 (默认 ABattleUnit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<ABattleUnit> UnitClass;

	/** 竞技场原点 (WFC 网格 (0,0,0) 所在位置) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	FVector ArenaOrigin = FVector::ZeroVector;

	/** 当前进度对应的建筑风格（后续由 RunState / Act 映射写入） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	EWFCBuildingStyle ArenaStyle = EWFCBuildingStyle::Style0;

	/** 本场战斗的 WFC 规模（X/Y/Z 格数）。SciFi01 两层随机结构使用 18x14x2（约 72m x 56m x 6m） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	FIntVector ArenaScale = FIntVector(18, 14, 2);

	/** 风格表：把 ArenaStyle 映射到具体 WFC 生成器类。为空时回退到 SciFi01 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TObjectPtr<UWFC3DStyleTable> WFCStyleTable = nullptr;

	/** 我方 / 敌方名单 (每个条目对应一个出生点,按顺序) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TArray<FBattleUnitStats> PlayerRoster;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TArray<FBattleUnitStats> EnemyRoster;

	UFUNCTION(BlueprintPure, Category = "Battle")
	ABattleArena* GetArena() const { return Arena; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	UBattleManager* GetBattleManager() const { return BattleManager; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	const TArray<ABattleUnit*>& GetPlayerUnits() const { return PlayerUnits; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	const TArray<ABattleUnit*>& GetEnemyUnits() const { return EnemyUnits; }

	/** 竞技场与单位全部就绪 */
	DECLARE_MULTICAST_DELEGATE(FOnBattleSetupComplete);
	FOnBattleSetupComplete OnBattleSetupComplete;

	bool IsBattleSetupComplete() const { return bSetupComplete; }

private:
	UFUNCTION()
	void HandleArenaGenerated(bool bSuccess);

	void SpawnTeam(const TArray<FBattleUnitStats>& Roster, const TArray<FVector>& Points, EBattleTeam Team, TArray<ABattleUnit*>& OutUnits);

	UPROPERTY(Transient)
	TObjectPtr<ABattleArena> Arena = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleManager> BattleManager = nullptr;

	TArray<ABattleUnit*> PlayerUnits;
	TArray<ABattleUnit*> EnemyUnits;
	bool bSetupComplete = false;
};
