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
	case EDamageType::Kinetic:    return FColor(240, 205, 160); // 暖白/力学浅褐
	case EDamageType::Thermal:    return FColor(255, 65, 30);   // 赤红烈火
	case EDamageType::Electric:   return FColor(60, 200, 255);  // 电弧青蓝
	case EDamageType::Chemical:   return FColor(90, 240, 60);   // 强酸亮绿
	case EDamageType::TrueDamage: return FColor(255, 235, 80);  // 金黄真伤
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

/** 技能定义 */
USTRUCT(BlueprintType)
struct FBattleSkillDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FString DisplayName = TEXT("Strike");

	/** AP 消耗 (整型行动点) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "0"))
	int32 APCost = 1;

	/** 射程 (厘米) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "10.0"))
	float Range = 250.0f;

	/** 基础伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "1"))
	int32 BaseDamage = 10;

	/** 伤害类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	EDamageType DamageType = EDamageType::Kinetic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FString Description = TEXT("Deal damage to an enemy");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FLinearColor Color = FLinearColor(1.0f, 0.4f, 0.1f);

	FBattleSkillDef() {}
	FBattleSkillDef(FName InId, const TCHAR* InName, int32 InAP, float InRange, int32 InDmg, EDamageType InType, const TCHAR* InDesc, FLinearColor InCol = FLinearColor(1.0f, 0.4f, 0.1f))
		: SkillId(InId), DisplayName(InName), APCost(InAP), Range(InRange), BaseDamage(InDmg), DamageType(InType), Description(InDesc), Color(InCol) {}

	// 兼容旧构造函数
	FBattleSkillDef(FName InId, const TCHAR* InName, float InAP, float InRange, int32 InDmg, const TCHAR* InDesc, FLinearColor InCol = FLinearColor(1.0f, 0.4f, 0.1f))
		: SkillId(InId), DisplayName(InName), APCost(FMath::Max(1, FMath::RoundToInt(InAP))), Range(InRange), BaseDamage(InDmg), DamageType(EDamageType::Kinetic), Description(InDesc), Color(InCol) {}
};

/** 单位基础数值 (8项核心战棋属性) */
USTRUCT(BlueprintType)
struct FBattleUnitStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString DisplayName = TEXT("Unit");

	// 1. 完整度 (生命值，Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Integrity", meta = (ClampMin = "1.0"))
	float MaxIntegrity = 50.0f;

	// 2. 行动点 (每回合恢复的行动力上限，Int)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Action", meta = (ClampMin = "1"))
	int32 MaxAP = 3;

	// 3. 先攻值 (决定行动顺位，同时决定 1 AP 兑换移动距离的转化率，Int)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Action")
	int32 Initiative = 10;

	// 4. 闪避率 (未命中或闪避概率 0.0 ~ 1.0，Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EvasionRate = 0.05f;

	// 5. 力学抗性 (负数为弱点，正数为减免，Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float KineticResistance = 0.0f;

	// 6. 热学抗性 (Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float ThermalResistance = 0.0f;

	// 7. 电学抗性 (Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float ElectricResistance = 0.0f;

	// 8. 化学抗性 (Float)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Resistances")
	float ChemicalResistance = 0.0f;

	// 兼容旧代码访问别名
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Deprecated")
	int32 MaxHP = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Deprecated")
	int32 Speed = 10;

	/** 携带的技能列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	TArray<FBattleSkillDef> Skills;

	FBattleUnitStats() {}

	// 全量 8 项属性构造函数
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
		, MaxHP(FMath::RoundToInt(InIntegrity))
		, Speed(InInit)
	{}

	// 简易构造函数 (带默认抗性)
	FBattleUnitStats(const TCHAR* InName, float InIntegrity, int32 InAP, int32 InInit, float InEvasion = 0.05f)
		: DisplayName(InName)
		, MaxIntegrity(InIntegrity)
		, MaxAP(InAP)
		, Initiative(InInit)
		, EvasionRate(InEvasion)
		, MaxHP(FMath::RoundToInt(InIntegrity))
		, Speed(InInit)
	{}

	// 兼容旧构造函数
	FBattleUnitStats(const TCHAR* InName, int32 InHP, float InAP, int32 InSpeed)
		: DisplayName(InName)
		, MaxIntegrity((float)InHP)
		, MaxAP(FMath::Max(1, FMath::RoundToInt(InAP)))
		, Initiative(InSpeed)
		, MaxHP(InHP)
		, Speed(InSpeed)
	{}
};
