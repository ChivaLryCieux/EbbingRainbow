// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Battle/Core/BattleTypes.h"
#include "BattleSkillTypes.generated.h"

class ABattleUnit;
class UBattleStatusEffect;

/**
 * 技能系统 2.0 核心类型 (顶层设计见 Docs/SkillSystem-2.0.md)
 *
 * 属性模型三层:
 *   当前值 (Integrity/Barrier/AP)   —— 瞬间效果唯一允许改动的对象
 *   派生值 (抗性/闪避/先攻/命中/暴击) —— 任何变化必须走状态效果并声明持续回合
 *   修饰符 FAttributeModifier       —— 两层共用的原子操作 (Add/Multiply/Override)
 */

/** 战斗属性枚举 (8 项经典 + 命中/暴击轴) */
UENUM(BlueprintType)
enum class EBattleAttribute : uint8
{
	// ---- 当前值 (瞬间效果专用) ----
	Integrity     UMETA(DisplayName = "完整度 (当前)"),
	Barrier       UMETA(DisplayName = "护盾 (当前)"),
	AP            UMETA(DisplayName = "行动点 (当前)"),

	// ---- 派生值 (仅状态效果可改) ----
	MaxIntegrity       UMETA(DisplayName = "完整度上限"),
	MaxAP              UMETA(DisplayName = "行动点上限"),
	Initiative         UMETA(DisplayName = "先攻"),
	Evasion            UMETA(DisplayName = "闪避率"),
	Accuracy           UMETA(DisplayName = "命中加成"),
	CritChance         UMETA(DisplayName = "暴击率"),
	CritMultiplier     UMETA(DisplayName = "暴击倍率"),
	KineticResistance  UMETA(DisplayName = "力学抗性"),
	ThermalResistance  UMETA(DisplayName = "热学抗性"),
	ElectricResistance UMETA(DisplayName = "电学抗性"),
	ChemicalResistance UMETA(DisplayName = "化学抗性")
};

/** 属性修饰操作 */
UENUM(BlueprintType)
enum class EModifierOp : uint8
{
	Add       UMETA(DisplayName = "加法 (Add)"),
	Multiply  UMETA(DisplayName = "乘法 (Multiply, 输入增量如 -0.25 表示 x0.75)"),
	Override  UMETA(DisplayName = "覆盖 (Override)")
};

/**
 * 属性修饰符 —— 效果系统的原子。
 * 瞬间效果: 只允许作用于当前值 (Integrity/Barrier/AP)。
 * 持续效果 (状态): 只允许作用于派生值, 按 Stacks 线性放大。
 */
USTRUCT(BlueprintType)
struct FAttributeModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	EBattleAttribute Attribute = EBattleAttribute::Integrity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	EModifierOp Op = EModifierOp::Add;

	/** Add: 加值; Multiply: 增量系数 (-0.25 = -25%); Override: 目标值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	float Magnitude = 0.0f;

	bool IsValidForCurrent() const
	{
		return Attribute == EBattleAttribute::Integrity
			|| Attribute == EBattleAttribute::Barrier
			|| Attribute == EBattleAttribute::AP;
	}

	bool IsValidForDerived() const { return !IsValidForCurrent(); }
};

/** 状态效果施加请求 (技能载荷里的"挂状态"条目) */
USTRUCT(BlueprintType)
struct FStatusApplication
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	TObjectPtr<UBattleStatusEffect> Status = nullptr;

	/** 本次施加的层数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status", meta = (ClampMin = "1"))
	int32 Stacks = 1;

	/** >=0 覆盖状态资产里的基础持续回合; <0 用资产默认 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	int32 DurationOverride = -1;

	/** 施加概率 [0,1] (走战斗随机流) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ApplyChance = 1.0f;
};

/** 范围效果规格 (以落点为中心的球形收集) */
USTRUCT(BlueprintType)
struct FAoESpec
{
	GENERATED_BODY()

	/** 半径 (cm), 0 = 非范围技能 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoE", meta = (ClampMin = "0"))
	float Radius = 0.0f;

	/** 距离衰减: 边缘伤害系数 (1 = 无衰减) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoE", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoE")
	bool bHitEnemies = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoE")
	bool bHitAllies = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoE")
	bool bHitSelf = false;

	/** 最多命中目标数 (0 = 不限), 超出按距离由近及远截断 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoE", meta = (ClampMin = "0"))
	int32 MaxTargets = 0;

	bool IsArea() const { return Radius > 0.0f; }

	float ScaleAtDistance(float Dist) const
	{
		if (Radius <= KINDA_SMALL_NUMBER || MinScale >= 1.0f)
		{
			return 1.0f;
		}
		const float Alpha = FMath::Clamp(Dist / Radius, 0.0f, 1.0f);
		return FMath::Lerp(1.0f, MinScale, Alpha);
	}
};

/**
 * 连锁规格 (电学学派特色): 单体技能命中主目标后, 电弧自动跳向最近的其它敌人。
 * 每次跳跃: 以上一个命中者位置为圆心, 在 JumpRadius 内找最近的未命中敌人;
 * 伤害 = 上一跳 x JumpDamageScale (独立命中/暴击/抗性判定)。
 */
USTRUCT(BlueprintType)
struct FChainSpec
{
	GENERATED_BODY()

