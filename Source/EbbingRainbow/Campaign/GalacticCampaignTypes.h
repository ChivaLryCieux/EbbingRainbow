// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GalacticCampaignTypes.generated.h"

/** 银河主要委托阵营 */
UENUM(BlueprintType)
enum class EFactionId : uint8
{
	Empire   UMETA(DisplayName = "泰拉帝国"),
	Rebels   UMETA(DisplayName = "自由叛军"),
	Dominion UMETA(DisplayName = "边缘自治领")
};

/** 虹膜链接兵团兵种原型 */
UENUM(BlueprintType)
enum class ELegionClass : uint8
{
	/** 重盾卫队: 正面高额格挡与抗线减伤, 侧面/背面脆弱 */
	Shieldbearer UMETA(DisplayName = "重盾卫队"),
	/** 长戟方阵: 静止驻守时对冲锋与近战造成高额反制击退 */
	Pikeman      UMETA(DisplayName = "长戟方阵"),
	/** 突击刃兵: 高机动近战破阵, 绕后撕裂远程与侧翼 */
	Vanguard     UMETA(DisplayName = "突击刃兵"),
	/** 精确射手: 远程齐射, 高地射程/伤害显著加成, 怕被近身 */
	Marksman     UMETA(DisplayName = "精确射手")
};

/** 阵营动态态势 */
USTRUCT(BlueprintType)
struct FFactionStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	EFactionId Faction = EFactionId::Empire;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString CodeName;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString DoctrineBrief;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FLinearColor Color = FLinearColor::White;

	/** 基金会声望 (-100 .. +100) */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 Reputation = 0;

	/** 银河战区总控制率 (三方之和恒为 100%) */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	float GalacticControl = 33.3f;
};

/** 银河战区节点 (用于星区态势与委托落点) */
USTRUCT(BlueprintType)
struct FGalacticSector
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 SectorId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString Code;

	/** 星区图归一化坐标 (0..1) */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FVector2D MapPos = FVector2D(0.5f, 0.5f);

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	EFactionId ControllingFaction = EFactionId::Empire;

	/** 战火激烈程度 (0..1) */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	float ConflictIntensity = 0.5f;
};

/** 玩家常备虹膜链接兵团档案 */
USTRUCT(BlueprintType)
struct FManagedLegionData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 LegionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString Callsign;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	ELegionClass Archetype = ELegionClass::Shieldbearer;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 MaxSoldiers = 20;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 AliveSoldiers = 20;

	/** 虹膜 AI 同步等级 (1..5), 提升兵团伤害/机动/协议冷却 */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 SyncLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 BattlesSurvived = 0;

	/** 单个缺失兵员的补员单价 */
	int32 GetReinforceCostPerSoldier() const { return 12; }
	int32 GetMissingSoldiers() const { return FMath::Max(0, MaxSoldiers - AliveSoldiers); }
	int32 GetFullReinforceCost() const { return GetMissingSoldiers() * GetReinforceCostPerSoldier(); }
	int32 GetSyncUpgradeCost() const { return SyncLevel >= 5 ? 0 : 280 * SyncLevel; }
};

/** 合同中单支敌军波次编成 */
USTRUCT(BlueprintType)
struct FEnemyWaveLegionSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	ELegionClass Archetype = ELegionClass::Vanguard;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 SoldierCount = 16;

	/** 0 = 开局即列阵; >0 = 战斗开始后延迟空投/增援秒数 */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	float DelaySeconds = 0.0f;

	/** 横向部署偏移 (-1..+1 映射战线宽度) */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	float LaneBias = 0.0f;
};

/** 阵营军事委托合同 */
USTRUCT(BlueprintType)
struct FGalacticContract
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 ContractId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString Briefing;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	EFactionId Employer = EFactionId::Empire;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	EFactionId EnemyFaction = EFactionId::Rebels;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 TargetSectorId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 TerrainSeed = 1001;

	/** 威胁星级 (1..5) */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 DangerRating = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 RewardCredits = 450;

	/** 胜出后雇主在银河控制度/星区势力上的推进量 */
	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	float ControlShift = 6.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 ReputationGain = 15;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	TArray<FEnemyWaveLegionSpec> EnemyWaves;

	int32 GetTotalEnemySoldiers() const
	{
		int32 Sum = 0;
		for (const FEnemyWaveLegionSpec& W : EnemyWaves) { Sum += W.SoldierCount; }
		return Sum;
	}
};

/** 最近一次军团战斗结算记录 */
USTRUCT(BlueprintType)
struct FLegionBattleOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	bool bVictory = false;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	bool bFromContract = false;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	FString ContractTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	EFactionId Employer = EFactionId::Empire;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	EFactionId EnemyFaction = EFactionId::Rebels;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 CreditsEarned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	float ControlShiftApplied = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 ReputationDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 PlayerCasualties = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Campaign")
	int32 EnemyEliminated = 0;
};

namespace ERGalacticMeta
{
	inline FString GetFactionName(EFactionId Faction)
	{
		switch (Faction)
		{
		case EFactionId::Empire:   return TEXT("泰拉帝国");
		case EFactionId::Rebels:   return TEXT("自由叛军");
		case EFactionId::Dominion: return TEXT("边缘自治领");
		default: return TEXT("未知势力");
		}
	}

