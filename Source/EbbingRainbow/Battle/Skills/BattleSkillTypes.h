// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Core/BattleTypes.h"
#include "BattleSkillTypes.generated.h"

class ABattleUnit;

/** 1. 技能派系 (四大学科) */
UENUM(BlueprintType)
enum class ESkillDiscipline : uint8
{
	Kinetic    UMETA(DisplayName = "力学系 (Kinetic)"),
	Thermal    UMETA(DisplayName = "热学系 (Thermal)"),
	Electric   UMETA(DisplayName = "电学系 (Electric)"),
	Chemical   UMETA(DisplayName = "化学系 (Chemical)")
};

/** 2. 射程规格 */
UENUM(BlueprintType)
enum class ESkillRangeProfile : uint8
{
	Melee      UMETA(DisplayName = "近程 (Melee)"),
	Ranged     UMETA(DisplayName = "远程 (Ranged)")
};

/** 3. 运载重量 / 结算节奏 */
UENUM(BlueprintType)
enum class ESkillDeliveryWeight : uint8
{
	Light      UMETA(DisplayName = "轻型 (多次/低数值连击)"),
	Heavy      UMETA(DisplayName = "重型 (一次/高数值爆发)")
};

/** 4. 战术功能类型 */
UENUM(BlueprintType)
enum class ESkillTacticalRole : uint8
{
	Offensive  UMETA(DisplayName = "攻击型 (Offensive)"),
	Defensive  UMETA(DisplayName = "防御型 (Defensive)"),
	Support    UMETA(DisplayName = "辅助型 (Support)")
};

/** 目标作用阵营 */
UENUM(BlueprintType)
enum class ESkillTargetFilter : uint8
{
	EnemyOnly   UMETA(DisplayName = "仅敌方"),
	AllyOrSelf  UMETA(DisplayName = "友方或自身"),
	SelfOnly    UMETA(DisplayName = "仅自身"),
	Any         UMETA(DisplayName = "无差别")
};

/**
 * 复合属性修饰包 (一次性修改/影响 8 大属性与护盾)
 * 填 0 表示不影响；负数通常代表伤害/减益，正数代表治疗/强化
 */
USTRUCT(BlueprintType)
struct FAttributeModifierBundle
{
	GENERATED_BODY()

	// 1. 完整度变化 (负数为扣减完整度/受到伤害，正数为恢复完整度)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Integrity")
	float IntegrityDelta = 0.0f;

	// 临时护盾变化 (正数为赋予护盾，负数为破坏护盾)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Integrity")
	float BarrierDelta = 0.0f;

	// 2. 行动点变化 (如扣除目标 1 AP，或技能返还 1 AP)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Action")
	int32 APDelta = 0;

	// 3. 先攻值变化 (正数为提速排前，负数为震荡推后行动顺位)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Action")
	int32 InitiativeDelta = 0;

	// 4. 闪避率变化 (如 -0.15 表示削弱 15% 闪避率)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Defense")
	float EvasionDelta = 0.0f;

	// 5. 力学抗性变化 (如 -0.2 表示破坏力学装甲，加深力学伤害)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Resistances")
	float KineticResistanceDelta = 0.0f;

	// 6. 热学抗性变化 (如 -0.25 造成过热/易燃)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Resistances")
	float ThermalResistanceDelta = 0.0f;

	// 7. 电学抗性变化 (如 -0.2 造成导电浸润)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Resistances")
	float ElectricResistanceDelta = 0.0f;

	// 8. 化学抗性变化 (如 -0.3 造成酸蚀溶解)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Resistances")
	float ChemicalResistanceDelta = 0.0f;

	bool HasAnyEffect() const
	{
		return !FMath::IsNearlyZero(IntegrityDelta)
			|| !FMath::IsNearlyZero(BarrierDelta)
			|| APDelta != 0
			|| InitiativeDelta != 0
			|| !FMath::IsNearlyZero(EvasionDelta)
			|| !FMath::IsNearlyZero(KineticResistanceDelta)
			|| !FMath::IsNearlyZero(ThermalResistanceDelta)
			|| !FMath::IsNearlyZero(ElectricResistanceDelta)
			|| !FMath::IsNearlyZero(ChemicalResistanceDelta);
	}
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
