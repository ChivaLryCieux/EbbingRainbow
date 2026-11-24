// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Battle/FX/BattleVFXTypes.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "BattleSkillData.generated.h"

class ABattleUnit;
class UBattleStatusEffect;

/** 1. 技能学派 (四大学科) —— 决定伤害类型与反应表入口 */
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
	Ranged    UMETA(DisplayName = "远程 (Ranged)")
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
 * 战棋技能定义 (UPrimaryDataAsset) —— 技能的唯一真相源。
 *
 * 技能 = 分类标签 + 目标规则 + 投射规则 + 效果载荷 的组合体 (顶层设计 §4)。
 * 新技能 = 新数据; 新机制 = 新状态/新反应表行, 不动本类。
 *
 * 双轨制已废除: 运行时实例为 FBattleSkillSpec, 由 UBattleSkillComponent 持有。
 */
UCLASS(Blueprintable, BlueprintType)
class EBBINGRAINBOW_API UBattleSkillData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UBattleSkillData();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BattleSkill"), GetFName());
	}

	// ---------------- 1. 核心四大枚举 ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillDiscipline Discipline = ESkillDiscipline::Kinetic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillRangeProfile RangeProfile = ESkillRangeProfile::Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillDeliveryWeight DeliveryWeight = ESkillDeliveryWeight::Heavy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillTacticalRole TacticalRole = ESkillTacticalRole::Offensive;

	// ---------------- 2. 基础信息与开销 ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|UI")
	FName SkillId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|UI")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|UI", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|UI")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|UI")
	FLinearColor ThemeColor = FLinearColor(1.0f, 0.4f, 0.1f);

	/** 行动点 (AP) 消耗 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules", meta = (ClampMin = "0"))
	int32 APCost = 1;

	/**
	 * 冷却回合数 (0 = 每回合可用)。
	 * 语义: 施放后接下来 N 个自己的回合不可用 (见设计文档 §4)。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules", meta = (ClampMin = "0"))
	int32 CooldownRounds = 0;

	/** 施法射程 (cm, 3D 距离 —— 多层战场上下层不能隔空互殴) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules", meta = (ClampMin = "10.0"))
	float Range = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules")
	ESkillTargetFilter TargetFilter = ESkillTargetFilter::EnemyOnly;

	/** 需要视线 (地形查询 LOS); 近战默认不需要 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules")
	bool bRequiresLineOfSight = true;

	// ---------------- 3. 命中模型 ----------------

	/**
	 * 基础命中率 [0,1]; <0 = 必中 (跳过判定, 但不影响暴击)。
	 * 最终命中率 = Skill.Accuracy - 目标闪避 + 地形/侧翼修正, clamp [0.05, 0.99]。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Accuracy", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Accuracy = 0.95f;

	/** 本技能追加暴击率 (叠加到施法者暴击率) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Accuracy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CritChanceBonus = 0.0f;

	/** 本技能追加暴击倍率 (叠加到施法者暴击倍率) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Accuracy")
	float CritMultiplierBonus = 0.0f;

	// ---------------- 4. 运载规格 (轻型 vs 重型) ----------------

	/** 打击段数: 轻型连击每段独立命中/暴击/浮动判定 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Delivery", meta = (ClampMin = "1", ClampMax = "10"))
	int32 HitCount = 1;

	/** 连击段间隔 (秒) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Delivery", meta = (ClampMin = "0.05"))
	float HitInterval = 0.15f;

	/** 伤害浮动 (0.1 = ±10%, 走战斗随机流) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Delivery", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DamageVariance = 0.10f;

	// ---------------- 5. 效果载荷 ----------------

	/** 命中伤害载荷 (每段) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	FDamagePayload Damage;

	/** 范围规格 (Radius=0 单体; >0 以落点为中心收集) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	FAoESpec AoE;

	/** 连锁规格 (单体技能专用: 命中后电弧跳跃, 电学学派特色) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	FChainSpec Chain;

	/** 对目标的瞬间属性修改 (只允许当前值: 完整度/护盾/AP) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	TArray<FAttributeModifier> TargetInstantEffects;

	/** 对施法者自身的瞬间属性修改 (如返还 AP / 自身护盾) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	TArray<FAttributeModifier> SelfInstantEffects;

	/** 命中后对目标施加的状态 (未命中不挂) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	TArray<FStatusApplication> TargetStatuses;

	/** 施放后对自身施加的状态 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	TArray<FStatusApplication> SelfStatuses;

	// ---------------- 6. 语义标签 (供反应表/AI/遗物查询) ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Tags")
	FGameplayTagContainer SkillTags;

	// ---------------- 7. 表现 ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Visuals")
	TObjectPtr<class UAnimSequence> CastAnimation = nullptr;

	/** 技能特效表现配置 (Niagara 槽位 / 可选蓝图演出覆盖 / 主题色) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Visuals")
	FBattleSkillVFXConfig VFX;

	// ---------------- 8. 接口 ----------------

	/** 检查是否满足施放条件 (AP/冷却/目标筛选/3D 射程; 视线由结算层查地形) */
	virtual bool CanActivate(const ABattleUnit* Caster, const FBattleSkillExecutionContext& Context, FString& OutReason) const;

	/** 派系对应的默认伤害类型 */
	EDamageType GetDisciplineDamageType() const;

	/** 派系对应的默认主题色 */
	static FLinearColor GetDisciplineDefaultColor(ESkillDiscipline InDiscipline);

	/** 蓝图自定义逻辑扩展入口 (结算管线在全部效果落地后调用) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill", meta = (DisplayName = "OnExecuteSkill"))
	void K2_Execute(const FBattleSkillExecutionContext& Context);

	/** UI 用伤害预估 (每段基础值 × 段数) */
	float GetEstimatedTotalDamage() const { return Damage.BaseAmount * FMath::Max(1, HitCount); }

	bool IsAutoHit() const { return Accuracy < 0.0f; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * 技能运行时实例 —— UI/AI/控制器的统一索引对象 (取代旧 FBattleSkillDef)。
 * 由 UBattleSkillComponent 持有; 冷却状态与数据分离, 同一数据资产可多处实例化。
 */
USTRUCT(BlueprintType)
struct FBattleSkillSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UBattleSkillData> SkillData = nullptr;

	/** 技能等级 (肉鸽构筑成长位; 当前 1 级 = 基础值) */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	int32 Level = 1;

	/** 剩余冷却回合 (见设计文档 §4 语义) */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	int32 CooldownRemaining = 0;

	/** 本回合是否被冷却禁用 (行动开始时快照) */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	bool bBlockedThisTurn = false;

	/** 冷却中 = 本回合被快照禁用 或 计数未归零 (施放当回合立即生效) */
	bool IsOnCooldown() const { return bBlockedThisTurn || CooldownRemaining > 0; }

	FString GetCooldownLabel() const
	{
		if (bBlockedThisTurn)
		{
			return FString::Printf(TEXT("冷却中 %d"), FMath::Max(1, CooldownRemaining));
		}
		if (CooldownRemaining > 0)
		{
			return FString::Printf(TEXT("冷却 %d"), CooldownRemaining);
		}
		return FString();
	}
};