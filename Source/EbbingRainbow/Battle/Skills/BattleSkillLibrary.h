// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "BattleSkillLibrary.generated.h"

class UBattleSkillData;
class UBattleStatusEffect;
class UBattleInteractionTable;

/**
 * 内置技能内容工厂 + 目录 (顶层设计 §9)。
 *
 * 纯代码构建的默认技能/状态/反应表 —— 无任何 .uasset 也能跑完整战斗;
 * 等美术资产就位后, 同 Id 的 DataAsset 目录可整体接管 (FindSkillById 优先查目录)。
 * 同时是肉鸽技能获取的解析入口: 持久化的 SkillIds 都从这里落回战斗。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleSkillLibrary : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UBattleSkillLibrary* Get(const UWorld* World);

	/** 按 Id 找技能 (内置目录; 未找到返回 nullptr) */
	UBattleSkillData* FindSkillById(FName SkillId) const;

	/** 按标签找状态 */
	UBattleStatusEffect* FindStatusByTag(FGameplayTag StatusTag) const;

	/** 学派 × 状态 反应表 */
	UBattleInteractionTable* GetInteractionTable() const { return Interactions; }

	const TArray<TObjectPtr<UBattleSkillData>>& GetAllSkills() const { return Skills; }

private:
	void BuildDefaults();
	UBattleStatusEffect* MakeStatus(FGameplayTag Tag, const TCHAR* Name, const TCHAR* Desc, int32 Duration,
		int32 MaxStacks, FLinearColor Color);
	UBattleSkillData* MakeSkill(FName Id, const TCHAR* Name, const TCHAR* Desc);

	UPROPERTY()
	TArray<TObjectPtr<UBattleStatusEffect>> Statuses;

	UPROPERTY()
	TArray<TObjectPtr<UBattleSkillData>> Skills;

	UPROPERTY()
	TObjectPtr<UBattleInteractionTable> Interactions = nullptr;
};