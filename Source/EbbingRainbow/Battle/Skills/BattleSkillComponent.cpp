// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleSkillComponent.h"
#include "Core/ERLog.h"
#include "Battle/Skills/BattleSkillLibrary.h"
#include "Battle/Skills/BattleResolver.h"
#include "Battle/Skills/BattleEventHub.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Core/UBattleContext.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

/**
 * 调试: ER.StatusSelfTest 1 —— 对首个存活单位施加 2 回合状态并连续结算 3 次回合结束,
 * 一帧内确定性验证 "施加 → 时长递减 → 到期移除" 全周期 (日志 [Status])。
 */
#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GStatusSelfTestCmd(
	TEXT("ER.StatusSelfTest"),
	TEXT("调试: 状态生命周期自检 (施加 2 回合状态 + 3 次回合结束结算, 验证递减/到期)。"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UBattleSkillLibrary* Library = UBattleSkillLibrary::Get(World);
		UBattleStatusEffect* Def = Library
			? Library->FindStatusByTag(FGameplayTag::RequestGameplayTag(FName("Status.Conductive")))
			: nullptr;
		if (!Def)
		{
			UE_LOG(LogERBattle, Warning, TEXT("[Status] 自检失败: 找不到内置状态"));
			return;
		}

		if (UBattleContext* Ctx = UBattleContext::Get(World))
		for (ABattleUnit* Unit : Ctx->GetUnits())
		{
			if (Unit && Unit->IsAlive() && Unit->GetSkillComponent())
			{
				UBattleSkillComponent* Comp = Unit->GetSkillComponent();
				UE_LOG(LogERBattle, Log, TEXT("[Status] 自检开始: [%s] 施加 2 回合状态 → 连续 3 次回合结束"),
					*Unit->Stats.DisplayName);
				Comp->ApplyStatusDef(Def, 1, -1, Unit);
				for (int32 i = 0; i < 3; ++i)
				{
					Comp->NotifyRoundEnd();
				}
				UE_LOG(LogERBattle, Log, TEXT("[Status] 自检结束 (剩余状态数 %d, 期望 0)"), Comp->GetActiveStatuses().Num());
				return;
			}
		}
	}));
#endif // !UE_BUILD_SHIPPING

UBattleSkillComponent::UBattleSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ABattleUnit* UBattleSkillComponent::GetOwnerUnit() const
{
	return Cast<ABattleUnit>(GetOwner());
}

// ---------------- 技能实例 ----------------

void UBattleSkillComponent::GrantSkill(UBattleSkillData* SkillData, int32 Level)
{
	if (!SkillData)
	{
		return;
	}

	FBattleSkillSpec Spec;
	Spec.SkillData = SkillData;
	Spec.Level = FMath::Max(1, Level);
	Skills.Add(Spec);
	OnSkillsetChanged.Broadcast();
}

void UBattleSkillComponent::GrantSkillsByIds(const TArray<FName>& SkillIds)
{
	ABattleUnit* OwnerUnit = GetOwnerUnit();
	UWorld* World = OwnerUnit ? OwnerUnit->GetWorld() : nullptr;
	UBattleSkillLibrary* Library = UBattleSkillLibrary::Get(World);
	if (!Library)
	{
		return;
	}

	for (const FName& Id : SkillIds)
	{
		GrantSkill(Library->FindSkillById(Id));
	}
}

void UBattleSkillComponent::ClearSkills()
{
	Skills.Reset();
	OnSkillsetChanged.Broadcast();
}

UBattleSkillData* UBattleSkillComponent::GetSkillData(int32 Index) const
{
	const FBattleSkillSpec* Spec = GetSpec(Index);
	return Spec ? Spec->SkillData.Get() : nullptr;
}

bool UBattleSkillComponent::IsSkillReady(int32 Index) const
{
	const FBattleSkillSpec* Spec = GetSpec(Index);
	return Spec && Spec->SkillData && !Spec->IsOnCooldown();
}

