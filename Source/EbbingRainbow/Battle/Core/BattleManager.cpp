// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Core/BattleGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBattleManager::UBattleManager()
{
}

void UBattleManager::InitializeBattle(ABattleGameMode* InGameMode, const TArray<ABattleUnit*>& Players, const TArray<ABattleUnit*>& Enemies)
{
	GameModeRef = InGameMode;
	AllUnits.Reset();
	AllUnits.Append(Players);
	AllUnits.Append(Enemies);

	for (ABattleUnit* Unit : AllUnits)
	{
		if (Unit)
		{
			Unit->OnUnitDied.AddDynamic(this, &UBattleManager::HandleUnitDied);
			Unit->OnMoveFinished.AddDynamic(this, &UBattleManager::HandleUnitMoveFinished);
		}
	}

	RoundIndex = 0;
	StartNewRound();
}

void UBattleManager::StartNewRound()
{
	if (CurrentState == EBattleState::BattleOver)
	{
		return;
	}

	++RoundIndex;
	CurrentState = EBattleState::RoundStart;

	// 收集存活单位
	InitiativeQueue.Reset();
	for (ABattleUnit* Unit : AllUnits)
	{
		if (Unit && Unit->IsAlive())
		{
			Unit->ResetAP();
			InitiativeQueue.Add(Unit);
		}
	}

	// 按 Initiative 降序排序 (先攻队列)
	InitiativeQueue.Sort([](const ABattleUnit& A, const ABattleUnit& B) {
		if (A.Stats.Initiative != B.Stats.Initiative)
		{
			return A.Stats.Initiative > B.Stats.Initiative;
		}
		// 先攻相同则按内存地址确保稳定排序
		return &A > &B;
	});

	CurrentQueueIndex = -1;
	OnRoundStarted.Broadcast(RoundIndex);
	OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("=== 第 %d 轮开始! (先攻队列共 %d 人) ==="), RoundIndex, InitiativeQueue.Num()));

	AdvanceToNextTurn();
}

void UBattleManager::AdvanceToNextTurn()
{
	if (CurrentState == EBattleState::BattleOver)
	{
		return;
	}

	// 清理旧活动单位选中
	if (ActiveUnit)
	{
		ActiveUnit->SetSelected(false);
	}

	// 寻找队列中下一个存活单位
	++CurrentQueueIndex;
	while (CurrentQueueIndex < InitiativeQueue.Num())
	{
		ABattleUnit* Candidate = InitiativeQueue[CurrentQueueIndex];
		if (Candidate && Candidate->IsAlive())
		{
			break;
		}
		++CurrentQueueIndex;
	}

	// 本轮所有人已行动完 -> 开始新一轮
	if (CurrentQueueIndex >= InitiativeQueue.Num())
	{
		StartNewRound();
		return;
	}

	ActiveUnit = InitiativeQueue[CurrentQueueIndex];
	ActiveUnit->SetSelected(true);
	CurrentState = EBattleState::TurnActive;

	OnTurnStarted.Broadcast(ActiveUnit);
	const FString TeamName = (ActiveUnit->Team == EBattleTeam::Player) ? TEXT("我方") : TEXT("敌方");
	OnBattleStatusMessage.Broadcast(FString::Printf(TEXT(">> 轮到 [%s] (%s) 行动! AP: %d, 先攻: %d"),
		*ActiveUnit->Stats.DisplayName, *TeamName, ActiveUnit->AP, ActiveUnit->Stats.Initiative));

	if (ActiveUnit->Team == EBattleTeam::Enemy)
	{
		TriggerEnemyAI();
	}
}

void UBattleManager::EndCurrentTurn()
{
	if (CurrentState == EBattleState::BattleOver)
	{
		return;
	}

	if (UWorld* World = GameModeRef ? GameModeRef->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(EnemyAITimerHandle);
	}

	if (ActiveUnit)
	{
		ActiveUnit->SetSelected(false);
		if (ActiveUnit->IsMoving())
		{
			ActiveUnit->StopMovement();
		}
	}

	CurrentState = EBattleState::TurnActive;
	AdvanceToNextTurn();
}

bool UBattleManager::RequestMove(ABattleUnit* Unit, const FVector& Destination)
{
	if (CurrentState != EBattleState::TurnActive || Unit != ActiveUnit || !Unit || !Unit->IsAlive())
	{
		return false;
	}

	if (Unit->MoveTo(Destination))
	{
		CurrentState = EBattleState::ActionExecuting;
		return true;
	}
	return false;
}

