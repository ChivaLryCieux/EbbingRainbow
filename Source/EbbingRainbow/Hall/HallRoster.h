// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Core/BattleTypes.h"
#include "HallRoster.generated.h"

class UTexture2D;

/**
 * 大厅「人员管理」面板的角色档案。
 *
 * 数值唯一真相源仍然是 ABattleGameMode::GetDefaultPlayerRoster()（战斗与 Run 共用），
 * 本结构只在其上补一层"档案表现"：代号、定位、一句标语、可选立绘。
 * 名字来自 Stats.DisplayName，学派来自 Stats.School —— 名字与学派是两件事。
 */
USTRUCT(BlueprintType)
struct EBBINGRAINBOW_API FHallHeroProfile
{
	GENERATED_BODY()

	/** 角色名 (磐 / 燧 / 霆 / 蚀) */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	FText HeroName;

	/** 拉丁代号 (BASALT / FLINT / ARC / VERDIGRIS) */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	FText Callsign;

	/** 战术定位中文 (先锋 / 斥候 / 术师 / 秘术) */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	FText RoleTitle;

	/** 战术定位拉丁 (VANGUARD / SKIRMISHER / CASTER / OCCULTIST) */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	FText RoleCaption;

	/** 一句档案标语 */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	FText Tagline;

	/** 所属学派 */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	EDamageType School = EDamageType::Kinetic;

	/** 战斗模板数值 (完整度/AP/先攻/闪避/暴击/四学派抗性/技能 Id) */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	FBattleUnitStats Stats;

	/** 可选的专属立绘; 为空时面板回退到占位图 */
	UPROPERTY(BlueprintReadOnly, Category = "Hall|Hero")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** 学派中文名 (力学 / 热学 / 电学 / 化学) */
	FText GetSchoolName() const;

	/** 学派拉丁名 (KINETIC / THERMAL / ELECTRIC / CHEMICAL) */
	FText GetSchoolCaption() const;

	/** 学派主题色 (与战斗飘字/抗性条同源) */
	FLinearColor GetSchoolColor() const;
};

namespace ERHallRoster
{
	/** 四名先遣特勤的档案 (顺序与 ABattleGameMode::GetDefaultPlayerRoster() 一一对应) */
	EBBINGRAINBOW_API const TArray<FHallHeroProfile>& GetProfiles();

	/** 学派中文名; 未知类型回退"未知" */
	EBBINGRAINBOW_API FText GetSchoolName(EDamageType School);

	/** 学派拉丁名 (用于小字标签) */
	EBBINGRAINBOW_API FText GetSchoolCaption(EDamageType School);

	/** 学派主题色 */
	EBBINGRAINBOW_API FLinearColor GetSchoolColor(EDamageType School);
}