void UBattleSkillComponent::NotifySkillActivated(int32 Index)
{
	FBattleSkillSpec* Spec = GetSpec(Index);
	if (!Spec || !Spec->SkillData)
	{
		return;
	}

	// 冷却语义 (设计文档 §4): 写入 N 后, 接下来 N 个自己的回合不可用
	Spec->CooldownRemaining = FMath::Max(Spec->CooldownRemaining, Spec->SkillData->CooldownRounds);
	OnSkillsetChanged.Broadcast();
}

void UBattleSkillComponent::NotifyTurnStart()
{
	for (FBattleSkillSpec& Spec : Skills)
	{
		Spec.bBlockedThisTurn = Spec.CooldownRemaining > 0;
		Spec.CooldownRemaining = FMath::Max(0, Spec.CooldownRemaining - 1);
	}
	OnSkillsetChanged.Broadcast();
}

// ---------------- 回合生命周期 ----------------

void UBattleSkillComponent::NotifyRoundStart()
{
	TickStatuses(EStatusTickTiming::RoundStart);
}

void UBattleSkillComponent::NotifyRoundEnd()
{
	TickStatuses(EStatusTickTiming::RoundEnd);

	// 时长递减, 到期移除 (0 回合 = 永久, -1 表示永久)
	bool bChanged = false;
	for (int32 i = ActiveStatuses.Num() - 1; i >= 0; --i)
	{
		FBattleActiveStatus& Status = ActiveStatuses[i];
		if (Status.RemainingRounds > 0)
		{
			--Status.RemainingRounds;
			bChanged = true;
			UE_LOG(LogERBattle, Log, TEXT("[Status] [%s] %s 时长递减 → %d"),
				*GetOwnerUnit()->Stats.DisplayName,
				Status.Def ? *Status.Def->DisplayName.ToString() : TEXT("?"),
				Status.RemainingRounds);

			if (Status.IsExpired())
			{
				if (UBattleStatusEffect* Def = Status.Def)
				{
					const ABattleUnit* ExpireOwner = GetOwnerUnit();
					UE_LOG(LogERBattle, Log, TEXT("[Status] [%s] 的 [%s] 到期移除"),
						ExpireOwner ? *ExpireOwner->Stats.DisplayName : TEXT("?"), *Def->DisplayName.ToString());
					if (UBattleEventHub* Hub = UBattleEventHub::Get(GetWorld()))
					{
						Hub->OnStatusChanged.Broadcast(GetOwnerUnit(), Def, false);
					}
				}
				ActiveStatuses.RemoveAt(i);
			}
		}
	}

	if (bChanged)
	{
		OnStatusesChanged.Broadcast();
	}
}

// ---------------- 状态效果 ----------------

bool UBattleSkillComponent::ApplyStatus(const FStatusApplication& Request, ABattleUnit* Source)
{
	UBattleStatusEffect* Def = Request.Status;
	if (!Def)
	{
		return false;
	}

	// 施加概率 (战斗随机流)
	if (Request.ApplyChance < 1.0f)
	{
		if (FBattleRandomStream* Rng = UBattleResolver::GetRng(GetWorld()))
		{
			if (!Rng->Chance(Request.ApplyChance))
			{
				return false;
			}
		}
	}

	return ApplyStatusDef(Def, Request.Stacks, Request.DurationOverride, Source);
}

