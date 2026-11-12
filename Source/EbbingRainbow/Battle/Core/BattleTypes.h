// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

UENUM(BlueprintType)
enum class EBattleTeam : uint8
{
	Player,
	Enemy
};

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	Initializing,
	RoundStart,
	TurnActive,
	ActionExecuting,
	BattleOver
};

UENUM(BlueprintType)
enum class EPlayerInteractionMode : uint8
{
	Idle,
	Moving,
	TargetingSkill
};

/** 伤害与抗性类型 (战棋核心规则) */
UENUM(BlueprintType)
enum class EDamageType : uint8
{
	Kinetic    UMETA(DisplayName = "力学 (Kinetic)"),
	Thermal    UMETA(DisplayName = "热学 (Thermal)"),
	Electric   UMETA(DisplayName = "电学 (Electric)"),
	Chemical   UMETA(DisplayName = "化学 (Chemical)"),
	TrueDamage UMETA(DisplayName = "真实伤害 (TrueDamage)")
};

inline FColor GetDamageTypeColor(EDamageType Type)
{
	switch (Type)
	{
	case EDamageType::Kinetic:    return FColor(80, 140, 255);  // 力学深蓝
	case EDamageType::Thermal:    return FColor(255, 65, 30);   // 赤红烈火
	case EDamageType::Electric:   return FColor(255, 190, 45);  // 电学鎏金
	case EDamageType::Chemical:   return FColor(175, 95, 240);  // 化学毒紫
	case EDamageType::TrueDamage: return FColor(245, 245, 245); // 纯白真伤
	default: return FColor::White;
	}
}

inline const TCHAR* GetDamageTypeName(EDamageType Type)
{
	switch (Type)
	{
	case EDamageType::Kinetic:    return TEXT("力学");
	case EDamageType::Thermal:    return TEXT("热学");
	case EDamageType::Electric:   return TEXT("电学");
	case EDamageType::Chemical:   return TEXT("化学");
	case EDamageType::TrueDamage: return TEXT("真实");
	default: return TEXT("未知");
	}
}

/**
 * 单位基础数值 (模板值 —— 战斗中永不被改写;
 * 抗性/闪避/先攻等的临时变化一律走状态效果, 见 BattleSkillTypes.h)。
 */
USTRUCT(BlueprintType)
struct FBattleUnitStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString DisplayName = TEXT("Unit");

	// 1. 完整度 (生命值, Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Integrity", meta = (ClampMin = "1.0"))
	float MaxIntegrity = 50.0f;

	// 2. 行动点 (每回合恢复的行动力上限, Int)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Action", meta = (ClampMin = "1"))
	int32 MaxAP = 3;

	// 3. 先攻值 (决定行动顺位 + 1 AP 兑换移动距离的转化率, Int)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Action")
	int32 Initiative = 10;

	// 4. 闪避率 (0.0 ~ 1.0, Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EvasionRate = 0.05f;

	// 5. 命中加成 (叠加到技能基础命中率)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Defense")
	float Accuracy = 0.0f;

	// 6. 暴击率 (0.0 ~ 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CritChance = 0.05f;

	// 7. 暴击倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Defense", meta = (ClampMin = "1.0"))
	float CritMultiplier = 1.5f;

	// 8~11. 四学派抗性 (负数为弱点, 正数为减免, Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float KineticResistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float ThermalResistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float ElectricResistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float ChemicalResistance = 0.0f;

	/** 携带的技能 Id 列表 (由 UBattleSkillLibrary 目录解析; 肉鸽构筑持久化同款) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	TArray<FName> SkillIds;

	FBattleUnitStats() {}

	// 全量属性构造函数 (默认 命中 0 / 暴击 5% x1.5)
	FBattleUnitStats(const TCHAR* InName, float InIntegrity, int32 InAP, int32 InInit, float InEvasion,
		float InKineticRes, float InThermalRes, float InElectricRes, float InChemicalRes)
		: DisplayName(InName)
		, MaxIntegrity(InIntegrity)
		, MaxAP(InAP)
		, Initiative(InInit)
		, EvasionRate(InEvasion)
		, KineticResistance(InKineticRes)
		, ThermalResistance(InThermalRes)
		, ElectricResistance(InElectricRes)
		, ChemicalResistance(InChemicalRes)
	{}

	// 简易构造函数 (带默认抗性)
	FBattleUnitStats(const TCHAR* InName, float InIntegrity, int32 InAP, int32 InInit, float InEvasion = 0.05f)
		: DisplayName(InName)
		, MaxIntegrity(InIntegrity)
		, MaxAP(InAP)
		, Initiative(InInit)
		, EvasionRate(InEvasion)
	{}
};