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

/** 目标属性类型 (对应 8 项核心战棋属性) */
UENUM(BlueprintType)
enum class ETargetAttributeType : uint8
{
	Integrity           UMETA(DisplayName = "完整度 (生命值)"),
	AP                  UMETA(DisplayName = "行动点 (AP)"),
	Initiative          UMETA(DisplayName = "先攻值"),
	EvasionRate         UMETA(DisplayName = "闪避率"),
	KineticResistance   UMETA(DisplayName = "力学抗性"),
	ThermalResistance   UMETA(DisplayName = "热学抗性"),
	ElectricResistance  UMETA(DisplayName = "电学抗性"),
	ChemicalResistance  UMETA(DisplayName = "化学抗性"),
	Barrier             UMETA(DisplayName = "护盾 (屏障)")
};

/** 属性修改方式 */
UENUM(BlueprintType)
enum class EAttributeModifierOp : uint8
{
	AddFlat             UMETA(DisplayName = "直接加减数值"),
	AddPercentage       UMETA(DisplayName = "百分比修饰"),
	Override            UMETA(DisplayName = "强制覆盖")
};

/** 技能对某项属性的具体效果配置 */
USTRUCT(BlueprintType)
struct FAttributeEffectDef
{
	GENERATED_BODY()

	/** 影响的目标属性 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	ETargetAttributeType TargetAttribute = ETargetAttributeType::Integrity;

	/** 修改方式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	EAttributeModifierOp Op = EAttributeModifierOp::AddFlat;

	/** 基础数值 (攻击伤害/治疗为正，削弱抗性/扣减为负) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float Value = 10.0f;

	/** 持续回合数 (0 表示即时永久结算，>0 表示临时 Buff/Debuff) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0"))
	int32 DurationRounds = 0;
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
