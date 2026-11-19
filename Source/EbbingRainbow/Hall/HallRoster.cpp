// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallRoster.h"

#include "Battle/Core/BattleGameMode.h"

namespace
{
	/** 只属于"档案"的表现层文案; 数值不在这里。 */
	struct FHeroPresentation
	{
		const TCHAR* Callsign;
		const TCHAR* RoleTitle;
		const TCHAR* RoleCaption;
		const TCHAR* Tagline;
	};

	// 顺序与 GetDefaultPlayerRoster() 一致: 力学 / 热学 / 电学 / 化学
	const FHeroPresentation GPresentations[] = {
		{ TEXT("BASALT"),    TEXT("先锋"), TEXT("VANGUARD"),   TEXT("以身为墙，替全队吃掉第一轮伤害。") },
		{ TEXT("FLINT"),     TEXT("斥候"), TEXT("SKIRMISHER"), TEXT("先攻与位移的极限，抢在敌人反应之前结束战斗。") },
		{ TEXT("ARC"),       TEXT("术师"), TEXT("CASTER"),     TEXT("电弧在敌群间跳跃，一次施法改写整条战线。") },
		{ TEXT("VERDIGRIS"), TEXT("秘术"), TEXT("OCCULTIST"),  TEXT("酸雾缓慢侵蚀，让强敌在自己的重量里崩塌。") },
	};
}

FText FHallHeroProfile::GetSchoolName() const
{
	return ERHallRoster::GetSchoolName(School);
}

FText FHallHeroProfile::GetSchoolCaption() const
{
	return ERHallRoster::GetSchoolCaption(School);
}

FLinearColor FHallHeroProfile::GetSchoolColor() const
{
	return ERHallRoster::GetSchoolColor(School);
}

const TArray<FHallHeroProfile>& ERHallRoster::GetProfiles()
{
	static const TArray<FHallHeroProfile> Profiles = []()
	{
		TArray<FHallHeroProfile> Out;
		const TArray<FBattleUnitStats>& Roster = ABattleGameMode::GetDefaultPlayerRoster();
		const int32 NumPresentations = UE_ARRAY_COUNT(GPresentations);
		for (int32 Index = 0; Index < Roster.Num(); ++Index)
		{
			const FBattleUnitStats& Stats = Roster[Index];
			const FHeroPresentation& Pres = GPresentations[FMath::Clamp(Index, 0, NumPresentations - 1)];

			FHallHeroProfile Profile;
			Profile.HeroName = FText::FromString(Stats.DisplayName);
			Profile.Callsign = FText::FromString(Pres.Callsign);
			Profile.RoleTitle = FText::FromString(Pres.RoleTitle);
			Profile.RoleCaption = FText::FromString(Pres.RoleCaption);
			Profile.Tagline = FText::FromString(Pres.Tagline);
			Profile.School = Stats.School;
			Profile.Stats = Stats;
			Out.Add(Profile);
		}
		return Out;
	}();
	return Profiles;
}

FText ERHallRoster::GetSchoolName(EDamageType School)
{
	return FText::FromString(GetDamageTypeName(School));
}

FText ERHallRoster::GetSchoolCaption(EDamageType School)
{
	switch (School)
	{
	case EDamageType::Kinetic:  return FText::FromString(TEXT("KINETIC"));
	case EDamageType::Thermal:  return FText::FromString(TEXT("THERMAL"));
	case EDamageType::Electric: return FText::FromString(TEXT("ELECTRIC"));
	case EDamageType::Chemical: return FText::FromString(TEXT("CHEMICAL"));
	default:                    return FText::FromString(TEXT("UNKNOWN"));
	}
}

FLinearColor ERHallRoster::GetSchoolColor(EDamageType School)
{
	// 与战斗飘字/抗性条同源: 力学蓝 / 热学赤 / 电学金 / 化学紫
	return FLinearColor(GetDamageTypeColor(School));
}
