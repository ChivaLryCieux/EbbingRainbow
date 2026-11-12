// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/AI/UBattleAIDirector.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UBattleAIDirector::Initialize(UBattleManager* InManager, UWorld* InWorld)
{
	Manager = InManager;
	World = InWorld;
}

void UBattleAIDirector::TriggerDecision(ABattleUnit* EnemyUnit)
{
	if (!EnemyUnit || !EnemyUnit->IsAlive() || EnemyUnit->Team != EBattleTeam::Enemy)
	{
		return;
	}
	if (World)
	{
		World->GetTimerManager().SetTimer(DecisionTimer, this, &UBattleAIDirector::RunDecision, 0.8f, false);
	}
}

void UBattleAIDirector::NotifyMoveFinished(ABattleUnit* Unit)
{
	if (Unit && Unit->Team == EBattleTeam::Enemy && World)
	{
		World->GetTimerManager().SetTimer(DecisionTimer, this, &UBattleAIDirector::RunDecision, 0.6f, false);
	}
}

void UBattleAIDirector::CancelPending()
{
	if (World)
	{
		World->GetTimerManager().ClearTimer(DecisionTimer);
	}
}

void UBattleAIDirector::RunDecision()
{
	if (!Manager || !World)
	{
		return;
	}

	ABattleUnit* ActiveUnit = Manager->GetActiveUnit();
	if (Manager->GetBattleState() == EBattleState::BattleOver
		|| !ActiveUnit || !ActiveUnit->IsAlive() || ActiveUnit->Team != EBattleTeam::Enemy)
	{
		Manager->EndCurrentTurn();
		return;
	}

	// 查找最近的存活玩家
	ABattleUnit* ClosestPlayer = nullptr;
	float MinDistSq = TNumericLimits<float>::Max();

	for (ABattleUnit* Unit : Manager->GetAllUnits())
	{
		if (Unit && Unit->IsAlive() && Unit->Team == EBattleTeam::Player)
		{
			const float DistSq = FVector::DistSquared2D(ActiveUnit->GetActorLocation(), Unit->GetActorLocation());
			if (DistSq < MinDistSq)
			{
				MinDistSq = DistSq;
				ClosestPlayer = Unit;
			}
		}
	}

	if (!ClosestPlayer)
	{
		Manager->EndCurrentTurn();
		return;
	}

	const float Distance = FMath::Sqrt(MinDistSq);
	const TArray<FBattleSkillSpec>& Skills = ActiveUnit->GetSkillSpecs();

	// 1. 优先尝试施放可达且 AP 足够的技能 (先对最近敌人, 自 buff/自疗类再对自身)
	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		FString FailReason;
		ABattleUnit* SkillTarget = nullptr;
		if (ActiveUnit->CanUseSkill(i, ClosestPlayer, FailReason))
		{
			SkillTarget = ClosestPlayer;
		}
		else if (ActiveUnit->CanUseSkill(i, ActiveUnit, FailReason))
		{
			SkillTarget = ActiveUnit;
		}

		if (SkillTarget)
		{
			Manager->RequestSkill(ActiveUnit, i, SkillTarget);

			// 攻击完后如果还有 AP, 延迟 0.7 秒再思考一次
			if (ActiveUnit->IsAlive() && ActiveUnit->AP >= 1 && Manager->GetBattleState() != EBattleState::BattleOver)
			{
				World->GetTimerManager().SetTimer(DecisionTimer, this, &UBattleAIDirector::RunDecision, 0.7f, false);
				return;
			}
			Manager->EndCurrentTurn();
			return;
		}
	}

	// 2. 如果技能打不到但有足够移动 AP, 朝目标移动
	if (ActiveUnit->AP >= 1)
	{
		const FVector TargetPos = ClosestPlayer->GetActorLocation();
		if (Manager->RequestMove(ActiveUnit, TargetPos))
		{
			// 移动开始, 等待 Manager 把移动完成事件转回来触发下一阶段决策
			return;
		}
	}

	// 3. 既不能攻击也不能移动, 结束回合
	Manager->EndCurrentTurn();
}
