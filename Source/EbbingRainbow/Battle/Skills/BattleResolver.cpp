// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleResolver.h"
#include "Core/ERLog.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "Battle/Skills/BattleSkillLibrary.h"
#include "Battle/Skills/BattleInteractionTable.h"
#include "Battle/Skills/BattleEventHub.h"
#include "Battle/Terrain/BattleTerrainQuery.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Core/UBattleContext.h"
#include "Battle/FX/BattleFloatingText.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace BattleResolverPrivate
{
	struct FTargetEntry
	{
		ABattleUnit* Unit = nullptr;
		float Scale = 1.0f;
		float Dist = 0.0f;
	};

	void SpawnText(UWorld* World, ABattleUnit* Unit, const FString& Text, FColor Color, float Size = 36.0f)
	{
		if (!World || !Unit)
		{
			return;
		}
		const FVector Loc = Unit->GetActorLocation() + FVector(0.0f, 0.0f, Unit->GetCapsuleHalfHeight() + 35.0f);
		ABattleFloatingText::SpawnFloatingText(World, Loc, Text, Color, Size);
	}

	/** 反应表消耗层数: 按标签找目标身上的状态并移除层数 */
	void ConsumeStatusStacks(UBattleSkillComponent* Comp, FGameplayTag Tag, int32 Stacks)
	{
		if (!Comp || !Tag.IsValid() || Stacks <= 0)
		{
			return;
		}
		for (const FBattleActiveStatus& Status : Comp->GetActiveStatuses())
		{
			if (Status.Def && Status.Def->StatusTag.MatchesTag(Tag))
			{
				Comp->RemoveStatus(Status.Def, Stacks);
				return;
			}
		}
	}
}

using namespace BattleResolverPrivate;

// ---------------- 随机源 ----------------

FBattleRandomStream* UBattleResolver::GetRng(const UWorld* World)
{
	// 战斗上下文统一入口: 管理器指针已在 InitializeBattle 时缓存,
	// 不再每次 Cast + GetBattleManager 逐级下钻
	if (UBattleContext* Ctx = UBattleContext::Get(World))
	{
		return Ctx->GetRng();
	}

	// 兜底流 (无上下文的孤立场景): 固定种子, 保证至少可复现
	static FBattleRandomStream Fallback;
	if (Fallback.InitialSeed == 0)
	{
		Fallback.Init(0x5EED1234);
	}
	return &Fallback;
}

// ---------------- 单段命中结算 (管线核心) ----------------