bool UBattleSkillComponent::ApplyStatusDef(UBattleStatusEffect* Def, int32 Stacks, int32 DurationOverride, ABattleUnit* Source)
{
	ABattleUnit* OwnerUnit = GetOwnerUnit();
	if (!Def || !OwnerUnit || !OwnerUnit->IsAlive())
	{
		return false;
	}

	// 1. 免疫检查: 已有状态声明免疫则拒绝
	for (const FBattleActiveStatus& Existing : ActiveStatuses)
	{
		if (Existing.Def && Existing.Def->ImmuneTags.HasTag(Def->StatusTag))
		{
			return false;
		}
	}

	// 2. 驱散规则: 新状态的标签命中已有状态的 RemoveOnTags → 移除旧状态
	for (int32 i = ActiveStatuses.Num() - 1; i >= 0; --i)
	{
		const FBattleActiveStatus& Existing = ActiveStatuses[i];
		if (Existing.Def && Existing.Def->RemoveOnTags.HasTag(Def->StatusTag))
		{
			RemoveStatus(Existing.Def, 0);
		}
	}

	const int32 NewDuration = (DurationOverride >= 0) ? DurationOverride : Def->BaseDurationRounds;
	// 0 或负 = 永久, 内部统一用 -1
	const int32 Remaining = (NewDuration <= 0) ? -1 : NewDuration;
	const int32 AddStacks = FMath::Max(1, Stacks);

	// 3. 叠层策略
	FBattleActiveStatus* ExistingStatus = ActiveStatuses.FindByPredicate(
		[Def](const FBattleActiveStatus& S) { return S.Def == Def; });

	if (ExistingStatus)
	{
		switch (Def->StackingPolicy)
		{
		case EStatusStackingPolicy::Reject:
			return false;

		case EStatusStackingPolicy::RefreshDuration:
			ExistingStatus->RemainingRounds = FMath::Max(ExistingStatus->RemainingRounds, Remaining);
			break;

		case EStatusStackingPolicy::StackCount:
			ExistingStatus->Stacks = FMath::Min(Def->MaxStacks, ExistingStatus->Stacks + AddStacks);
			ExistingStatus->RemainingRounds = FMath::Max(ExistingStatus->RemainingRounds, Remaining);
			break;

		case EStatusStackingPolicy::StackAndRefresh:
		default:
			ExistingStatus->Stacks = FMath::Min(Def->MaxStacks, ExistingStatus->Stacks + AddStacks);
			ExistingStatus->RemainingRounds = Remaining;
			break;
		}

		if (Source)
		{
			ExistingStatus->Source = Source;
		}
	}
	else
	{
		FBattleActiveStatus NewStatus;
		NewStatus.Def = Def;
		NewStatus.Stacks = FMath::Min(Def->MaxStacks, AddStacks);
		NewStatus.RemainingRounds = Remaining;
		NewStatus.Source = Source;
		ActiveStatuses.Add(NewStatus);
	}

	OnStatusesChanged.Broadcast();
	if (UBattleEventHub* Hub = UBattleEventHub::Get(GetWorld()))
	{
		Hub->OnStatusChanged.Broadcast(OwnerUnit, Def, true);
	}
	UE_LOG(LogERBattle, Log, TEXT("[Status] [%s] 获得 [%s] x%d (%s)"),
		*OwnerUnit->Stats.DisplayName, *Def->DisplayName.ToString(),
		ExistingStatus ? ExistingStatus->Stacks : AddStacks,
		Remaining < 0 ? TEXT("永久") : *FString::Printf(TEXT("%d 回合"), Remaining));
	return true;
}

void UBattleSkillComponent::RemoveStatus(UBattleStatusEffect* Def, int32 Stacks)
{
	if (!Def)
	{
		return;
	}

	for (int32 i = 0; i < ActiveStatuses.Num(); ++i)
	{
		if (ActiveStatuses[i].Def != Def)
		{
			continue;
		}

		if (Stacks <= 0 || Stacks >= ActiveStatuses[i].Stacks)
		{
			ActiveStatuses.RemoveAt(i);
		}
		else
		{
			ActiveStatuses[i].Stacks -= Stacks;
		}

		OnStatusesChanged.Broadcast();
		if (UBattleEventHub* Hub = UBattleEventHub::Get(GetWorld()))
		{
			Hub->OnStatusChanged.Broadcast(GetOwnerUnit(), Def, false);
		}
		return;
	}
}

void UBattleSkillComponent::ClearAllStatuses()
{
	if (ActiveStatuses.Num() > 0)
	{
		ActiveStatuses.Reset();
		OnStatusesChanged.Broadcast();
	}
}

