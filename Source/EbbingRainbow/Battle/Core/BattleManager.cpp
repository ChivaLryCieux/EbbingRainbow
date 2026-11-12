// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Core/BattleManager.h"
#include "Core/ERLog.h"
#include "Battle/Core/UBattleContext.h"
#include "Battle/AI/UBattleAIDirector.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleEventHub.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBattleManager::UBattleManager()
{
}

void UBattleManager::InitializeBattle(ABattleGameMode* InGameMode, const TArray<ABattleUnit*>& Players, const TArray<ABattleUnit*>& Enemies, int32 BattleSeed)
{
	GameModeRef = InGameMode;
	AllUnits.Reset();
	AllUnits.Append(Players);
	AllUnits.Append(Enemies);
	++UnitsRevision;

	// 登记到战斗上下文: Resolver/TerrainQuery/自检都经注册表取单位,
	// 不再全场 TActorIterator
	if (UWorld* W = InGameMode ? InGameMode->GetWorld() : nullptr)
	{
		if (UBattleContext* Ctx = UBattleContext::Get(W))
		{
			Ctx->SetManager(this);
		}
		AIDirector = NewObject<UBattleAIDirector>(this);
		AIDirector->Initialize(this, W);
	}

	// 种子化随机流: 同种子同操作序列 ⇒ 同结果 (设计文档 §8)
	Rng.Init(BattleSeed != 0 ? BattleSeed : 0x5F3759DF);

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

	// 收集存活单位: 回合开始生命周期 (RoundStart 状态跳动 + AP 重置)
	InitiativeQueue.Reset();
	for (ABattleUnit* Unit : AllUnits)
	{
		if (Unit && Unit->IsAlive())
		{
			Unit->ResetAP();
			if (UBattleSkillComponent* Comp = Unit->GetSkillComponent())
			{
				Comp->NotifyRoundStart();
			}
			Unit->RefreshLabel();
			InitiativeQueue.Add(Unit);
		}
	}

	// 按有效先攻降序排序 (状态可改先攻; 同先攻按稳定下标, 保证可复现)
	InitiativeQueue.Sort([this](const ABattleUnit& A, const ABattleUnit& B) {
		const UBattleSkillComponent* CompA = A.GetSkillComponent();
		const UBattleSkillComponent* CompB = B.GetSkillComponent();
		const float InitA = CompA ? CompA->GetEffectiveInitiative() : (float)A.Stats.Initiative;
		const float InitB = CompB ? CompB->GetEffectiveInitiative() : (float)B.Stats.Initiative;
		if (!FMath::IsNearlyEqual(InitA, InitB))
		{
			return InitA > InitB;
		}
		// 先攻相同按出场顺序: 比内存地址稳定 (可复现)
		return AllUnits.IndexOfByKey(const_cast<ABattleUnit*>(&A)) < AllUnits.IndexOfByKey(const_cast<ABattleUnit*>(&B));
	});

	CurrentQueueIndex = -1;
	++InitiativeRevision;
	OnRoundStarted.Broadcast(RoundIndex);
	if (UBattleEventHub* Hub = UBattleEventHub::Get(GameModeRef ? GameModeRef->GetWorld() : nullptr))
	{
		Hub->OnRoundStarted.Broadcast(RoundIndex);
	}
	OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("=== 第 %d 轮开始! (先攻队列共 %d 人) ==="), RoundIndex, InitiativeQueue.Num()));
	UE_LOG(LogERBattle, Log, TEXT("[Battle] === 第 %d 轮开始 (先攻队列 %d 人) ==="), RoundIndex, InitiativeQueue.Num());

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

	// 本轮所有人已行动完 -> 回合结束生命周期, 开始新一轮
	if (CurrentQueueIndex >= InitiativeQueue.Num())
	{
		EndRoundLifecycle();
		StartNewRound();
		return;
	}

	ActiveUnit = InitiativeQueue[CurrentQueueIndex];
	ActiveUnit->SetSelected(true);
	CurrentState = EBattleState::TurnActive;

	// 行动开始生命周期: 冷却快照 (本回合禁用判定) + 递减
	if (UBattleSkillComponent* Comp = ActiveUnit->GetSkillComponent())
	{
		Comp->NotifyTurnStart();
	}
	ActiveUnit->RefreshLabel();

	OnTurnStarted.Broadcast(ActiveUnit);
	const FString TeamName = (ActiveUnit->Team == EBattleTeam::Player) ? TEXT("我方") : TEXT("敌方");
	const UBattleSkillComponent* Comp = ActiveUnit->GetSkillComponent();
	const int32 EffectiveInit = FMath::RoundToInt(Comp ? Comp->GetEffectiveInitiative() : (float)ActiveUnit->Stats.Initiative);
	OnBattleStatusMessage.Broadcast(FString::Printf(TEXT(">> 轮到 [%s] (%s) 行动! AP: %d, 先攻: %d"),
		*ActiveUnit->Stats.DisplayName, *TeamName, ActiveUnit->AP, EffectiveInit));

	if (ActiveUnit->Team == EBattleTeam::Enemy)
	{
		TriggerEnemyAI();
	}
}

