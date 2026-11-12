// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "BattleStatusEffect.generated.h"

/** 状态叠层策略 */
UENUM(BlueprintType)
enum class EStatusStackingPolicy : uint8
{
	/** 不叠加: 再次施加仅刷新时长 */
	RefreshDuration UMETA(DisplayName = "刷新时长"),
	/** 叠层数: 层数累加 (受 MaxStacks 限制), 时长取最大 */
	StackCount      UMETA(DisplayName = "叠层数"),
	/** 叠层 + 刷新时长 */
	StackAndRefresh UMETA(DisplayName = "叠层并刷新"),
	/** 严格不叠加: 已存在则拒绝 */
	Reject          UMETA(DisplayName = "拒绝重复")
};

/** 周期跳动时机 */
UENUM(BlueprintType)
enum class EStatusTickTiming : uint8
{
	None       UMETA(DisplayName = "无周期"),
	RoundStart UMETA(DisplayName = "每回合开始"),
	RoundEnd   UMETA(DisplayName = "每回合结束")
};

/**
 * 状态效果定义 (顶层设计 §3) —— 持续性变化的唯一载体。
 *
 * 纪律: 派生属性 (抗性/闪避/先攻/命中/暴击) 的任何变化必须以状态形式存在并
 * 声明持续回合; 瞬间改派生属性在技能载荷层被拒绝。这就是"没有永久破甲"的保证。
 */
UCLASS(Blueprintable, BlueprintType)
class EBBINGRAINBOW_API UBattleStatusEffect : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UBattleStatusEffect();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BattleStatus"), GetFName());
	}

	// ---------------- 身份 ----------------

	/** 语义标签, 如 Status.Burn。层级匹配: Status 匹配所有状态 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Identity")
	FGameplayTag StatusTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|UI")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|UI", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|UI")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|UI")
	FLinearColor ThemeColor = FLinearColor::White;

	// ---------------- 持续与叠层 ----------------

	/** 基础持续回合 (0 = 永久直到被驱散, 慎用) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Duration", meta = (ClampMin = "0"))
	int32 BaseDurationRounds = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Duration")
	EStatusStackingPolicy StackingPolicy = EStatusStackingPolicy::StackAndRefresh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Duration", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxStacks = 3;

	// ---------------- 效果 ----------------

	/** 持续属性修饰 (仅派生属性; 每层生效) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Effects")
	TArray<FAttributeModifier> OngoingModifiers;

	/** 周期跳动时机 (如燃烧的每回合伤害) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Effects")
	EStatusTickTiming TickTiming = EStatusTickTiming::RoundEnd;

	/** 周期伤害 (>0 走完整伤害管线: 可被抗性/护盾减免) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Effects")
	FDamagePayload TickDamage;

	/** 周期治疗 (每层) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Effects", meta = (ClampMin = "0"))
	float TickHealAmount = 0.0f;

	// ---------------- 标签交互 ----------------

	/** 激活期间授予持有者的标签 (供反应表/AI 查询) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Tags")
	FGameplayTagContainer GrantedTags;

	/** 持有期间免疫的状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Tags")
	FGameplayTagContainer ImmuneTags;

	/** 施加携带这些标签的状态时, 本状态被移除 (如"净化") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Tags")
	FGameplayTagContainer RemoveOnTags;

	/** 可被驱散 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Tags")
	bool bDispellable = true;

	bool HasTick() const { return TickTiming != EStatusTickTiming::None && (TickDamage.BaseAmount > 0.0f || TickHealAmount > 0.0f); }
};

/**
 * 状态运行时实例 (挂在 UBattleSkillComponent 上)。
 */
USTRUCT(BlueprintType)
struct FBattleActiveStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Status")
	TObjectPtr<UBattleStatusEffect> Def = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Status")
	int32 Stacks = 1;

	/**
	 * 剩余回合: >0 = 剩余回合数, 每回合结束 -1, 到 0 移除;
	 * -1 = 永久 (直到被驱散/覆盖)。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	int32 RemainingRounds = 0;

	/** 施加者 (可能已离场, 用弱引用语义: 不阻止 GC) */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	TObjectPtr<class ABattleUnit> Source = nullptr;

	bool IsPermanent() const { return RemainingRounds < 0; }
	bool IsExpired() const { return RemainingRounds == 0; }

	FString ToLabelString() const
	{
		if (!Def)
		{
			return FString();
		}
		FString Name = Def->DisplayName.IsEmpty() ? Def->GetName() : Def->DisplayName.ToString();
		if (Stacks > 1)
		{
			Name += FString::Printf(TEXT(" x%d"), Stacks);
		}
		if (RemainingRounds > 0)
		{
			Name += FString::Printf(TEXT(" (%d)"), RemainingRounds);
		}
		return Name;
	}
};