bool UBattleSkillComponent::HasStatusTag(FGameplayTag Tag) const
{
	for (const FBattleActiveStatus& Status : ActiveStatuses)
	{
		if (Status.Def && (Status.Def->StatusTag.MatchesTag(Tag) || Status.Def->GrantedTags.HasTag(Tag)))
		{
			return true;
		}
	}
	return false;
}

FGameplayTagContainer UBattleSkillComponent::GetStatusTags() const
{
	FGameplayTagContainer Tags;
	for (const FBattleActiveStatus& Status : ActiveStatuses)
	{
		if (Status.Def)
		{
			Tags.AddTag(Status.Def->StatusTag);
			Tags.AppendTags(Status.Def->GrantedTags);
		}
	}
	return Tags;
}

FString UBattleSkillComponent::GetStatusSummary() const
{
	FString Summary;
	for (const FBattleActiveStatus& Status : ActiveStatuses)
	{
		const FString Label = Status.ToLabelString();
		if (Label.IsEmpty())
		{
			continue;
		}
		if (!Summary.IsEmpty())
		{
			Summary += TEXT(" · ");
		}
		Summary += Label;
	}
	return Summary;
}

// ---------------- 派生属性 ----------------

void UBattleSkillComponent::GatherModifiers(EBattleAttribute Attribute, TArray<FAttributeModifier>& OutMods, TArray<int32>& OutStacks) const
{
	for (const FBattleActiveStatus& Status : ActiveStatuses)
	{
		if (!Status.Def)
		{
			continue;
		}
		for (const FAttributeModifier& Mod : Status.Def->OngoingModifiers)
		{
			if (Mod.Attribute == Attribute)
			{
				OutMods.Add(Mod);
				OutStacks.Add(Status.Stacks);
			}
		}
	}
}

float UBattleSkillComponent::GetEffectiveAttribute(EBattleAttribute Attribute) const
{
	const ABattleUnit* OwnerUnit = GetOwnerUnit();
	if (!OwnerUnit)
	{
		return 0.0f;
	}

	// 基础值: 当前值直接取运行时, 派生值取模板 (永不被运行时改写)
	float Base = 0.0f;
	switch (Attribute)
	{
	case EBattleAttribute::Integrity:            Base = OwnerUnit->Integrity; break;
	case EBattleAttribute::Barrier:              Base = OwnerUnit->Barrier; break;
	case EBattleAttribute::AP:                   Base = (float)OwnerUnit->AP; break;
	case EBattleAttribute::MaxIntegrity:         Base = OwnerUnit->Stats.MaxIntegrity; break;
	case EBattleAttribute::MaxAP:                Base = (float)OwnerUnit->Stats.MaxAP; break;
	case EBattleAttribute::Initiative:           Base = (float)OwnerUnit->Stats.Initiative; break;
	case EBattleAttribute::Evasion:              Base = OwnerUnit->Stats.EvasionRate; break;
	case EBattleAttribute::Accuracy:             Base = OwnerUnit->Stats.Accuracy; break;
	case EBattleAttribute::CritChance:           Base = OwnerUnit->Stats.CritChance; break;
	case EBattleAttribute::CritMultiplier:       Base = OwnerUnit->Stats.CritMultiplier; break;
	case EBattleAttribute::KineticResistance:    Base = OwnerUnit->Stats.KineticResistance; break;
	case EBattleAttribute::ThermalResistance:    Base = OwnerUnit->Stats.ThermalResistance; break;
	case EBattleAttribute::ElectricResistance:   Base = OwnerUnit->Stats.ElectricResistance; break;
	case EBattleAttribute::ChemicalResistance:   Base = OwnerUnit->Stats.ChemicalResistance; break;
	default: break;
	}

	if (IsCurrentAttribute(Attribute))
	{
		// 当前值不允许持续修饰 (纪律: 瞬间效果专属)
		return Base;
	}

	TArray<FAttributeModifier> Mods;
	TArray<int32> Stacks;
	GatherModifiers(Attribute, Mods, Stacks);
	return AggregateDerivedAttribute(Base, Mods, Stacks);
}