void UBattleManager::EndRoundLifecycle()
{
	UE_LOG(LogERBattle, Log, TEXT("[Battle] ---- 第 %d 轮结束: 状态跳动/时长递减 ----"), RoundIndex);

	// 回合结束: 全员 RoundEnd 状态跳动 (燃烧等 DoT) + 状态时长递减/到期
	for (ABattleUnit* Unit : AllUnits)
	{
		if (Unit && Unit->IsAlive())
		{
			if (UBattleSkillComponent* Comp = Unit->GetSkillComponent())
			{
				Comp->NotifyRoundEnd();
			}
			Unit->RefreshLabel();
		}
	}
	// 跳动可能致死 (燃烧结算), 立即判定胜负
	CheckGameOver();
}

void UBattleManager::EndCurrentTurn()
{
	if (CurrentState == EBattleState::BattleOver)
	{
		return;
	}

	if (AIDirector)
	{
		AIDirector->CancelPending();
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
		UE_LOG(LogERBattle, Log, TEXT("[BattleInput] RequestMove 拒绝: 状态=%d Unit=%s Active=%s alive=%d"),
			(int32)CurrentState, *GetNameSafe(Unit), *GetNameSafe(ActiveUnit), Unit ? Unit->IsAlive() : 0);
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
		if (const UBattleSkillComponent* Comp = Unit->GetSkillComponent())
		{
			if (const FBattleSkillSpec* Spec = Comp->GetSpec(SkillIndex))
			{
				if (const UBattleSkillData* Skill = Spec->SkillData)
				{
					const FString TargetName = Target ? Target->Stats.DisplayName : TEXT("落点");
					OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("[%s] 对 [%s] 施放了 [%s] (%s)!"),
						*Unit->Stats.DisplayName, *TargetName, *Skill->DisplayName.ToString(),
						GetDamageTypeName(Skill->Damage.DamageType)));
				}
			}
		}

		CheckGameOver();
		return true;
	}
	return false;
}

void UBattleManager::DebugKillAllEnemies()
{
	if (CurrentState == EBattleState::BattleOver)
	{
		return;
	}

	// 清掉待执行的 AI 决策, 否则它会在敌人死后再跑一次
	if (AIDirector)
	{
		AIDirector->CancelPending();
	}

	// 先收集再处决: 处决会广播 OnUnitDied -> CheckGameOver, 边遍历边改战斗状态容易踩到重入
	TArray<ABattleUnit*> Enemies;
	for (ABattleUnit* Unit : AllUnits)
	{
		if (Unit && Unit->IsAlive() && Unit->Team == EBattleTeam::Enemy)
		{
			Enemies.Add(Unit);
		}
	}

	UE_LOG(LogERBattle, Log, TEXT("[Battle] DEBUG 秒杀: 敌方 %d 个单位"), Enemies.Num());

	for (ABattleUnit* Enemy : Enemies)
	{
		Enemy->DebugKill();
	}

	CheckGameOver();
}

void UBattleManager::HandleUnitMoveFinished(ABattleUnit* Unit)
{
	if (CurrentState == EBattleState::ActionExecuting && Unit == ActiveUnit)
	{
		CurrentState = EBattleState::TurnActive;

		// 若当前为 AI 行动, 移动结束后继续后续决策 (如在射程内则攻击)
		if (ActiveUnit && ActiveUnit->Team == EBattleTeam::Enemy && AIDirector)
		{
			AIDirector->NotifyMoveFinished(ActiveUnit);
		}
	}
}

void UBattleManager::HandleUnitDied(ABattleUnit* Unit)
{
	++UnitsRevision;
	if (Unit)
	{
		OnBattleStatusMessage.Broadcast(FString::Printf(TEXT("!! [%s] 已阵亡!"), *Unit->Stats.DisplayName));
	}
	CheckGameOver();
}

void UBattleManager::CheckGameOver()
{
	// 已判定过胜负就不再重复广播（多个单位同一帧死亡会重复调用）
	if (CurrentState == EBattleState::BattleOver)
	{
		return;
	}

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
	if (AIDirector)
	{
		AIDirector->TriggerDecision(ActiveUnit);
	}
}
