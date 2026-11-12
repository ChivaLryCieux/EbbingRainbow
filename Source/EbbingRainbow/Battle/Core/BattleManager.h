// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Battle/Core/BattleTypes.h"
#include "Battle/Skills/BattleRandomStream.h"
#include "BattleManager.generated.h"

class ABattleUnit;
class ABattleGameMode;
class UBattleAIDirector;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnStarted, ABattleUnit*, ActiveUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStarted, int32, RoundNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleFinished, bool, bPlayerWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleStatusMessage, const FString&, Message);

/**
 * 回合制战斗管理器:
 * 负责先攻队列排序 (按有效先攻降序)、回合切换、状态/冷却回合生命周期、
 * 敌我胜负检测与 AI 行动分发。
 *
 * 随机数纪律 (设计文档 §8): 本类持有全战斗唯一随机流, 种子 = f(Run 种子, 遭遇种子);
 * 任何战斗判定不得使用 FMath::Rand 系列。
 */
UCLASS(BlueprintType)
class EBBINGRAINBOW_API UBattleManager : public UObject
{
	GENERATED_BODY()

public:
	UBattleManager();

	void InitializeBattle(ABattleGameMode* InGameMode, const TArray<ABattleUnit*>& Players, const TArray<ABattleUnit*>& Enemies, int32 BattleSeed = 0);

	// ---------- 战斗随机流 ----------

	/** 全战斗唯一随机源 (命中/暴击/浮动/AI 抖动都从这里出) */
	FBattleRandomStream& GetRng() { return Rng; }

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

	/** 调试用: 秒杀场上所有敌人并立即结算胜负 (战斗界面作弊按钮) */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void DebugKillAllEnemies();

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

	/** 先攻队列修订号: 每轮重建队列时 +1 (UI 对比免每帧哈希遍历) */
	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetInitiativeRevision() const { return InitiativeRevision; }

	/** 单位存亡修订号: 初始化/有人阵亡时 +1 */
	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetUnitsRevision() const { return UnitsRevision; }

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

	/** 回合结束: 全员 RoundEnd 状态跳动 + 时长递减 */
	void EndRoundLifecycle();

	UFUNCTION()
	void HandleUnitDied(ABattleUnit* Unit);

	UFUNCTION()
	void HandleUnitMoveFinished(ABattleUnit* Unit);

	void CheckGameOver();
	void TriggerEnemyAI();

	/** 敌方 AI 决策 (抽离为独立对象; 本类只转发触发/移动完成/取消事件) */
	UPROPERTY(Transient)
	TObjectPtr<UBattleAIDirector> AIDirector = nullptr;

	UPROPERTY()
	TObjectPtr<ABattleGameMode> GameModeRef = nullptr;

	TArray<ABattleUnit*> AllUnits;
	TArray<ABattleUnit*> InitiativeQueue;
	int32 CurrentQueueIndex = -1;
	int32 RoundIndex = 0;
	int32 InitiativeRevision = 0;
	int32 UnitsRevision = 0;

	UPROPERTY()
	TObjectPtr<ABattleUnit> ActiveUnit = nullptr;

	EBattleState CurrentState = EBattleState::Initializing;

	/** 全战斗唯一随机流 (种子化, 可复现) */
	FBattleRandomStream Rng;
};