float UBattleSkillComponent::GetEffectiveEvasion() const
{
	return FMath::Clamp(GetEffectiveAttribute(EBattleAttribute::Evasion), 0.0f, 1.0f);
}

float UBattleSkillComponent::GetEffectiveInitiative() const
{
	return FMath::Max(1.0f, GetEffectiveAttribute(EBattleAttribute::Initiative));
}

float UBattleSkillComponent::GetEffectiveAccuracy() const
{
	return GetEffectiveAttribute(EBattleAttribute::Accuracy);
}

float UBattleSkillComponent::GetEffectiveCritChance() const
{
	return FMath::Clamp(GetEffectiveAttribute(EBattleAttribute::CritChance), 0.0f, 1.0f);
}

float UBattleSkillComponent::GetEffectiveCritMultiplier() const
{
	return FMath::Max(1.0f, GetEffectiveAttribute(EBattleAttribute::CritMultiplier));
}

float UBattleSkillComponent::GetEffectiveResistance(EDamageType DamageType) const
{
	EBattleAttribute Attribute;
	switch (DamageType)
	{
	case EDamageType::Kinetic:    Attribute = EBattleAttribute::KineticResistance; break;
	case EDamageType::Thermal:    Attribute = EBattleAttribute::ThermalResistance; break;
	case EDamageType::Electric:   Attribute = EBattleAttribute::ElectricResistance; break;
	case EDamageType::Chemical:   Attribute = EBattleAttribute::ChemicalResistance; break;
	case EDamageType::TrueDamage:
	default:                      return 0.0f; // 真实伤害无视抗性
	}
	return FMath::Clamp(GetEffectiveAttribute(Attribute), -1.0f, 0.85f);
}

float UBattleSkillComponent::GetEffectiveMaxIntegrity() const
{
	return FMath::Max(1.0f, GetEffectiveAttribute(EBattleAttribute::MaxIntegrity));
}

int32 UBattleSkillComponent::GetEffectiveMaxAP() const
{
	return FMath::Max(1, FMath::RoundToInt(GetEffectiveAttribute(EBattleAttribute::MaxAP)));
}

// ---------------- 状态跳动 ----------------

void UBattleSkillComponent::TickStatuses(EStatusTickTiming Timing)
{
	if (Timing == EStatusTickTiming::None)
	{
		return;
	}

	ABattleUnit* OwnerUnit = GetOwnerUnit();
	if (!OwnerUnit || !OwnerUnit->IsAlive())
	{
		return;
	}

	for (const FBattleActiveStatus& Status : ActiveStatuses)
	{
		UBattleStatusEffect* Def = Status.Def;
		if (!Def || Def->TickTiming != Timing || !Def->HasTick())
		{
			continue;
		}

		ABattleUnit* Source = Status.Source ? Status.Source.Get() : OwnerUnit;

		// 周期伤害: 走完整伤害管线 (抗性/护盾减免, 不做闪避判定)
		if (Def->TickDamage.BaseAmount > 0.0f)
		{
			const float Amount = Def->TickDamage.BaseAmount * Status.Stacks;
			UBattleResolver::ApplyDirectDamage(Source, OwnerUnit, Def->TickDamage.DamageType, Amount);
		}

		// 周期治疗: 瞬间完整度修改
		if (Def->TickHealAmount > 0.0f && OwnerUnit->IsAlive())
		{
			FAttributeModifier Heal;
			Heal.Attribute = EBattleAttribute::Integrity;
			Heal.Op = EModifierOp::Add;
			Heal.Magnitude = Def->TickHealAmount * Status.Stacks;
			UBattleResolver::ApplyInstantEffects(OwnerUnit, Source, { Heal }, EDamageType::TrueDamage);
		}

		if (!OwnerUnit->IsAlive())
		{
			break;
		}
	}
}