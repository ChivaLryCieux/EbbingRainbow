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

/** 技能定义 */
USTRUCT(BlueprintType)
struct FBattleSkillDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FString DisplayName = TEXT("Strike");

	/** AP 消耗 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "0.0"))
	float APCost = 4.0f;

	/** 射程 (厘米) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "10.0"))
	float Range = 250.0f;

	/** 基础伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "1"))
	int32 BaseDamage = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FString Description = TEXT("Deal damage to an enemy");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FLinearColor Color = FLinearColor(1.0f, 0.4f, 0.1f);

	FBattleSkillDef() {}
	FBattleSkillDef(FName InId, const TCHAR* InName, float InAP, float InRange, int32 InDmg, const TCHAR* InDesc, FLinearColor InCol = FLinearColor(1.0f, 0.4f, 0.1f))
		: SkillId(InId), DisplayName(InName), APCost(InAP), Range(InRange), BaseDamage(InDmg), Description(InDesc), Color(InCol) {}
};

/** 单位基础数值 */
USTRUCT(BlueprintType)
struct FBattleUnitStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString DisplayName = TEXT("Unit");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "1"))
	int32 MaxHP = 30;

	/** 每回合行动值 (移动与技能共用) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "1"))
	float MaxAP = 10.0f;

	/** 先攻速度: 每轮开始按此降序排队 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Speed = 10;

	/** 携带的技能列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	TArray<FBattleSkillDef> Skills;

	FBattleUnitStats() {}
	FBattleUnitStats(const TCHAR* InName, int32 InHP, float InAP, int32 InSpeed)
		: DisplayName(InName), MaxHP(InHP), MaxAP(InAP), Speed(InSpeed) {}
};
