// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "BattleInteractionTable.generated.h"

class UBattleStatusEffect;

/**
 * 学派 × 状态 反应行 (顶层设计 §6) —— 涌现交互的数据载体。
 *
 * 结算管线在每次命中后查询 [伤害类型, 目标持有的状态] → 命中行即触发:
 * 倍率加深 / 追加状态 / 连锁溅射 / 消耗层数。四学派 × 四状态的全部互动
 * 不写一行技能代码, 纯数据长出来。
 */
USTRUCT(BlueprintType)
struct FStatusReactionRow
{
	GENERATED_BODY()

	/** 触发伤害类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	EDamageType TriggerDamage = EDamageType::Kinetic;

	/** 目标须持有的状态标签 (层级匹配) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	FGameplayTag RequiredStatus;

	/** 反应伤害倍率 (1.5 = 加深 50%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	float DamageMultiplier = 1.0f;

	/** 反应追加的状态 (如燃烧蔓延/导电加深) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	TArray<FStatusApplication> BonusStatuses;

	/** 触发后消耗目标状态层数 (0 = 不消耗) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	int32 ConsumeStacks = 0;

	// ---- 连锁溅射 (电击导电目标 → 波及附近带电者) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Chain")
	bool bChain = false;

	/** 连锁搜索半径 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Chain", meta = (ClampMin = "0"))
	float ChainRadius = 600.0f;

	/** 溅射伤害比例 (相对反应后伤害) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Chain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChainDamageScale = 0.5f;

	/** 连锁目标须持有的标签 (空 = 与 RequiredStatus 相同) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Chain")
	FGameplayTag ChainTag;
};

/**
 * 反应表 (可被数据资产整体替换)。
 */
UCLASS(Blueprintable, BlueprintType)
class EBBINGRAINBOW_API UBattleInteractionTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TArray<FStatusReactionRow> Rows;

	/** 找到第一条 [伤害类型 + 目标持有 RequiredStatus] 的反应行 */
	const FStatusReactionRow* FindReaction(EDamageType DamageType, const FGameplayTagContainer& TargetStatusTags) const;
};