	/** 最大跳跃次数 (0 = 不连锁) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "0", ClampMax = "6"))
	int32 MaxJumps = 0;

	/** 每跳的搜索半径 (cm, 以上一个命中者为圆心) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "0"))
	float JumpRadius = 260.0f;

	/** 每跳伤害衰减系数 (相对上一跳) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JumpDamageScale = 0.6f;

	/** 每跳间隔 (秒, 表现节奏) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "0.0"))
	float JumpInterval = 0.09f;

	bool IsChain() const { return MaxJumps > 0; }
};

/** 伤害载荷 */
USTRUCT(BlueprintType)
struct FDamagePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EDamageType DamageType = EDamageType::Kinetic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseAmount = 0.0f;
};

/** 单次命中结算结果 (事件载荷) */
USTRUCT(BlueprintType)
struct FBattleHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	TObjectPtr<ABattleUnit> Attacker = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	TObjectPtr<ABattleUnit> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	EDamageType DamageType = EDamageType::Kinetic;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bEvaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bCrit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bReactionTriggered = false;

	/** 命中判定使用的目标命中率 (诊断/教学用) */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float HitChance = 1.0f;

	/** 进入抗性前的最终基础伤害 (含暴击/地形/浮动) */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float RolledDamage = 0.0f;

	/** 抗性减免后 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float MitigatedDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float BarrierAbsorbed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float IntegrityDamage = 0.0f;
};

/** 一次伤害事件 (护盾/完整度实际变化) */
USTRUCT(BlueprintType)
struct FBattleDamageEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<ABattleUnit> Source = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<ABattleUnit> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EDamageType DamageType = EDamageType::Kinetic;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float BarrierAbsorbed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float IntegrityDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bKilled = false;
};

/** 技能执行上下文 */
USTRUCT(BlueprintType)
struct FBattleSkillExecutionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Skill")
	TObjectPtr<ABattleUnit> Caster = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Skill")
	TObjectPtr<ABattleUnit> Target = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Skill")
	FVector TargetLocation = FVector::ZeroVector;
};

// ---------------- 属性工具 ----------------

inline const TCHAR* GetBattleAttributeName(EBattleAttribute Attribute)
{
	switch (Attribute)
	{
	case EBattleAttribute::Integrity:            return TEXT("完整度");
	case EBattleAttribute::Barrier:              return TEXT("护盾");
	case EBattleAttribute::AP:                  return TEXT("行动点");
	case EBattleAttribute::MaxIntegrity:        return TEXT("完整度上限");
	case EBattleAttribute::MaxAP:               return TEXT("行动点上限");
	case EBattleAttribute::Initiative:          return TEXT("先攻");
	case EBattleAttribute::Evasion:             return TEXT("闪避率");
	case EBattleAttribute::Accuracy:            return TEXT("命中加成");
	case EBattleAttribute::CritChance:          return TEXT("暴击率");
	case EBattleAttribute::CritMultiplier:      return TEXT("暴击倍率");
	case EBattleAttribute::KineticResistance:   return TEXT("力学抗性");
	case EBattleAttribute::ThermalResistance:   return TEXT("热学抗性");
	case EBattleAttribute::ElectricResistance:  return TEXT("电学抗性");
	case EBattleAttribute::ChemicalResistance:  return TEXT("化学抗性");
	default:                                    return TEXT("未知");
	}
}

inline bool IsCurrentAttribute(EBattleAttribute Attribute)
{
	return Attribute == EBattleAttribute::Integrity
		|| Attribute == EBattleAttribute::Barrier
		|| Attribute == EBattleAttribute::AP;
}

/** 派生属性的有效值聚合: 先加后乘, Multiply 按 (1+m)^stacks 展开 */
inline float AggregateDerivedAttribute(float Base, const TArray<FAttributeModifier>& Mods, const TArray<int32>& StacksPerMod)
{
	float AddSum = 0.0f;
	float MultFactor = 1.0f;

	for (int32 Index = 0; Index < Mods.Num(); ++Index)
	{
		const FAttributeModifier& Mod = Mods[Index];
		const int32 Stacks = StacksPerMod.IsValidIndex(Index) ? FMath::Max(1, StacksPerMod[Index]) : 1;

		switch (Mod.Op)
		{
		case EModifierOp::Add:
			AddSum += Mod.Magnitude * Stacks;
			break;
		case EModifierOp::Multiply:
			MultFactor *= FMath::Pow(1.0f + Mod.Magnitude, (float)Stacks);
			break;
		case EModifierOp::Override:
			return Mod.Magnitude;
		default:
			break;
		}
	}

	return (Base + AddSum) * MultFactor;
}