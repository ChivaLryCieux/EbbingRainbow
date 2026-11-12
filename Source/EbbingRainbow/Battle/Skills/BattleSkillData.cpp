// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleSkillData.h"
#include "Core/ERLog.h"
#include "Battle/Units/BattleUnit.h"

UBattleSkillData::UBattleSkillData()
{
	Discipline = ESkillDiscipline::Kinetic;
	RangeProfile = ESkillRangeProfile::Melee;
	DeliveryWeight = ESkillDeliveryWeight::Heavy;
	TacticalRole = ESkillTacticalRole::Offensive;

	APCost = 1;
	CooldownRounds = 0;
	Range = 250.0f;
	HitCount = 1;
	HitInterval = 0.15f;
	DamageVariance = 0.10f;
	Accuracy = 0.95f;
	ThemeColor = GetDisciplineDefaultColor(Discipline);
	Damage.DamageType = GetDisciplineDamageType();
}

EDamageType UBattleSkillData::GetDisciplineDamageType() const
{
	switch (Discipline)
	{
	case ESkillDiscipline::Kinetic:  return EDamageType::Kinetic;
	case ESkillDiscipline::Thermal:  return EDamageType::Thermal;
	case ESkillDiscipline::Electric: return EDamageType::Electric;
	case ESkillDiscipline::Chemical: return EDamageType::Chemical;
	default: return EDamageType::Kinetic;
	}
}

FLinearColor UBattleSkillData::GetDisciplineDefaultColor(ESkillDiscipline InDiscipline)
{
	switch (InDiscipline)
	{
	case ESkillDiscipline::Kinetic:  return FLinearColor(0.20f, 0.45f, 0.95f);// 力学: 深蓝 (图标 blue)
	case ESkillDiscipline::Thermal:  return FLinearColor(0.92f, 0.16f, 0.08f);// 热学: 赤红 (图标 red)
	case ESkillDiscipline::Electric: return FLinearColor(0.88f, 0.62f, 0.06f);// 电学: 鎏金 (图标 gold)
	case ESkillDiscipline::Chemical: return FLinearColor(0.55f, 0.28f, 0.88f);// 化学: 毒紫 (图标 purple)
	default: return FLinearColor::White;
	}
}

bool UBattleSkillData::CanActivate(const ABattleUnit* Caster, const FBattleSkillExecutionContext& Context, FString& OutReason) const
{
	if (!Caster || !Caster->IsAlive())
	{
		OutReason = TEXT("施法者不存在或已损毁");
		return false;
	}

	if (Caster->AP < APCost)
	{
		OutReason = FString::Printf(TEXT("行动点不足 (需要 %d, 剩余 %d)"), APCost, Caster->AP);
		return false;
	}

	const ABattleUnit* Target = Context.Target;

	// 目标筛选判定
	if (TargetFilter == ESkillTargetFilter::SelfOnly)
	{
		if (Target != Caster)
		{
			OutReason = TEXT("该技能只能以自身为目标");
			return false;
		}
	}
	else if (Target)
	{
		if (!Target->IsAlive())
		{
			OutReason = TEXT("目标无效或已损毁");
			return false;
		}

		if (TargetFilter == ESkillTargetFilter::EnemyOnly && Target->Team == Caster->Team)
		{
			OutReason = TEXT("不能以友军为目标");
			return false;
		}

		if (TargetFilter == ESkillTargetFilter::AllyOrSelf && Target->Team != Caster->Team)
		{
			OutReason = TEXT("只能以友军或自身为目标");
			return false;
		}
	}
	else if (!AoE.IsArea())
	{
		OutReason = TEXT("目标无效或已损毁");
		return false;
	}

	// 3D 射程判定 (多层战场: 上下层不能隔空互殴)
	const FVector AimPoint = Target ? Target->GetActorLocation() : Context.TargetLocation;
	if (TargetFilter != ESkillTargetFilter::SelfOnly)
	{
		const float Dist = FVector::Dist(Caster->GetActorLocation(), AimPoint);
		if (Dist > Range)
		{
			OutReason = FString::Printf(TEXT("目标超出射程 (当前 %.0fcm, 最大 %.0fcm)"), Dist, Range);
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
void UBattleSkillData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();

	// 自动同步派系默认颜色与伤害类型
	if (PropName == GET_MEMBER_NAME_CHECKED(UBattleSkillData, Discipline))
	{
		ThemeColor = GetDisciplineDefaultColor(Discipline);
		Damage.DamageType = GetDisciplineDamageType();
	}

	// 自动同步射程规格默认数值
	if (PropName == GET_MEMBER_NAME_CHECKED(UBattleSkillData, RangeProfile))
	{
		if (RangeProfile == ESkillRangeProfile::Melee)
		{
			bRequiresLineOfSight = false;
			if (Range > 400.0f)
			{
				Range = 250.0f;
			}
		}
		else if (RangeProfile == ESkillRangeProfile::Ranged)
		{
			bRequiresLineOfSight = true;
			if (Range < 500.0f)
			{
				Range = 1200.0f;
			}
		}
	}

	// 自动同步运载重量默认段数
	if (PropName == GET_MEMBER_NAME_CHECKED(UBattleSkillData, DeliveryWeight))
	{
		if (DeliveryWeight == ESkillDeliveryWeight::Heavy)
		{
			HitCount = 1;
		}
		else if (DeliveryWeight == ESkillDeliveryWeight::Light && HitCount == 1)
		{
			HitCount = 3;
		}
	}

	// 瞬间效果只允许当前值: 派生属性请用状态
	for (FAttributeModifier& Mod : TargetInstantEffects)
	{
		if (Mod.IsValidForDerived())
		{
			UE_LOG(LogERBattle, Warning, TEXT("[Skill] %s: 瞬间效果不能改派生属性 [%s], 已忽略语义, 请改用状态效果"),
				*GetName(), GetBattleAttributeName(Mod.Attribute));
		}
	}
	for (FAttributeModifier& Mod : SelfInstantEffects)
	{
		if (Mod.IsValidForDerived())
		{
			UE_LOG(LogERBattle, Warning, TEXT("[Skill] %s: 瞬间效果不能改派生属性 [%s], 已忽略语义, 请改用状态效果"),
				*GetName(), GetBattleAttributeName(Mod.Attribute));
		}
	}
}
#endif