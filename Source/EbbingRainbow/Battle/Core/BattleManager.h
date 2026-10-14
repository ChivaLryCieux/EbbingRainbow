// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Battle/Core/BattleTypes.h"
#include "BattleManager.generated.h"

class ABattleUnit;
class ABattleGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnStarted, ABattleUnit*, ActiveUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStarted, int32, RoundNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleFinished, bool, bPlayerWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleStatusMessage, const FString&, Message);

/**
 * 回合制战斗管理器:
 * 负责先攻队列排序 (按 Speed 降序)、回合切换、敌我胜负检测与 AI 行动分发。
 */
UCLASS(BlueprintType)
class EBBINGRAINBOW_API UBattleManager : public UObject
{
	GENERATED_BODY()

public:
	UBattleManager();

	void InitializeBattle(ABattleGameMode* InGameMode, const TArray<ABattleUnit*>& Players, const TArray<ABattleUnit*>& Enemies);

	// ---------- 回合控制 ----------

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartNewRound();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndCurrentTurn();

	// ---------- 命令接口 ----------

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool RequestMove(ABattleUnit* Unit, const FVector& Destination);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool RequestSkill(ABattleUnit* Unit, int32 SkillIndex, ABattleUnit* Target);

	// ---------- 状态读取 ----------

	UFUNCTION(BlueprintPure, Category = "Battle")
	ABattleUnit* GetActiveUnit() const { return ActiveUnit; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetRoundIndex() const { return RoundIndex; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	EBattleState GetBattleState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	const TArray<ABattleUnit*>& GetInitiativeQueue() const { return InitiativeQueue; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	const TArray<ABattleUnit*>& GetAllUnits() const { return AllUnits; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetCurrentQueueIndex() const { return CurrentQueueIndex; }

	// ---------- 事件委托 ----------

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnTurnStarted OnTurnStarted;

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnRoundStarted OnRoundStarted;

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnBattleFinished OnBattleFinished;

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnBattleStatusMessage OnBattleStatusMessage;

private:
	void AdvanceToNextTurn();

	UFUNCTION()
	void HandleUnitDied(ABattleUnit* Unit);

	UFUNCTION()
	void HandleUnitMoveFinished(ABattleUnit* Unit);

	void CheckGameOver();
	void TriggerEnemyAI();

	UFUNCTION()
	void RunEnemyDecision();

	UPROPERTY()
	TObjectPtr<ABattleGameMode> GameModeRef = nullptr;

	TArray<ABattleUnit*> AllUnits;
	TArray<ABattleUnit*> InitiativeQueue;
	int32 CurrentQueueIndex = -1;
	int32 RoundIndex = 0;

	UPROPERTY()
	TObjectPtr<ABattleUnit> ActiveUnit = nullptr;

	EBattleState CurrentState = EBattleState::Initializing;
	FTimerHandle EnemyAITimerHandle;
};
