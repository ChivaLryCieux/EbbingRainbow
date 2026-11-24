// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleSkillLibrary.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleStatusEffect.h"
#include "Battle/Skills/BattleInteractionTable.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"

void UBattleSkillLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildDefaults();
}

UBattleSkillLibrary* UBattleSkillLibrary::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleSkillLibrary>() : nullptr;
}

UBattleStatusEffect* UBattleSkillLibrary::FindStatusByTag(FGameplayTag StatusTag) const
{
	for (const TObjectPtr<UBattleStatusEffect>& Status : Statuses)
	{
		if (Status && Status->StatusTag == StatusTag)
		{
			return Status;
		}
	}
	return nullptr;
}

UBattleSkillData* UBattleSkillLibrary::FindSkillById(FName SkillId) const
{
	for (const TObjectPtr<UBattleSkillData>& Skill : Skills)
	{
		if (Skill && Skill->SkillId == SkillId)
		{
			return Skill;
		}
	}
	return nullptr;
}

// ---------------- 内置内容构建 ----------------

UBattleStatusEffect* UBattleSkillLibrary::MakeStatus(FGameplayTag Tag, const TCHAR* Name, const TCHAR* Desc,
	int32 Duration, int32 MaxStacks, FLinearColor Color)
{
	UBattleStatusEffect* Status = NewObject<UBattleStatusEffect>(this);
	Status->StatusTag = Tag;
	Status->DisplayName = FText::FromString(Name);
	Status->Description = FText::FromString(Desc);
	Status->BaseDurationRounds = Duration;
	Status->MaxStacks = MaxStacks;
	Status->ThemeColor = Color;
	Statuses.Add(Status);
	return Status;
}

UBattleSkillData* UBattleSkillLibrary::MakeSkill(FName Id, const TCHAR* Name, const TCHAR* Desc)
{
	UBattleSkillData* Skill = NewObject<UBattleSkillData>(this);
	Skill->SkillId = Id;
	Skill->DisplayName = FText::FromString(Name);
	Skill->Description = FText::FromString(Desc);
	Skills.Add(Skill);
	return Skill;
}