namespace
{
	void ResolveSingleHit(ABattleUnit* Caster, ABattleUnit* Target, UBattleSkillData* Skill,
		int32 SkillLevel, float DistanceScale, bool bIsArea)
	{
		if (!Caster || !Target || !Skill || !Target->IsAlive())
		{
			return;
		}

		UWorld* World = Caster->GetWorld();
		UBattleEventHub* Hub = UBattleEventHub::Get(World);
		FBattleRandomStream* Rng = UBattleResolver::GetRng(World);
		UBattleSkillComponent* CasterComp = Caster->GetSkillComponent();
		UBattleSkillComponent* TargetComp = Target->GetSkillComponent();
		if (!Rng || !CasterComp || !TargetComp)
		{
			return;
		}

		const EDamageType DamageType = Skill->Damage.DamageType;

		// 地形上下文: 视线/掩体/高低差/朝向 (设计文档 §7)
		UBattleTerrainQuery* Terrain = UBattleTerrainQuery::Get(World);
		const FBattleTerrainContext TerrainCtx = Terrain ? Terrain->Evaluate(Caster, Target) : FBattleTerrainContext();

		FBattleHitResult Hit;
		Hit.Attacker = Caster;
		Hit.Target = Target;
		Hit.DamageType = DamageType;
		Hit.HitChance = 1.0f;

		// ---- ① 命中判定 (真伤也躲得开: 躲开就是躲开) ----
		if (!Skill->IsAutoHit())
		{
			const float TargetEvasion = TargetComp->GetEffectiveEvasion() * TerrainCtx.GetEvasionMultiplier();
			const float HitChance = FMath::Clamp(
				Skill->Accuracy + CasterComp->GetEffectiveAccuracy() - TargetEvasion + TerrainCtx.GetHitChanceDelta(),
				0.05f, 0.99f);
			Hit.HitChance = HitChance;

			if (!Rng->Chance(HitChance))
			{
				Hit.bHit = false;
				Hit.bEvaded = true;
				if (Hub)
				{
					Hub->OnHitResolved.Broadcast(Hit);
				}
				SpawnText(World, Target, TEXT("闪避 (EVADED)"), FColor(120, 200, 255));
				return;
			}
		}
		Hit.bHit = true;

		// ---- ② 暴击判定 ----
		const float CritChance = FMath::Clamp(
			CasterComp->GetEffectiveCritChance() + Skill->CritChanceBonus + TerrainCtx.GetCritChanceBonus(), 0.0f, 1.0f);
		Hit.bCrit = Rng->Chance(CritChance);
		const float CritMult = Hit.bCrit ? (CasterComp->GetEffectiveCritMultiplier() + Skill->CritMultiplierBonus) : 1.0f;

		// ---- ③④ 伤害滚动: 基础 × 等级 × 浮动 × AoE 衰减 × 暴击 ----
		const float LevelScale = 1.0f + 0.10f * (float)FMath::Max(0, SkillLevel - 1);
		float Damage = Skill->Damage.BaseAmount * LevelScale * Rng->Variation(Skill->DamageVariance) * DistanceScale;
		if (Hit.bCrit)
		{
			Damage *= CritMult;
		}

		// ---- ⑤ 地形修正 (真实伤害跳过; AoE 免掩体但保留高低差) ----
		if (DamageType != EDamageType::TrueDamage)
		{
			Damage *= TerrainCtx.GetHeightDamageMultiplier();
			if (!bIsArea)
			{
				Damage *= TerrainCtx.GetCoverDamageMultiplier();
			}
		}
		Hit.RolledDamage = Damage;

		// ---- ⑥⑦ 反应表 (倍率在抗性前; 连锁在落地后) ----
		UBattleInteractionTable* Table = nullptr;
		if (UBattleSkillLibrary* Library = UBattleSkillLibrary::Get(World))
		{
			Table = Library->GetInteractionTable();
		}
		const FGameplayTagContainer TargetTags = TargetComp->GetStatusTags();
		const FStatusReactionRow* Reaction = Table ? Table->FindReaction(DamageType, TargetTags) : nullptr;
		if (Reaction)
		{
			Damage *= Reaction->DamageMultiplier;
			Hit.bReactionTriggered = true;
			UE_LOG(LogERBattle, Log, TEXT("[Skill] 反应触发: %s 伤害 × 目标状态 → 伤害 x%.2f%s"),
				GetDamageTypeName(DamageType), Reaction->DamageMultiplier,
				Reaction->bChain ? TEXT(" (+连锁溅射)") : TEXT(""));
		}

		// ---- ⑥ 抗性减免 ----
		const float Resistance = TargetComp->GetEffectiveResistance(DamageType);
		const float Mitigated = FMath::Max(1.0f, Damage * (1.0f - Resistance));
		Hit.MitigatedDamage = Mitigated;

		// ---- ⑧ 护盾 → 完整度 ----
		float BarrierAbsorbed = 0.0f;
		float IntegrityDamage = 0.0f;
		Target->ApplyDamageAmount(Mitigated, Caster, DamageType, Hit.bCrit, &BarrierAbsorbed, &IntegrityDamage);
		Hit.BarrierAbsorbed = BarrierAbsorbed;
		Hit.IntegrityDamage = IntegrityDamage;

		// ---- ⑨ 状态施加 (命中才挂) ----
		for (const FStatusApplication& Request : Skill->TargetStatuses)
		{
			TargetComp->ApplyStatus(Request, Caster);
		}

		// 暴击独立提示 (无论是否触发反应)
		if (Hit.bCrit)
		{
			SpawnText(World, Target, TEXT("暴击!"), FColor(255, 220, 90), 46.0f);
		}

		// ---- ⑦ 反应后续: 追加状态 / 消耗层数 / 连锁溅射 ----
		if (Reaction)
		{
			for (const FStatusApplication& Bonus : Reaction->BonusStatuses)
			{
				TargetComp->ApplyStatus(Bonus, Caster);
			}
			ConsumeStatusStacks(TargetComp, Reaction->RequiredStatus, Reaction->ConsumeStacks);
			SpawnText(World, Target, TEXT("反应! (REACTION)"), FColor(255, 240, 150), 34.0f);
		}

		// ---- ⑩ 事件广播 ----
		if (Hub)
		{
			Hub->OnHitResolved.Broadcast(Hit);

			FBattleDamageEvent DamageEvent;
			DamageEvent.Source = Caster;
			DamageEvent.Target = Target;
			DamageEvent.DamageType = DamageType;
			DamageEvent.BarrierAbsorbed = BarrierAbsorbed;
			DamageEvent.IntegrityDamage = IntegrityDamage;
			DamageEvent.bKilled = !Target->IsAlive();
			Hub->OnDamageResolved.Broadcast(DamageEvent);
		}

		// ---- 连锁溅射 (波及附近携带同标签者, 不递归反应) ----
		if (Reaction && Reaction->bChain && Target->IsAlive() && World)
		{
			const FGameplayTag ChainTag = Reaction->ChainTag.IsValid() ? Reaction->ChainTag : Reaction->RequiredStatus;
			const float ChainDamage = Mitigated * Reaction->ChainDamageScale;

			TArray<ABattleUnit*> ChainVictims;
			if (UBattleContext* Ctx = UBattleContext::Get(World))
			for (ABattleUnit* Candidate : Ctx->GetUnits())
			{
				if (!Candidate || Candidate == Target || Candidate == Caster || !Candidate->IsAlive())
				{
					continue;
				}
				if (FVector::Dist(Candidate->GetActorLocation(), Target->GetActorLocation()) > Reaction->ChainRadius)
				{
					continue;
				}
				UBattleSkillComponent* CandidateComp = Candidate->GetSkillComponent();
				if (CandidateComp && CandidateComp->HasStatusTag(ChainTag))
				{
					ChainVictims.Add(Candidate);
				}
			}

			for (ABattleUnit* Victim : ChainVictims)
			{
				SpawnText(World, Victim, TEXT("连锁! (CHAIN)"), FColor(90, 210, 255), 34.0f);
				UBattleResolver::ApplyDirectDamage(Caster, Victim, DamageType, ChainDamage);
			}
		}
	}
}

