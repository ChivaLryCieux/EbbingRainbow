// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "BattleSkillData.generated.h"

class ABattleUnit;

/**
 * 战棋技能定义基类 (UPrimaryDataAsset)
 * 基于 4 维正交分类枚举，承载对敌我双方 8 项核心战棋属性的复合修改
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

	/** 技能学派 / 派系 (力学、热学、电学、化学) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillDiscipline Discipline = ESkillDiscipline::Kinetic;

	/** 射程规格: 近程 / 远程 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillRangeProfile RangeProfile = ESkillRangeProfile::Melee;

	/** 运载重量: 轻型 (多次/低数值连击) / 重型 (单次/高数值爆发) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Classification")
	ESkillDeliveryWeight DeliveryWeight = ESkillDeliveryWeight::Heavy;

	/** 战术功能类型: 攻击 / 防御 / 辅助 */
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
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|UI")
	FLinearColor ThemeColor = FLinearColor(1.0f, 0.4f, 0.1f);

	/** 行动点 (AP) 消耗 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules", meta = (ClampMin = "0"))
	int32 APCost = 1;

	/** 冷却回合数 (0 表示每回合可用) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules", meta = (ClampMin = "0"))
	int32 CooldownRounds = 0;

	/** 施法射程 (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules", meta = (ClampMin = "10.0"))
	float Range = 250.0f;

	/** 目标筛选规则 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Rules")
	ESkillTargetFilter TargetFilter = ESkillTargetFilter::EnemyOnly;

	// ---------------- 3. 运载规格 (轻型 vs 重型) ----------------

	/**
	 * 打击段数 / 次数:
	 * 重型技能通常为 1 段高伤爆发；
	 * 轻型技能通常为 2~4 段连续打击 (每段均受闪避与抗性独立判定)。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Delivery", meta = (ClampMin = "1", ClampMax = "10"))
	int32 HitCount = 1;

	/** 连续打击时每段之间的间隔 (秒) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Delivery", meta = (ClampMin = "0.05"))
	float HitInterval = 0.15f;

	// ---------------- 4. 对 8 项核心属性的影响配置 (同时影响多种属性) ----------------

	/** 对目标生效的复合属性修改 (支持同时影响完整度、抗性、AP、先攻等) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	FAttributeModifierBundle TargetModifiers;

	/** 对施法者自身生效的复合属性修改 (如自身获得护盾、增加先攻、修改抗性等) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	FAttributeModifierBundle SelfModifiers;

	// ---------------- 5. 表现与动画 ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Visuals")
	TObjectPtr<class UAnimSequence> CastAnimation;

	// ---------------- 6. 核心接口虚函数 ----------------

	/** 检查是否满足施放条件 */
	virtual bool CanActivate(const ABattleUnit* Caster, const FBattleSkillExecutionContext& Context, FString& OutReason) const;

	/** 执行技能效果 (支持多态或派生子类扩展) */
	virtual bool Execute(const FBattleSkillExecutionContext& Context);

	/** 蓝图自定义逻辑扩展入口 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill", meta = (DisplayName = "OnExecuteSkill"))
	bool K2_Execute(const FBattleSkillExecutionContext& Context);

	/** 派系对应的默认伤害类型 */
	EDamageType GetDisciplineDamageType() const;

	/** 派系对应的默认主题色 */
	static FLinearColor GetDisciplineDefaultColor(ESkillDiscipline InDiscipline);

protected:
	/** 应用复合属性修改包到指定单位 */
	virtual void ApplyModifierBundle(ABattleUnit* Receiver, ABattleUnit* SourceUnit, const FAttributeModifierBundle& Bundle);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
