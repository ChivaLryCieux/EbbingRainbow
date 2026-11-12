// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UBattleAIDirector.generated.h"

class ABattleUnit;
class UBattleManager;

/**
 * 敌方 AI 决策器 —— 从 UBattleManager 抽出的单职责对象。
 *
 * 职责: 敌方回合开始 → 找最近存活玩家 → 能打就放技能 → 打不到且有 AP 就移动 →
 * 都不能就结束回合。决策节奏由定时器驱动 (0.8s 首拍 / 0.6s 移动后续 / 0.7s 剩余 AP)。
 * 战斗管理器只负责把事件转发进来。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleAIDirector : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UBattleManager* InManager, UWorld* InWorld);

	/** 敌方回合开始 (延迟 0.8s 出第一拍) */
	void TriggerDecision(ABattleUnit* EnemyUnit);

	/** 移动结束后的后续决策 (0.6s) */
	void NotifyMoveFinished(ABattleUnit* Unit);

	/** 取消挂起决策 (回合结束/战斗结束/作弊清场) */
	void CancelPending();

private:
	UFUNCTION()
	void RunDecision();

	UPROPERTY(Transient)
	TObjectPtr<UBattleManager> Manager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWorld> World = nullptr;

	FTimerHandle DecisionTimer;
};