// ---------------- 整技能结算 ----------------

void UBattleResolver::ResolveSkill(ABattleUnit* Caster, ABattleUnit* Target, const FVector& TargetLocation,
	UBattleSkillData* Skill, int32 SkillLevel, const FBattleSkillExecutionContext& Context)
{
	if (!Caster || !Skill)
	{
		return;
	}

	UWorld* World = Caster->GetWorld();
	if (UBattleEventHub* Hub = UBattleEventHub::Get(World))
	{
		Hub->OnSkillActivated.Broadcast(Caster, Skill);
	}
	UE_LOG(LogERBattle, Log, TEXT("[Skill] [%s] 施放 [%s] (AP %d, CD %d, 段数 %d)"),
		*Caster->Stats.DisplayName, *Skill->DisplayName.ToString(), Skill->APCost, Skill->CooldownRounds,
		FMath::Max(1, Skill->HitCount));

	const EDamageType DamageType = Skill->Damage.DamageType;

	// 1. 自身效果 (瞬间 + 状态)
	ApplyInstantEffects(Caster, Caster, Skill->SelfInstantEffects, DamageType);
	if (UBattleSkillComponent* CasterComp = Caster->GetSkillComponent())
	{
		for (const FStatusApplication& Request : Skill->SelfStatuses)
		{
			CasterComp->ApplyStatus(Request, Caster);
		}
	}

	// 2. 目标收集
	TArray<FTargetEntry> Entries;
	if (Skill->AoE.IsArea())
	{
		if (UBattleContext* Ctx = UBattleContext::Get(World))
		for (ABattleUnit* Unit : Ctx->GetUnits())
		{
			if (!Unit || !Unit->IsAlive())
			{
				continue;
			}

			const float Dist = FVector::Dist(Unit->GetActorLocation(), TargetLocation);
			if (Dist > Skill->AoE.Radius)
			{
				continue;
			}

			const bool bIsEnemy = Unit->Team != Caster->Team;
			if (bIsEnemy && !Skill->AoE.bHitEnemies)
			{
				continue;
			}
			if (!bIsEnemy && Unit != Caster && !Skill->AoE.bHitAllies)
			{
				continue;
			}
			if (Unit == Caster && !Skill->AoE.bHitSelf && !Skill->AoE.bHitAllies)
			{
				continue;
			}

			FTargetEntry Entry;
			Entry.Unit = Unit;
			Entry.Scale = Skill->AoE.ScaleAtDistance(Dist);
			Entry.Dist = Dist;
			Entries.Add(Entry);
		}

		// 由近及远, 超出上限截断
		Entries.Sort([](const FTargetEntry& A, const FTargetEntry& B) { return A.Dist < B.Dist; });
		if (Skill->AoE.MaxTargets > 0 && Entries.Num() > Skill->AoE.MaxTargets)
		{
			Entries.SetNum(Skill->AoE.MaxTargets);
		}
	}
	else if (Target && Target->IsAlive())
	{
		FTargetEntry Entry;
		Entry.Unit = Target;
		Entry.Scale = 1.0f;
		Entries.Add(Entry);
	}

	// 3. 逐目标逐段: 首段立即结算, 后续按 HitInterval 延迟 (轻型连击手感)
	const bool bIsArea = Skill->AoE.IsArea();
	for (const FTargetEntry& Entry : Entries)
	{
		for (int32 HitIdx = 0; HitIdx < FMath::Max(1, Skill->HitCount); ++HitIdx)
		{
			const float Delay = HitIdx * Skill->HitInterval;
			if (Delay <= KINDA_SMALL_NUMBER)
			{
				ResolveSingleHit(Caster, Entry.Unit, Skill, SkillLevel, Entry.Scale, bIsArea);
			}
			else if (World)
			{
				TWeakObjectPtr<ABattleUnit> WeakCaster = Caster;
				TWeakObjectPtr<ABattleUnit> WeakTarget = Entry.Unit;
				TWeakObjectPtr<UBattleSkillData> WeakSkill = Skill;

				FTimerHandle HitHandle;
				World->GetTimerManager().SetTimer(HitHandle, FTimerDelegate::CreateWeakLambda(Caster,
					[WeakCaster, WeakTarget, WeakSkill, SkillLevel, Scale = Entry.Scale, bIsArea]()
					{
						ABattleUnit* CasterStrong = WeakCaster.Get();
						ABattleUnit* TargetStrong = WeakTarget.Get();
						UBattleSkillData* SkillStrong = WeakSkill.Get();
						if (CasterStrong && TargetStrong && SkillStrong)
						{
							ResolveSingleHit(CasterStrong, TargetStrong, SkillStrong, SkillLevel, Scale, bIsArea);
						}
					}), Delay, false);
			}
		}
	}

	// 3b. 连锁弹跳 (单体技能: 主目标命中后电弧跳向最近敌人, 电学学派特色)
	//     以每个命中者为圆心向 JumpRadius 内最近的未命中敌人跳跃; 与主段数错开,
	//     伤害按 JumpDamageScale 逐跳衰减, 每跳独立走完整命中管线。
	if (!bIsArea && Target && Skill->Chain.IsChain() && World)
	{
		TArray<ABattleUnit*> Visited;
		Visited.Add(Caster);
		Visited.Add(Target);

		ABattleUnit* From = Target;
		float JumpScale = Skill->Chain.JumpDamageScale;
		const float BaseDelay = FMath::Max(0, Skill->HitCount - 1) * Skill->HitInterval;

		for (int32 Jump = 0; Jump < Skill->Chain.MaxJumps && From; ++Jump)
		{
			// 在上一个命中者的 JumpRadius 内找最近的未命中敌人
			ABattleUnit* Next = nullptr;
			float BestDistSq = FMath::Square(Skill->Chain.JumpRadius);
			if (UBattleContext* Ctx = UBattleContext::Get(World))
			for (ABattleUnit* Candidate : Ctx->GetUnits())
			{
				if (!Candidate || !Candidate->IsAlive() || Candidate->Team == Caster->Team
					|| Visited.Contains(Candidate))
				{
					continue;
				}
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), From->GetActorLocation());
				if (DistSq <= BestDistSq)
				{
					BestDistSq = DistSq;
					Next = Candidate;
				}
			}
			if (!Next)
			{
				break;
			}

			Visited.Add(Next);
			const float JumpDelay = BaseDelay + Skill->Chain.JumpInterval * (float)(Jump + 1);
			TWeakObjectPtr<ABattleUnit> WeakCaster = Caster;
			TWeakObjectPtr<ABattleUnit> WeakTarget = Next;
			TWeakObjectPtr<UBattleSkillData> WeakSkill = Skill;

			FTimerHandle ChainHandle;
			World->GetTimerManager().SetTimer(ChainHandle, FTimerDelegate::CreateWeakLambda(Caster,
				[WeakCaster, WeakTarget, WeakSkill, SkillLevel, Scale = JumpScale]()
				{
					ABattleUnit* CasterStrong = WeakCaster.Get();
					ABattleUnit* TargetStrong = WeakTarget.Get();
					UBattleSkillData* SkillStrong = WeakSkill.Get();
					if (CasterStrong && TargetStrong && SkillStrong)
					{
						ResolveSingleHit(CasterStrong, TargetStrong, SkillStrong, SkillLevel, Scale, false);
					}
				}), JumpDelay, false);

			SpawnText(World, Next, TEXT("跳跃! (ARC)"), FColor(255, 205, 80), 30.0f);
			From = Next;
			JumpScale *= Skill->Chain.JumpDamageScale;
		}
	}

	// 4. 蓝图扩展钩子
	Skill->K2_Execute(Context);
}

