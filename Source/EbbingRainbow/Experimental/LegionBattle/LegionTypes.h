// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "LegionTypes.generated.h"

class USkeletalMeshComponent;

/** 兵团阵营 (与战斗系统 EBattleTeam 语义对齐, 但独立定义避免耦合) */
UENUM(BlueprintType)
enum class ELegionTeam : uint8
{
	Player UMETA(DisplayName = "我方"),
	Enemy  UMETA(DisplayName = "敌方")
};

/** 兵团顶层状态机 */
UENUM(BlueprintType)
enum class ELegionState : uint8
{
	/** 原地驻守 */
	Holding  UMETA(DisplayName = "驻守"),
	/** 向目标点/目标兵团机动 */
	Moving   UMETA(DisplayName = "机动"),
	/** 已与目标兵团接战 (互相结算伤亡) */
	Engaged  UMETA(DisplayName = "交战"),
	/** 全员阵亡 */
	Wiped    UMETA(DisplayName = "覆灭")
};

/** 兵种数值原型表 (数据驱动克制链) */
struct FLegionArchetypeStats
{
	ELegionClass Class = ELegionClass::Shieldbearer;
	float MoveSpeed = 480.0f;
	int32 SoldierHP = 36;
	float DamagePerSoldierPerSec = 6.0f;
	float EngageRange = 360.0f;
	/** > 0 表示远程兵种 (会在 VolleyInterval 周期触发齐射弹道) */
	bool bRanged = false;
	float VolleyInterval = 0.95f;
	/** 正面扇区格挡减伤率 (0..0.9) */
	float FrontalBlockRatio = 0.0f;
	/** 静止驻守时对抗突击兵/近战冲击的额外倍率 */
	float BraceAntiMeleeMul = 1.0f;
	/** 近战冲击施加的击退速度 (cm/s) */
	float KnockbackImpulse = 0.0f;
	/** 主动技冷却 (s) 与持续时间 (s) */
	float SkillCooldown = 14.0f;
	float SkillDuration = 5.0f;
	/** 编队横纵比 (越大越宽扁) */
	float ColumnBias = 2.0f;
	/** 士兵视觉尺度微调 */
	float BodyXYScale = 1.0f;

	static FLegionArchetypeStats GetForClass(ELegionClass InClass)
	{
		FLegionArchetypeStats S;
		S.Class = InClass;
		switch (InClass)
		{
		case ELegionClass::Shieldbearer:
			S.MoveSpeed = 420.0f;
			S.SoldierHP = 48;
			S.DamagePerSoldierPerSec = 4.6f;
			S.EngageRange = 320.0f;
			S.bRanged = false;
			S.FrontalBlockRatio = 0.65f; // 正面抗线极硬, 怕侧背
			S.BraceAntiMeleeMul = 1.15f;
			S.KnockbackImpulse = 90.0f;
			S.SkillCooldown = 14.0f;
			S.SkillDuration = 6.0f;
			S.ColumnBias = 1.8f; // 稳固方阵, 适配任意朝向个人转身
			S.BodyXYScale = 1.15f;
			break;

		case ELegionClass::Pikeman:
			S.MoveSpeed = 480.0f;
			S.SoldierHP = 34;
			S.DamagePerSoldierPerSec = 6.2f;
			S.EngageRange = 420.0f;
			S.bRanged = false;
			S.FrontalBlockRatio = 0.25f;
			S.BraceAntiMeleeMul = 1.85f; // 静止驻守专克突击冲阵
			S.KnockbackImpulse = 260.0f;
			S.SkillCooldown = 12.0f;
			S.SkillDuration = 1.5f;
			S.ColumnBias = 1.7f;
			S.BodyXYScale = 0.98f;
			break;

		case ELegionClass::Vanguard:
			S.MoveSpeed = 660.0f;
			S.SoldierHP = 32;
			S.DamagePerSoldierPerSec = 8.4f;
			S.EngageRange = 300.0f;
			S.bRanged = false;
			S.FrontalBlockRatio = 0.15f;
			S.BraceAntiMeleeMul = 1.0f;
			S.KnockbackImpulse = 190.0f;
			S.SkillCooldown = 11.0f;
			S.SkillDuration = 5.0f;
			S.ColumnBias = 1.4f;
			S.BodyXYScale = 1.0f;
			break;

		case ELegionClass::Marksman:
			S.MoveSpeed = 500.0f;
			S.SoldierHP = 22;
			S.DamagePerSoldierPerSec = 6.8f;
			S.EngageRange = 2300.0f; // 远程射程 (配合高地可达 ~3000cm, 远小于开局间距)
			S.bRanged = true;
			S.VolleyInterval = 1.05f;
			S.FrontalBlockRatio = 0.0f;
			S.BraceAntiMeleeMul = 0.65f; // 被贴身弱势
			S.KnockbackImpulse = 70.0f;
			S.SkillCooldown = 15.0f;
			S.SkillDuration = 4.0f;
			S.ColumnBias = 1.8f;
			S.BodyXYScale = 0.92f;
			break;
		}
		return S;
	}
};

/** 远程齐射弹道视觉线段 (供 HUD/世界绘制轨迹) */
struct FLegionTracerBeam
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Remaining = 0.25f;
	float Duration = 0.25f;
};

/**
 * 单个士兵的实例化状态 —— 纯值类型, 无 UObject。
 * 士兵只是 HISM 实例索引 + 编队槽位 + 少量动画相位, 不承担任何独立逻辑,
 * 这是本实验能把同屏单位数推到 1000+ 的核心设计。
 */
USTRUCT(BlueprintType)
struct FLegionSoldier
{
	GENERATED_BODY()

	/** 编队局部槽位偏移 (兵团局部空间, X=前后纵深, Y=左右翼展) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Legion")
	FVector HomeOffset = FVector::ZeroVector;

	/** 主体 HISM 实例索引 */
	int32 InstanceIndex = INDEX_NONE;

	/** 附属 HISM 实例索引 (盾牌/长戟/步枪/头饰层) */
	int32 ExtraIndex = INDEX_NONE;

	/** 骨骼士兵组件 (仅骨骼士兵模式; 每兵一个 SMC, 单节点动画) */
	TWeakObjectPtr<USkeletalMeshComponent> SkelComp;

	/** 朝向散布角 (deg), 确定性生成, 让队列不死板 */
	float JitterYaw = 0.0f;

	/** 个体当前朝向 (世界 yaw, deg): 士兵各自平滑转向 */
	float CurYaw = 0.0f;

	/** 个体贴地补偿 (局部 Z, cm): 每兵按地形高度平滑贴合, 坡上不悬空不钻地 */
	float GroundZ = 0.0f;

	bool bAlive = true;

	/** 步伐/交战突刺的相位种子 (确定性, 非真随机) */
	float Phase = 0.0f;

	/** 阵亡坍缩进度, >=0 表示正在播死亡动画 (0..1) */
	float DieT = -1.0f;

	/** 布娃娃已开启 (仅骨骼士兵模式) */
	bool bRagdolled = false;
};
