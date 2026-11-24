// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "BattleEventHub.generated.h"

class ABattleUnit;
class UBattleSkillData;
class UBattleStatusEffect;

/**
 * 战斗事件总线 (顶层设计 §5) —— 结算管线是事件源, 一切旁观者只订阅不侵入。
 *
 * UI (飘字/卡牌/状态图标)、AI (学习反应表)、未来的卡牌遗物/成就统计
 * 全部挂在这些委托上。全部为非动态多播: Slate/C++ 订阅不受 UFUNCTION 限制。
 */

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattleSkillActivated, ABattleUnit* /*Caster*/, const UBattleSkillData* /*Skill*/);
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnBattleSkillCastStarted, ABattleUnit* /*Caster*/, const UBattleSkillData* /*Skill*/, ABattleUnit* /*Target*/, const FVector& /*TargetLocation*/, float /*HitDelay*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnBattleSkillAreaTriggered, ABattleUnit* /*Caster*/, const UBattleSkillData* /*Skill*/, const FVector& /*TargetLocation*/, float /*Radius*/);
DECLARE_MULTICAST_DELEGATE_SixParams(FOnBattleChainArcTriggered, ABattleUnit* /*Caster*/, ABattleUnit* /*FromUnit*/, ABattleUnit* /*ToUnit*/, const UBattleSkillData* /*Skill*/, int32 /*JumpIndex*/, float /*Delay*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleHitResolved, const FBattleHitResult& /*Hit*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleDamageResolved, const FBattleDamageEvent& /*Damage*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBattleStatusChanged, ABattleUnit* /*Unit*/, const UBattleStatusEffect* /*Status*/, bool /*bApplied*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleUnitDied, ABattleUnit* /*Unit*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleRoundStarted, int32 /*RoundIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleLog, const FString& /*Message*/);

UCLASS()
class EBBINGRAINBOW_API UBattleEventHub : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 便捷取用 (世界不存在时返回 nullptr) */
	static UBattleEventHub* Get(const UWorld* World);

	FOnBattleSkillActivated OnSkillActivated;
	FOnBattleSkillCastStarted OnSkillCastStarted;
	FOnBattleSkillAreaTriggered OnSkillAreaTriggered;
	FOnBattleChainArcTriggered OnChainArcTriggered;
	FOnBattleHitResolved OnHitResolved;
	FOnBattleDamageResolved OnDamageResolved;
	FOnBattleStatusChanged OnStatusChanged;
	FOnBattleUnitDied OnUnitDied;
	FOnBattleRoundStarted OnRoundStarted;
	FOnBattleLog OnLog;
};