bool UBattleManager::RequestSkill(ABattleUnit* Unit, int32 SkillIndex, ABattleUnit* Target)
{
	if (CurrentState != EBattleState::TurnActive || Unit != ActiveUnit || !Unit || !Unit->IsAlive())
	{
		return false;
	}

	if (Unit->ExecuteSkill(SkillIndex, Target))
	{
		const FBattleSkillDef& Skill = Unit->GetSkills()[SkillIndex];
		OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("[%s] 对 [%s] 施放了 [%s] (%s)!"),
			*Unit->Stats.DisplayName, *Target->Stats.DisplayName, *Skill.DisplayName, GetDamageTypeName(Skill.DamageType)));

		CheckGameOver();
		return true;
	}
	return false;
}

void UBattleManager::HandleUnitMoveFinished(ABattleUnit* Unit)
{
	if (CurrentState == EBattleState::ActionExecuting && Unit == ActiveUnit)
	{
		CurrentState = EBattleState::TurnActive;

		// 若当前为 AI 行动, 移动结束后继续后续决策 (如在射程内则攻击)
		if (ActiveUnit && ActiveUnit->Team == EBattleTeam::Enemy)
		{
			if (UWorld* World = GameModeRef ? GameModeRef->GetWorld() : nullptr)
			{
				World->GetTimerManager().SetTimer(EnemyAITimerHandle, this, &UBattleManager::RunEnemyDecision, 0.6f, false);
			}
		}
	}
}

void UBattleManager::HandleUnitDied(ABattleUnit* Unit)
{
	if (Unit)
	{
		OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("!! [%s] 已阵亡!"), *Unit->Stats.DisplayName));
	}
	CheckGameOver();
}

void UBattleManager::CheckGameOver()
{
	int32 LivingPlayers = 0;
	int32 LivingEnemies = 0;

	for (ABattleUnit* Unit : AllUnits)
	{
		if (Unit && Unit->IsAlive())
		{
			if (Unit->Team == EBattleTeam::Player)
			{
				++LivingPlayers;
			}
			else
			{
				++LivingEnemies;
			}
		}
	}

	if (LivingEnemies == 0)
	{
		CurrentState = EBattleState::BattleOver;
		OnBattleFinished.Broadcast(true);
		OnBattleStatusMessage.Broadcast(TEXT("============================\n  胜利! 敌方全灭!\n============================"));
	}
	else if (LivingPlayers == 0)
	{
		CurrentState = EBattleState::BattleOver;
		OnBattleFinished.Broadcast(false);
		OnBattleStatusMessage.Broadcast(TEXT("============================\n  失败! 我方全军覆没!\n============================"));
	}
}

void UBattleManager::TriggerEnemyAI()
{
	if (!ActiveUnit || !ActiveUnit->IsAlive() || ActiveUnit->Team != EBattleTeam::Enemy)
	{
		return;
	}

	if (UWorld* World = GameModeRef ? GameModeRef->GetWorld() : nullptr)
	{
		World->GetTimerManager().SetTimer(EnemyAITimerHandle, this, &UBattleManager::RunEnemyDecision, 0.8f, false);
	}
}

void UBattleManager::RunEnemyDecision()
{
	if (CurrentState == EBattleState::BattleOver || !ActiveUnit || !ActiveUnit->IsAlive() || ActiveUnit->Team != EBattleTeam::Enemy)
	{
		EndCurrentTurn();
		return;
	}

	// 查找最近的存活玩家
	ABattleUnit* ClosestPlayer = nullptr;
	float MinDistSq = TNumericLimits<float>::Max();

	for (ABattleUnit* Unit : AllUnits)
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
		EndCurrentTurn();
		return;
	}

	const float Distance = FMath::Sqrt(MinDistSq);
	const TArray<FBattleSkillDef>& Skills = ActiveUnit->GetSkills();

	// 1. 优先尝试施放可达且 AP 足够的技能
	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		FString FailReason;
		if (ActiveUnit->CanUseSkill(i, ClosestPlayer, FailReason))
		{
			RequestSkill(ActiveUnit, i, ClosestPlayer);

			// 攻击完后如果还有 AP, 延迟 0.7 秒再思考一次
			if (ActiveUnit->IsAlive() && ActiveUnit->AP >= 1 && CurrentState != EBattleState::BattleOver)
			{
				if (UWorld* World = GameModeRef ? GameModeRef->GetWorld() : nullptr)
				{
					World->GetTimerManager().SetTimer(EnemyAITimerHandle, this, &UBattleManager::RunEnemyDecision, 0.7f, false);
					return;
				}
			}
			EndCurrentTurn();
			return;
		}
	}

	// 2. 如果技能打不到但有足够移动 AP, 朝目标移动
	if (ActiveUnit->AP >= 1)
	{
		const FVector TargetPos = ClosestPlayer->GetActorLocation();
		if (RequestMove(ActiveUnit, TargetPos))
		{
			// 移动开始, 等待 HandleUnitMoveFinished 触发下一阶段决策
			return;
		}
	}

	// 3. 既不能攻击也不能移动, 结束回合
	EndCurrentTurn();
}