// ---------------- 原子结算 ----------------

float UBattleResolver::ApplyDirectDamage(ABattleUnit* Source, ABattleUnit* Target, EDamageType DamageType, float Amount)
{
	if (!Target || !Target->IsAlive() || Amount <= 0.0f)
	{
		return 0.0f;
	}

	UBattleSkillComponent* TargetComp = Target->GetSkillComponent();
	if (!TargetComp)
	{
		return 0.0f;
	}

	// 抗性减免 (真实伤害免疫)
	const float Resistance = TargetComp->GetEffectiveResistance(DamageType);
	const float Mitigated = FMath::Max(1.0f, Amount * (1.0f - Resistance));

	float BarrierAbsorbed = 0.0f;
	float IntegrityDamage = 0.0f;
	Target->ApplyDamageAmount(Mitigated, Source, DamageType, false, &BarrierAbsorbed, &IntegrityDamage);

	if (UBattleEventHub* Hub = UBattleEventHub::Get(Target->GetWorld()))
	{
		FBattleDamageEvent DamageEvent;
		DamageEvent.Source = Source;
		DamageEvent.Target = Target;
		DamageEvent.DamageType = DamageType;
		DamageEvent.BarrierAbsorbed = BarrierAbsorbed;
		DamageEvent.IntegrityDamage = IntegrityDamage;
		DamageEvent.bKilled = !Target->IsAlive();
		Hub->OnDamageResolved.Broadcast(DamageEvent);
	}

	return IntegrityDamage;
}

