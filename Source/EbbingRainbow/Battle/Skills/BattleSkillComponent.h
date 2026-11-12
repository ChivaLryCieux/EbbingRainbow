// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleStatusEffect.h"
#include "BattleSkillComponent.generated.h"

class ABattleUnit;

DECLARE_MULTICAST_DELEGATE(FOnBattleSkillsetChanged);

/**
 * 技能组件 —— 单位的技能运行时 (顶层设计 §2/§3)。
 *
 * 职责:
 *   1. 技能实例 (FBattleSkillSpec): 装备/冷却状态 —— UI/AI 的统一索引
 *   2. 状态堆叠 (FBattleActiveStatus): 施加/叠层/驱散/回合生命周期
 *   3. 派生属性聚合: 基础值 + 状态修饰, 战斗公式只读有效值, 基础值永不被改写
 */
UCLASS(ClassGroup = (Battle), meta = (BlueprintSpawnableComponent))
class EBBINGRAINBOW_API UBattleSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleSkillComponent();

	// ---------- 持有者 ----------
	UFUNCTION(BlueprintPure, Category = "Battle|Skills")
	ABattleUnit* GetOwnerUnit() const;

	// ---------- 技能实例 ----------

	/** 装备技能 (同一数据资产可多次实例化, 各自独立冷却) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Skills")
	void GrantSkill(UBattleSkillData* SkillData, int32 Level = 1);

	/** 按 Id 批量装备 (Id 来自肉鸽构筑的持久化列表; 内置目录解析) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Skills")
	void GrantSkillsByIds(const TArray<FName>& SkillIds);

	UFUNCTION(BlueprintCallable, Category = "Battle|Skills")
	void ClearSkills();

	const TArray<FBattleSkillSpec>& GetSkillSpecs() const { return Skills; }
	int32 GetSkillCount() const { return Skills.Num(); }

	bool HasSkillIndex(int32 Index) const { return Skills.IsValidIndex(Index); }
	FBattleSkillSpec* GetSpec(int32 Index) { return Skills.IsValidIndex(Index) ? &Skills[Index] : nullptr; }
	const FBattleSkillSpec* GetSpec(int32 Index) const { return Skills.IsValidIndex(Index) ? &Skills[Index] : nullptr; }
	UBattleSkillData* GetSkillData(int32 Index) const;

	/** 技能是否就绪 (冷却中返回 false) */
	UFUNCTION(BlueprintPure, Category = "Battle|Skills")
	bool IsSkillReady(int32 Index) const;

	/** 施放成功后调用: 写入冷却 */
	void NotifySkillActivated(int32 Index);

	// ---------- 回合生命周期 (BattleManager 驱动) ----------

	/** 行动开始: 冷却快照 (本回合禁用判定) + 递减 */
	void NotifyTurnStart();

	/** 回合开始: RoundStart 状态跳动 */
	void NotifyRoundStart();

	/** 回合结束: RoundEnd 状态跳动 + 状态时长递减/到期 */
	void NotifyRoundEnd();

	// ---------- 状态效果 ----------

	/** 施加状态 (含叠层/免疫/驱散规则与概率判定) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Status")
	bool ApplyStatus(const FStatusApplication& Request, ABattleUnit* Source);

	bool ApplyStatusDef(UBattleStatusEffect* Def, int32 Stacks, int32 DurationOverride, ABattleUnit* Source);

	/** 驱散若干层 (Stacks<=0 全部移除) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Status")
	void RemoveStatus(UBattleStatusEffect* Def, int32 Stacks);

	UFUNCTION(BlueprintCallable, Category = "Battle|Status")
	void ClearAllStatuses();

	UFUNCTION(BlueprintPure, Category = "Battle|Status")
	bool HasStatusTag(FGameplayTag Tag) const;

	/** 持有者当前全部语义标签 = 状态标签 ∪ 状态授予标签 (供反应表/AI 查询) */
	FGameplayTagContainer GetStatusTags() const;

	const TArray<FBattleActiveStatus>& GetActiveStatuses() const { return ActiveStatuses; }

	/** 对外文案 (头顶标签): "燃烧 x2 (1) · 导电 (2)" */
	FString GetStatusSummary() const;

	// ---------- 派生属性 ----------

	/** 属性有效值 = 基础值 + 状态 Add(×层数) → 状态 Multiply^(层数); 已按规则钳制 */
	UFUNCTION(BlueprintPure, Category = "Battle|Attributes")
	float GetEffectiveAttribute(EBattleAttribute Attribute) const;

	float GetEffectiveEvasion() const;
	float GetEffectiveInitiative() const;
	float GetEffectiveAccuracy() const;
	float GetEffectiveCritChance() const;
	float GetEffectiveCritMultiplier() const;
	float GetEffectiveResistance(EDamageType DamageType) const;
	float GetEffectiveMaxIntegrity() const;
	int32 GetEffectiveMaxAP() const;

	// ---------- 事件 ----------

	/** 技能组/状态变化时广播 (UI 卡牌与状态条刷新) */
	FOnBattleSkillsetChanged OnSkillsetChanged;
	FOnBattleSkillsetChanged OnStatusesChanged;

private:
	UPROPERTY(EditAnywhere, Category = "Battle|Skills")
	TArray<FBattleSkillSpec> Skills;

	UPROPERTY(EditAnywhere, Category = "Battle|Status")
	TArray<FBattleActiveStatus> ActiveStatuses;

	/** 派生属性聚合: 收集某属性上的全部状态修饰 */
	void GatherModifiers(EBattleAttribute Attribute, TArray<FAttributeModifier>& OutMods, TArray<int32>& OutStacks) const;

	/** 状态跳动 (伤害/治疗); Timing 匹配才执行 */
	void TickStatuses(EStatusTickTiming Timing);
};