	/** 势力简称 (UI 排印铁律: 中英文不得并存, 代号一律中文) */
	inline FString GetFactionCode(EFactionId Faction)
	{
		switch (Faction)
		{
		case EFactionId::Empire:   return TEXT("帝国");
		case EFactionId::Rebels:   return TEXT("叛军");
		case EFactionId::Dominion: return TEXT("自治领");
		default: return TEXT("未知");
		}
	}

	/** 一..十 的中文序数 (呼号用), 超过 10 回退阿拉伯数字 */
	inline FString GetChineseOrdinal(int32 N)
	{
		static const TCHAR* Digits[] = { TEXT("零"), TEXT("一"), TEXT("二"), TEXT("三"), TEXT("四"),
			TEXT("五"), TEXT("六"), TEXT("七"), TEXT("八"), TEXT("九"), TEXT("十") };
		return (N >= 0 && N <= 10) ? FString(Digits[N]) : FString::FromInt(N);
	}

	inline FLinearColor GetFactionColor(EFactionId Faction)
	{
		switch (Faction)
		{
		case EFactionId::Empire:   return FLinearColor(0.96f, 0.56f, 0.04f, 1.0f); // 帝国暖金
		case EFactionId::Rebels:   return FLinearColor(0.84f, 0.14f, 0.10f, 1.0f); // 叛军朱红
		case EFactionId::Dominion: return FLinearColor(0.05f, 0.56f, 0.94f, 1.0f); // 自治领冷青
		default: return FLinearColor::White;
		}
	}

	inline FString GetClassName(ELegionClass C)
	{
		switch (C)
		{
		case ELegionClass::Shieldbearer: return TEXT("重盾卫队");
		case ELegionClass::Pikeman:      return TEXT("长戟方阵");
		case ELegionClass::Vanguard:     return TEXT("突击刃兵");
		case ELegionClass::Marksman:     return TEXT("精确射手");
		default: return TEXT("未知兵团");
		}
	}

	/** 兵种简称 (中文, 用于番号牌/呼号等空间紧凑处) */
	inline FString GetClassCode(ELegionClass C)
	{
		switch (C)
		{
		case ELegionClass::Shieldbearer: return TEXT("重盾");
		case ELegionClass::Pikeman:      return TEXT("长戟");
		case ELegionClass::Vanguard:     return TEXT("刃兵");
		case ELegionClass::Marksman:     return TEXT("射手");
		default: return TEXT("兵团");
		}
	}

	inline FString GetClassRoleDesc(ELegionClass C)
	{
		switch (C)
		{
		case ELegionClass::Shieldbearer: return TEXT("正面高额格挡与抗线减伤 / 惧侧背突袭");
		case ELegionClass::Pikeman:      return TEXT("驻守反制冲锋与击退 / 惧远程火力");
		case ELegionClass::Vanguard:     return TEXT("高机动近战突进 / 切后排与侧翼包抄");
		case ELegionClass::Marksman:     return TEXT("远程齐射与高地射程压制 / 怕贴身肉搏");
		default: return TEXT("");
		}
	}

	/** 部队番号命名: 第一旅 / 第二旅 / 第三旅 ... */
	inline FString GetBrigadeName(int32 Ordinal)
	{
		return FString::Printf(TEXT("第%s旅"), *GetChineseOrdinal(FMath::Max(1, Ordinal)));
	}

	/** 敌军部队番号: 委托战带阵营简称(如 帝国第一旅), 演武模式为 敌第一旅 */
	inline FString GetEnemyBrigadeName(int32 Ordinal, bool bHasFaction = false, EFactionId Faction = EFactionId::Rebels)
	{
		if (bHasFaction)
		{
			return FString::Printf(TEXT("%s%s"), *GetFactionCode(Faction), *GetBrigadeName(Ordinal));
		}
		return FString::Printf(TEXT("敌%s"), *GetBrigadeName(Ordinal));
	}

	/** 部队占位 Logo 纹理路径 (暂用技能图标占位: 我方蓝色 / 敌方红色) */
	inline const TCHAR* GetClassLogoAssetPath(ELegionClass C, bool bAlly)
	{
		if (bAlly)
		{
			switch (C)
			{
			case ELegionClass::Shieldbearer: return TEXT("/Game/UI/SkillIcons/T_Blue_Shield.T_Blue_Shield");
			case ELegionClass::Pikeman:      return TEXT("/Game/UI/SkillIcons/T_Blue_Axe.T_Blue_Axe");
			case ELegionClass::Vanguard:     return TEXT("/Game/UI/SkillIcons/T_Blue_Sword.T_Blue_Sword");
			case ELegionClass::Marksman:     return TEXT("/Game/UI/SkillIcons/T_Blue_Bow.T_Blue_Bow");
			default:                         return TEXT("/Game/UI/SkillIcons/T_Blue_Shield.T_Blue_Shield");
			}
		}
		switch (C)
		{
		case ELegionClass::Shieldbearer: return TEXT("/Game/UI/SkillIcons/T_Red_Shield.T_Red_Shield");
		case ELegionClass::Pikeman:      return TEXT("/Game/UI/SkillIcons/T_Red_Axe.T_Red_Axe");
		case ELegionClass::Vanguard:     return TEXT("/Game/UI/SkillIcons/T_Red_Sword.T_Red_Sword");
		case ELegionClass::Marksman:     return TEXT("/Game/UI/SkillIcons/T_Red_Bow.T_Red_Bow");
		default:                         return TEXT("/Game/UI/SkillIcons/T_Red_Shield.T_Red_Shield");
		}
	}
}