void UBattleResolver::ApplyInstantEffects(ABattleUnit* Receiver, ABattleUnit* Source,
	const TArray<FAttributeModifier>& Mods, EDamageType DamageType)
{
	if (!Receiver)
	{
		return;
	}

	UWorld* World = Receiver->GetWorld();
	UBattleSkillComponent* Comp = Receiver->GetSkillComponent();

	for (const FAttributeModifier& Mod : Mods)
	{
		// 纪律: 瞬间效果只动当前值 (派生属性必须走状态效果)
		if (!Mod.IsValidForCurrent())
		{
			UE_LOG(LogERBattle, Warning, TEXT("[Resolver] 瞬间效果忽略派生属性 [%s] (请改用状态效果)"),
				GetBattleAttributeName(Mod.Attribute));
			continue;
		}
		if (Mod.Op != EModifierOp::Add)
		{
			UE_LOG(LogERBattle, Warning, TEXT("[Resolver] 当前值只支持 Add 修饰, 已忽略 Op=%d"), (int32)Mod.Op);
			continue;
		}

		switch (Mod.Attribute)
		{
		case EBattleAttribute::Integrity:
			if (Mod.Magnitude < 0.0f)
			{
				ApplyDirectDamage(Source, Receiver, DamageType, -Mod.Magnitude);
			}
			else if (Mod.Magnitude > 0.0f)
			{
				Receiver->ApplyHeal(Mod.Magnitude, Source);
			}
			break;

		case EBattleAttribute::Barrier:
			if (Mod.Magnitude >= 0.0f)
			{
				Receiver->AddBarrier(Mod.Magnitude);
			}
			else
			{
				Receiver->DrainBarrier(-Mod.Magnitude);
			}
			break;

		case EBattleAttribute::AP:
			if (!FMath::IsNearlyZero(Mod.Magnitude) && Comp)
			{
				const int32 MaxAP = Comp->GetEffectiveMaxAP();
				const int32 NewAP = FMath::Clamp(Receiver->AP + FMath::RoundToInt(Mod.Magnitude), 0, MaxAP);
				const int32 Delta = NewAP - Receiver->AP;
				Receiver->AP = NewAP;
				Receiver->RefreshLabel();
				if (Delta != 0)
				{
					SpawnText(World, Receiver, FString::Printf(TEXT("%+d AP"), Delta), FColor(100, 200, 255));
				}
			}
			break;

		default:
			break;
		}
	}
}