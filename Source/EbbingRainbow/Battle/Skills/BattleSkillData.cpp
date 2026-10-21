// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/UI/BattleFloatingText.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

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
	ThemeColor = GetDisciplineDefaultColor(Discipline);
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
	case ESkillDiscipline::Kinetic:  return FLinearColor(0.95f, 0.8f, 0.6f);  // 力学: 暖白/砂岩金
	case ESkillDiscipline::Thermal:  return FLinearColor(1.0f, 0.25f, 0.1f);  // 热学: 赤红烈火
	case ESkillDiscipline::Electric: return FLinearColor(0.2f, 0.8f, 1.0f);   // 电学: 电弧青蓝
	case ESkillDiscipline::Chemical: return FLinearColor(0.35f, 0.95f, 0.25f);// 化学: 强酸亮绿
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
	else
	{
		if (!Target || !Target->IsAlive())
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

		// 射程判定
		const float Dist = FVector::Dist2D(Caster->GetActorLocation(), Target->GetActorLocation());
		if (Dist > Range)
		{
			OutReason = FString::Printf(TEXT("目标超出射程 (当前 %.0fcm, 最大 %.0fcm)"), Dist, Range);
			return false;
		}
	}

	return true;
}

bool UBattleSkillData::Execute(const FBattleSkillExecutionContext& Context)
{
	ABattleUnit* Caster = Context.Caster;
	ABattleUnit* Target = Context.Target;
	if (!Caster)
	{
		return false;
	}

	UWorld* World = Caster->GetWorld();
	if (!World)
	{
		return false;
	}

	// 1. 施法者面向目标
	if (Target && Target != Caster)
	{
		const FVector ToTarget = Target->GetActorLocation() - Caster->GetActorLocation();
		Caster->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	// 2. 播放施法动作
	if (CastAnimation && Caster->GetMesh())
	{
		Caster->GetMesh()->PlayAnimation(CastAnimation, false);
	}

	// 3. 视觉表现: 技能光束/打击射线
	if (Target)
	{
		const FVector Muzzle = Caster->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		const FVector HitLoc = Target->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		const FColor FxColor = ThemeColor.ToFColor(true);

		DrawDebugLine(World, Muzzle, HitLoc, FxColor, false, 1.2f, 0, DeliveryWeight == ESkillDeliveryWeight::Heavy ? 6.0f : 3.0f);
		DrawDebugSphere(World, HitLoc, DeliveryWeight == ESkillDeliveryWeight::Heavy ? 50.0f : 30.0f, 12, FxColor, false, 1.0f);
	}

	// 4. 对施法者自身应用效果 (如护盾、增益)
	for (const FAttributeEffectDef& Effect : SelfEffects)
	{
		ApplyAttributeEffect(Caster, Caster, Effect);
	}

	// 5. 对目标应用效果 (区分轻型多次打击与重型单次爆发)
	if (Target)
	{
		const int32 ActualHits = FMath::Max(1, HitCount);

		if (ActualHits == 1)
		{
			// 重型 (单次高伤爆发) 或单发技能
			for (const FAttributeEffectDef& Effect : TargetEffects)
			{
				ApplyAttributeEffect(Target, Caster, Effect);
			}
		}
		else
		{
			// 轻型 (多次/低数值连击)
			// 每段打击按时间间隔连续执行，模拟机枪连射/刺拳连击效果
			for (int32 HitIdx = 0; HitIdx < ActualHits; ++HitIdx)
			{
				const float Delay = HitIdx * HitInterval;
				if (Delay <= KINDA_SMALL_NUMBER)
				{
					for (const FAttributeEffectDef& Effect : TargetEffects)
					{
						ApplyAttributeEffect(Target, Caster, Effect);
					}
				}
				else
				{
					TWeakObjectPtr<ABattleUnit> WeakTarget = Target;
					TWeakObjectPtr<ABattleUnit> WeakCaster = Caster;
					TArray<FAttributeEffectDef> EffectsCopy = TargetEffects;
					EDamageType DmgType = GetDisciplineDamageType();

					FTimerHandle HitHandle;
					World->GetTimerManager().SetTimer(HitHandle, [WeakTarget, WeakCaster, EffectsCopy, this]() {
						if (WeakTarget.IsValid() && WeakTarget->IsAlive() && WeakCaster.IsValid())
						{
							for (const FAttributeEffectDef& Effect : EffectsCopy)
							{
								const_cast<UBattleSkillData*>(this)->ApplyAttributeEffect(WeakTarget.Get(), WeakCaster.Get(), Effect);
							}
						}
					}, Delay, false);
				}
			}
		}
	}

	// 6. 允许蓝图扩展
	K2_Execute(Context);

	return true;
}

void UBattleSkillData::ApplyAttributeEffect(ABattleUnit* Receiver, ABattleUnit* Instigator, const FAttributeEffectDef& EffectDef)
{
	if (!Receiver || !Receiver->IsAlive())
	{
		return;
	}

	const EDamageType DmgType = GetDisciplineDamageType();
	Receiver->ApplyAttributeModifier(EffectDef.TargetAttribute, EffectDef.Value, EffectDef.Op, Instigator, DmgType);
}

#if WITH_EDITOR
void UBattleSkillData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();

	// 自动同步派系默认颜色
	if (PropName == GET_MEMBER_NAME_CHECKED(UBattleSkillData, Discipline))
	{
		ThemeColor = GetDisciplineDefaultColor(Discipline);
	}

	// 自动同步射程规格默认数值
	if (PropName == GET_MEMBER_NAME_CHECKED(UBattleSkillData, RangeProfile))
	{
		if (RangeProfile == ESkillRangeProfile::Melee && Range > 400.0f)
		{
			Range = 250.0f;
		}
		else if (RangeProfile == ESkillRangeProfile::Ranged && Range < 500.0f)
		{
			Range = 1200.0f;
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
}
#endif