void UBattleSkillLibrary::BuildDefaults()
{
	if (Statuses.Num() > 0)
	{
		return;
	}

	// ============ 四大学派状态 (设计文档 §3) ============

	UBattleStatusEffect* Burn = MakeStatus(FGameplayTag::RequestGameplayTag(FName("Status.Burn")),
		TEXT("燃烧"), TEXT("每回合结束受到热学伤害, 可叠层。"), 2, 3, FLinearColor(1.0f, 0.35f, 0.1f));
	Burn->TickTiming = EStatusTickTiming::RoundEnd;
	Burn->TickDamage.DamageType = EDamageType::Thermal;
	Burn->TickDamage.BaseAmount = 4.0f;

	UBattleStatusEffect* Conductive = MakeStatus(FGameplayTag::RequestGameplayTag(FName("Status.Conductive")),
		TEXT("导电"), TEXT("电学抗性大幅下降; 被电学击中会连锁溅射。"), 2, 3, FLinearColor(0.25f, 0.8f, 1.0f));
	{
		FAttributeModifier Mod;
		Mod.Attribute = EBattleAttribute::ElectricResistance;
		Mod.Op = EModifierOp::Multiply;
		Mod.Magnitude = -0.25f;
		Conductive->OngoingModifiers.Add(Mod);
	}

	UBattleStatusEffect* Corroded = MakeStatus(FGameplayTag::RequestGameplayTag(FName("Status.Corroded")),
		TEXT("酸蚀"), TEXT("全抗性下降; 被热学击中会沸腾蒸发。"), 3, 3, FLinearColor(0.4f, 0.95f, 0.25f));
	{
		for (EBattleAttribute Attr : { EBattleAttribute::KineticResistance, EBattleAttribute::ThermalResistance,
			EBattleAttribute::ElectricResistance, EBattleAttribute::ChemicalResistance })
		{
			FAttributeModifier Mod;
			Mod.Attribute = Attr;
			Mod.Op = EModifierOp::Add;
			Mod.Magnitude = -0.15f;
			Corroded->OngoingModifiers.Add(Mod);
		}
	}

	UBattleStatusEffect* Shattered = MakeStatus(FGameplayTag::RequestGameplayTag(FName("Status.Shattered")),
		TEXT("破甲"), TEXT("闪避与力学抗性下降; 被力学击中会碎裂加深。"), 2, 3, FLinearColor(0.95f, 0.8f, 0.6f));
	{
		FAttributeModifier Eva;
		Eva.Attribute = EBattleAttribute::Evasion;
		Eva.Op = EModifierOp::Add;
		Eva.Magnitude = -0.10f;
		Shattered->OngoingModifiers.Add(Eva);

		FAttributeModifier Res;
		Res.Attribute = EBattleAttribute::KineticResistance;
		Res.Op = EModifierOp::Add;
		Res.Magnitude = -0.15f;
		Shattered->OngoingModifiers.Add(Res);
	}

	// ============ 内置技能 (设计文档 §11 数值基线) ============
	//
	// 学派定位:
	//   力学 (blue)   —— 近距离单体伤害 (近战斩击/盾击/贯穿)
	//   热学 (red)    —— 以自身为圆心的周身范围伤害 (新星/脉冲)
	//   电学 (gold)   —— 命中后电弧连锁弹跳 (Chain.MaxJumps > 0)
	//   化学 (purple) —— 投掷型落点 AoE (爆弹/毒雾)
	//
	// 图标: /Game/UI/SkillIcons/T_<Color>_<Glyph> (128px 学派色徽章)
	const auto LoadIcon = [](const TCHAR* AssetName) -> UTexture2D*
	{
		return LoadObject<UTexture2D>(nullptr,
			*(FString(TEXT("/Game/UI/SkillIcons/")) + AssetName + TEXT(".") + AssetName));
	};

	// ---- 力学: 近战重击, 概率破甲 ----
	if (UBattleSkillData* Slash = MakeSkill(TEXT("Slash"), TEXT("近战斩击"), TEXT("近身重击目标敌军 (力学伤害), 50% 概率破甲。")))
	{
		Slash->Discipline = ESkillDiscipline::Kinetic;
		Slash->RangeProfile = ESkillRangeProfile::Melee;
		Slash->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Slash->TacticalRole = ESkillTacticalRole::Offensive;
		Slash->APCost = 1;
		Slash->Range = 250.0f;
		Slash->bRequiresLineOfSight = false;
		Slash->Accuracy = 1.0f;
		Slash->Damage.DamageType = EDamageType::Kinetic;
		Slash->Damage.BaseAmount = 15.0f;
		Slash->Icon = LoadIcon(TEXT("T_Blue_Sword"));
		Slash->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Melee")));

		FStatusApplication ShatterApply;
		ShatterApply.Status = Shattered;
		ShatterApply.Stacks = 1;
		ShatterApply.ApplyChance = 0.5f;
		Slash->TargetStatuses.Add(ShatterApply);
	}

	// ---- 力学: 盾击, 必破甲 (近战单体) ----
	if (UBattleSkillData* Bash = MakeSkill(TEXT("ShieldBash"), TEXT("盾击"), TEXT("近身盾击目标敌军 (力学伤害), 必定破甲。")))
	{
		Bash->Discipline = ESkillDiscipline::Kinetic;
		Bash->RangeProfile = ESkillRangeProfile::Melee;
		Bash->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Bash->TacticalRole = ESkillTacticalRole::Offensive;
		Bash->APCost = 1;
		Bash->Range = 250.0f;
		Bash->bRequiresLineOfSight = false;
		Bash->Accuracy = 0.95f;
		Bash->Damage.DamageType = EDamageType::Kinetic;
		Bash->Damage.BaseAmount = 10.0f;
		Bash->Icon = LoadIcon(TEXT("T_Blue_Shield"));
		Bash->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Melee")));

		FStatusApplication ShatterApply;
		ShatterApply.Status = Shattered;
		ShatterApply.Stacks = 1;
		Bash->TargetStatuses.Add(ShatterApply);
	}

	// ---- 力学: 力场贯穿, 重击高暴击 (近战单体) ----
	if (UBattleSkillData* Strike = MakeSkill(TEXT("PowerStrike"), TEXT("力场贯穿"), TEXT("蓄力贯穿目标敌军 (力学伤害), 高伤害, 暴击率提升。")))
	{
		Strike->Discipline = ESkillDiscipline::Kinetic;
		Strike->RangeProfile = ESkillRangeProfile::Melee;
		Strike->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Strike->TacticalRole = ESkillTacticalRole::Offensive;
		Strike->APCost = 2;
		Strike->Range = 250.0f;
		Strike->bRequiresLineOfSight = false;
		Strike->Accuracy = 0.90f;
		Strike->CritChanceBonus = 0.25f;
		Strike->Damage.DamageType = EDamageType::Kinetic;
		Strike->Damage.BaseAmount = 30.0f;
		Strike->Icon = LoadIcon(TEXT("T_Blue_Axe"));
		Strike->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Melee")));
	}

	// ---- 力学: 远程狙击, 高暴击 ----
	if (UBattleSkillData* Shoot = MakeSkill(TEXT("Shoot"), TEXT("远程穿透"), TEXT("远程狙击敌军 (力学伤害), 暴击率提升。")))
	{
		Shoot->Discipline = ESkillDiscipline::Kinetic;
		Shoot->RangeProfile = ESkillRangeProfile::Ranged;
		Shoot->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Shoot->TacticalRole = ESkillTacticalRole::Offensive;
		Shoot->APCost = 2;
		Shoot->Range = 1200.0f;
		Shoot->Accuracy = 0.90f;
		Shoot->CritChanceBonus = 0.15f;
		Shoot->Damage.DamageType = EDamageType::Kinetic;
		Shoot->Damage.BaseAmount = 12.0f;
		Shoot->Icon = LoadIcon(TEXT("T_Blue_Bow"));
		Shoot->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));
		Shoot->SkillTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Projectile")));
	}

	// ---- 力学: 力场护盾 (自身护盾) ----
	if (UBattleSkillData* Barrier = MakeSkill(TEXT("FieldBarrier"), TEXT("力场护盾"), TEXT("展开力场, 获得 25 点护盾 (优先吸收伤害)。")))
	{
		Barrier->Discipline = ESkillDiscipline::Kinetic;
		Barrier->RangeProfile = ESkillRangeProfile::Melee;
		Barrier->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Barrier->TacticalRole = ESkillTacticalRole::Defensive;
		Barrier->TargetFilter = ESkillTargetFilter::SelfOnly;
		Barrier->APCost = 1;
		Barrier->CooldownRounds = 2;
		Barrier->Range = 100.0f;
		Barrier->Accuracy = -1.0f;
		Barrier->Damage.BaseAmount = 0.0f;
		Barrier->Icon = LoadIcon(TEXT("T_Blue_Rune"));

		FAttributeModifier Shield;
		Shield.Attribute = EBattleAttribute::Barrier;
		Shield.Op = EModifierOp::Add;
		Shield.Magnitude = 25.0f;
		Barrier->SelfInstantEffects.Add(Shield);
		Barrier->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Defensive")));
	}

	// ---- 热学: 高热粒子, 挂燃烧 (单体启动技) ----
	if (UBattleSkillData* Overheat = MakeSkill(TEXT("Overheat"), TEXT("热能过载"), TEXT("倾泻高热粒子 (热学伤害), 施加燃烧。")))
	{
		Overheat->Discipline = ESkillDiscipline::Thermal;
		Overheat->RangeProfile = ESkillRangeProfile::Ranged;
		Overheat->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Overheat->TacticalRole = ESkillTacticalRole::Offensive;
		Overheat->APCost = 2;
		Overheat->CooldownRounds = 1;
		Overheat->Range = 650.0f;
		Overheat->Accuracy = 0.88f;
		Overheat->Damage.DamageType = EDamageType::Thermal;
		Overheat->Damage.BaseAmount = 18.0f;
		Overheat->Icon = LoadIcon(TEXT("T_Red_Flame"));
		Overheat->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));

		FStatusApplication BurnApply;
		BurnApply.Status = Burn;
		BurnApply.Stacks = 2;
		BurnApply.ApplyChance = 0.7f;
		Overheat->TargetStatuses.Add(BurnApply);
	}

	// ---- 热学: 灼烧脉冲 (自身为圆心的周身 AoE, 轻型) ----
	if (UBattleSkillData* Pulse = MakeSkill(TEXT("ScorchPulse"), TEXT("灼烧脉冲"), TEXT("以自身为圆心释放 300cm 灼热脉冲 (热学伤害), 波及周围敌军并点燃。")))
	{
		Pulse->Discipline = ESkillDiscipline::Thermal;
		Pulse->RangeProfile = ESkillRangeProfile::Melee;
		Pulse->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Pulse->TacticalRole = ESkillTacticalRole::Offensive;
		Pulse->TargetFilter = ESkillTargetFilter::SelfOnly;
		Pulse->APCost = 2;
		Pulse->CooldownRounds = 1;
		Pulse->Accuracy = 0.9f;
		Pulse->Damage.DamageType = EDamageType::Thermal;
		Pulse->Damage.BaseAmount = 10.0f;
		Pulse->AoE.Radius = 300.0f;
		Pulse->AoE.MinScale = 0.7f;
		Pulse->bRequiresLineOfSight = false;
		Pulse->Icon = LoadIcon(TEXT("T_Red_Sparkle"));
		Pulse->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.AoE")));

		FStatusApplication BurnApply;
		BurnApply.Status = Burn;
		BurnApply.Stacks = 1;
		BurnApply.ApplyChance = 0.8f;
		Pulse->TargetStatuses.Add(BurnApply);
	}

	// ---- 热学: 热能新星 (自身为圆心的周身 AoE, 重型) ----
	if (UBattleSkillData* Nova = MakeSkill(TEXT("NovaBlast"), TEXT("热能新星"), TEXT("以自身为圆心引爆 450cm 爆燃 (热学伤害), 边缘衰减, 有概率点燃。")))
	{
		Nova->Discipline = ESkillDiscipline::Thermal;
		Nova->RangeProfile = ESkillRangeProfile::Melee;
		Nova->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Nova->TacticalRole = ESkillTacticalRole::Offensive;
		Nova->TargetFilter = ESkillTargetFilter::SelfOnly;
		Nova->APCost = 3;
		Nova->CooldownRounds = 2;
		Nova->Accuracy = 0.85f;
		Nova->Damage.DamageType = EDamageType::Thermal;
		Nova->Damage.BaseAmount = 16.0f;
		Nova->AoE.Radius = 450.0f;
		Nova->AoE.MinScale = 0.6f;
		Nova->bRequiresLineOfSight = false;
		Nova->Icon = LoadIcon(TEXT("T_Red_Sun"));
		Nova->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.AoE")));

		FStatusApplication BurnApply;
		BurnApply.Status = Burn;
		BurnApply.Stacks = 1;
		BurnApply.ApplyChance = 0.6f;
		Nova->TargetStatuses.Add(BurnApply);
	}

	// ---- 电学: 电弧脉冲, 必挂导电 (单体启动技) ----
	if (UBattleSkillData* Shock = MakeSkill(TEXT("Shock"), TEXT("电弧激荡"), TEXT("发射高频电弧 (电学伤害), 施加导电。")))
	{
		Shock->Discipline = ESkillDiscipline::Electric;
		Shock->RangeProfile = ESkillRangeProfile::Ranged;
		Shock->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Shock->TacticalRole = ESkillTacticalRole::Offensive;
		Shock->APCost = 1;
		Shock->Range = 800.0f;
		Shock->Accuracy = 0.95f;
		Shock->Damage.DamageType = EDamageType::Electric;
		Shock->Damage.BaseAmount = 10.0f;
		Shock->Icon = LoadIcon(TEXT("T_Gold_Bolt"));
		Shock->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));

		FStatusApplication ConductiveApply;
		ConductiveApply.Status = Conductive;
		ConductiveApply.Stacks = 1;
		Shock->TargetStatuses.Add(ConductiveApply);
	}

	// ---- 电学: 链式电弧 (命中后弹跳 2 次) ----
	if (UBattleSkillData* Chain = MakeSkill(TEXT("ChainLightning"), TEXT("链式电弧"), TEXT("电弧命中后向 300cm 内最近敌军弹跳 2 次 (电学伤害), 每跳衰减, 可挂导电。")))
	{
		Chain->Discipline = ESkillDiscipline::Electric;
		Chain->RangeProfile = ESkillRangeProfile::Ranged;
		Chain->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Chain->TacticalRole = ESkillTacticalRole::Offensive;
		Chain->APCost = 2;
		Chain->Range = 700.0f;
		Chain->Accuracy = 0.9f;
		Chain->Damage.DamageType = EDamageType::Electric;
		Chain->Damage.BaseAmount = 16.0f;
		Chain->Chain.MaxJumps = 2;
		Chain->Chain.JumpRadius = 300.0f;
		Chain->Chain.JumpDamageScale = 0.6f;
		Chain->Icon = LoadIcon(TEXT("T_Gold_Sparkle"));
		Chain->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));
		Chain->SkillTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Chain")));

		FStatusApplication ConductiveApply;
		ConductiveApply.Status = Conductive;
		ConductiveApply.Stacks = 1;
		ConductiveApply.ApplyChance = 0.6f;
		Chain->TargetStatuses.Add(ConductiveApply);
	}

	// ---- 电学: 电弧风暴 (命中后弹跳 3 次, 广域挂导电) ----
	if (UBattleSkillData* Storm = MakeSkill(TEXT("ArcStorm"), TEXT("电弧风暴"), TEXT("释放风暴电弧, 命中后向 350cm 内敌军连续弹跳 3 次 (电学伤害), 施加导电。")))
	{
		Storm->Discipline = ESkillDiscipline::Electric;
		Storm->RangeProfile = ESkillRangeProfile::Ranged;
		Storm->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Storm->TacticalRole = ESkillTacticalRole::Offensive;
		Storm->APCost = 3;
		Storm->CooldownRounds = 2;
		Storm->Range = 800.0f;
		Storm->Accuracy = 0.88f;
		Storm->Damage.DamageType = EDamageType::Electric;
		Storm->Damage.BaseAmount = 12.0f;
		Storm->Chain.MaxJumps = 3;
		Storm->Chain.JumpRadius = 350.0f;
		Storm->Chain.JumpDamageScale = 0.55f;
		Storm->Icon = LoadIcon(TEXT("T_Gold_Sun"));
		Storm->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));
		Storm->SkillTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Chain")));

		FStatusApplication ConductiveApply;
		ConductiveApply.Status = Conductive;
		ConductiveApply.Stacks = 2;
		Storm->TargetStatuses.Add(ConductiveApply);
	}

	// ---- 化学: 轻型连击酸蚀喷射 (三段独立判定) ----
	if (UBattleSkillData* Acid = MakeSkill(TEXT("AcidSpray"), TEXT("酸蚀喷射"), TEXT("三段酸液连射 (化学伤害), 每段独立判定并可叠酸蚀。")))
	{
		Acid->Discipline = ESkillDiscipline::Chemical;
		Acid->RangeProfile = ESkillRangeProfile::Ranged;
		Acid->DeliveryWeight = ESkillDeliveryWeight::Light;
		Acid->TacticalRole = ESkillTacticalRole::Offensive;
		Acid->APCost = 2;
		Acid->CooldownRounds = 1;
		Acid->Range = 500.0f;
		Acid->Accuracy = 0.85f;
		Acid->HitCount = 3;
		Acid->HitInterval = 0.15f;
		Acid->Damage.DamageType = EDamageType::Chemical;
		Acid->Damage.BaseAmount = 6.0f;
		Acid->Icon = LoadIcon(TEXT("T_Purple_Droplet"));
		Acid->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));

		FStatusApplication CorrodeApply;
		CorrodeApply.Status = Corroded;
		CorrodeApply.Stacks = 1;
		CorrodeApply.ApplyChance = 0.5f;
		Acid->TargetStatuses.Add(CorrodeApply);
	}

	// ---- 化学: 酸蚀爆弹 (投掷落点 AoE) ----
	if (UBattleSkillData* Bomb = MakeSkill(TEXT("AcidBomb"), TEXT("酸蚀爆弹"), TEXT("向目标投掷酸蚀爆弹, 落点 300cm 溅射 (化学伤害), 波及范围内敌军并叠酸蚀。")))
	{
		Bomb->Discipline = ESkillDiscipline::Chemical;
		Bomb->RangeProfile = ESkillRangeProfile::Ranged;
		Bomb->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Bomb->TacticalRole = ESkillTacticalRole::Offensive;
		Bomb->APCost = 2;
		Bomb->CooldownRounds = 1;
		Bomb->Range = 700.0f;
		Bomb->Accuracy = 0.88f;
		Bomb->Damage.DamageType = EDamageType::Chemical;
		Bomb->Damage.BaseAmount = 14.0f;
		Bomb->AoE.Radius = 300.0f;
		Bomb->AoE.MinScale = 0.65f;
		Bomb->Icon = LoadIcon(TEXT("T_Purple_Potion"));
		Bomb->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));
		Bomb->SkillTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.AoE")));

		FStatusApplication CorrodeApply;
		CorrodeApply.Status = Corroded;
		CorrodeApply.Stacks = 1;
		CorrodeApply.ApplyChance = 0.8f;
		Bomb->TargetStatuses.Add(CorrodeApply);
	}

	// ---- 化学: 蚀雾榴弹 (大范围投掷 AoE, 重酸蚀) ----
	if (UBattleSkillData* Cloud = MakeSkill(TEXT("VenomCloud"), TEXT("蚀雾榴弹"), TEXT("向目标区域投掷蚀雾榴弹, 420cm 毒雾覆盖 (化学伤害), 高概率叠多层酸蚀。")))
	{
		Cloud->Discipline = ESkillDiscipline::Chemical;
		Cloud->RangeProfile = ESkillRangeProfile::Ranged;
		Cloud->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Cloud->TacticalRole = ESkillTacticalRole::Offensive;
		Cloud->APCost = 3;
		Cloud->CooldownRounds = 2;
		Cloud->Range = 650.0f;
		Cloud->Accuracy = 0.85f;
		Cloud->Damage.DamageType = EDamageType::Chemical;
		Cloud->Damage.BaseAmount = 8.0f;
		Cloud->AoE.Radius = 420.0f;
		Cloud->AoE.MinScale = 0.7f;
		Cloud->Icon = LoadIcon(TEXT("T_Purple_Skull"));
		Cloud->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Ranged")));
		Cloud->SkillTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.AoE")));

		FStatusApplication CorrodeApply;
		CorrodeApply.Status = Corroded;
		CorrodeApply.Stacks = 2;
		CorrodeApply.ApplyChance = 0.8f;
		Cloud->TargetStatuses.Add(CorrodeApply);
	}

	// ---- 电学辅助: 超频回路 (返还 AP) ----
	if (UBattleSkillData* Overclock = MakeSkill(TEXT("Overclock"), TEXT("超频回路"), TEXT("过载行动回路, 净赚 1 点行动点 (辅助)。")))
	{
		Overclock->Discipline = ESkillDiscipline::Electric;
		Overclock->RangeProfile = ESkillRangeProfile::Melee;
		Overclock->DeliveryWeight = ESkillDeliveryWeight::Heavy;
		Overclock->TacticalRole = ESkillTacticalRole::Support;
		Overclock->TargetFilter = ESkillTargetFilter::SelfOnly;
		Overclock->APCost = 1;
		Overclock->CooldownRounds = 3;
		Overclock->Range = 100.0f;
		Overclock->Accuracy = -1.0f;
		Overclock->Damage.BaseAmount = 0.0f;
		Overclock->Icon = LoadIcon(TEXT("T_Gold_Rune"));

		FAttributeModifier GainAP;
		GainAP.Attribute = EBattleAttribute::AP;
		GainAP.Op = EModifierOp::Add;
		GainAP.Magnitude = 2.0f;
		Overclock->SelfInstantEffects.Add(GainAP);
		Overclock->SkillTags = FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Support")));
	}

	// ============ 默认反应表 (设计文档 §6: 四学派 × 四状态) ============

	Interactions = NewObject<UBattleInteractionTable>(this);

	// 力学 × 破甲: 碎裂加深
	{
		FStatusReactionRow Row;
		Row.TriggerDamage = EDamageType::Kinetic;
		Row.RequiredStatus = FGameplayTag::RequestGameplayTag(FName("Status.Shattered"));
		Row.DamageMultiplier = 1.4f;
		FStatusApplication Bonus;
		Bonus.Status = Shattered;
		Bonus.Stacks = 1;
		Row.BonusStatuses.Add(Bonus);
		Interactions->Rows.Add(Row);
	}

	// 热学 × 酸蚀: 沸腾蒸发 + 引燃
	{
		FStatusReactionRow Row;
		Row.TriggerDamage = EDamageType::Thermal;
		Row.RequiredStatus = FGameplayTag::RequestGameplayTag(FName("Status.Corroded"));
		Row.DamageMultiplier = 1.5f;
		FStatusApplication Bonus;
		Bonus.Status = Burn;
		Bonus.Stacks = 1;
		Row.BonusStatuses.Add(Bonus);
		Interactions->Rows.Add(Row);
	}

	// 电学 × 导电: 连锁溅射
	{
		FStatusReactionRow Row;
		Row.TriggerDamage = EDamageType::Electric;
		Row.RequiredStatus = FGameplayTag::RequestGameplayTag(FName("Status.Conductive"));
		Row.DamageMultiplier = 1.5f;
		Row.bChain = true;
		Row.ChainRadius = 600.0f;
		Row.ChainDamageScale = 0.5f;
		Interactions->Rows.Add(Row);
	}

	// 化学 × 燃烧: 助燃
	{
		FStatusReactionRow Row;
		Row.TriggerDamage = EDamageType::Chemical;
		Row.RequiredStatus = FGameplayTag::RequestGameplayTag(FName("Status.Burn"));
		Row.DamageMultiplier = 1.5f;
		FStatusApplication Bonus;
		Bonus.Status = Burn;
		Bonus.Stacks = 1;
		Row.BonusStatuses.Add(Bonus);
		Interactions->Rows.Add(Row);
	}

	// 统一同步技能 ThemeColor 与各自学派色 (MakeSkill 构造时仅设默认力学蓝，需在学科确定后回填)
	for (UBattleSkillData* Skill : Skills)
	{
		if (Skill)
		{
			Skill->ThemeColor = UBattleSkillData::GetDisciplineDefaultColor(Skill->Discipline);
		}
	}
}