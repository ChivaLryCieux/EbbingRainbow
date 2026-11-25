// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "LegionTypes.generated.h"

class USkeletalMeshComponent;
class ALegionUnit;

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
	/** 编队横纵比 (越大越宽扁) */
	float ColumnBias = 2.0f;
	/** 士兵视觉尺度微调 */
	float BodyXYScale = 1.0f;

	static FLegionArchetypeStats GetForClass(ELegionClass InClass)
	{
		// 2026-12 真实弹道改造: 敌我全员统一为同一套射手数值。
		// 杀伤不再按兵种数值结算, 完全由实体弹道命中决定 (见 LegionGameMode 子弹模拟);
		// 兵种差异暂退居纯视觉/战役身份, 克制链等真实弹道版兵种特性成熟后再回归。
		FLegionArchetypeStats S;
		S.Class = InClass;
		S.MoveSpeed = 520.0f;
		S.SoldierHP = 40;             // 统一两枪倒 (步枪单发 25)
		S.DamagePerSoldierPerSec = 0.0f; // 已废弃: 不再有按秒自动结算伤害
		S.EngageRange = 2600.0f;      // 统一步枪射程
		S.bRanged = true;
		S.VolleyInterval = 1.9f;      // 单兵射击基础 Cadence (秒), 个体带 ±30% 抖动
		S.FrontalBlockRatio = 0.0f;
		S.BraceAntiMeleeMul = 1.0f;
		S.KnockbackImpulse = 0.0f;
		switch (InClass)
		{
		case ELegionClass::Shieldbearer: S.ColumnBias = 1.8f; S.BodyXYScale = 1.15f; break;
		case ELegionClass::Pikeman:      S.ColumnBias = 1.7f; S.BodyXYScale = 0.98f; break;
		case ELegionClass::Vanguard:     S.ColumnBias = 1.4f; S.BodyXYScale = 1.0f;  break;
		case ELegionClass::Marksman:     S.ColumnBias = 1.8f; S.BodyXYScale = 0.92f; break;
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
 * 真实弹道子弹 (2026-12 改造): 纯值类型轻量模拟, 不生成 Actor。
 * GameMode 逐帧积分推进, 线段扫描敌方士兵胶囊与地形静态物,
 * 只有几何上确实命中目标个体胶囊才结算伤害 —— 射不准就真的是打飞。
 */
struct FLegionBullet
{
	FVector Pos = FVector::ZeroVector;
	FVector PrevPos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	float Life = 0.0f;
	ELegionTeam Team = ELegionTeam::Player;
	TWeakObjectPtr<ALegionUnit> Source;
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

	/** 子弹命中代理胶囊 (仅骨骼士兵模式; 挂在 SkelComp 上, 只挡 WorldDynamic) */
	TWeakObjectPtr<class UCapsuleComponent> HitProxy;

	/** 朝向散布角 (deg), 确定性生成, 让队列不死板 */
	float JitterYaw = 0.0f;

	/** 个体当前朝向 (世界 yaw, deg): 士兵各自平滑转向 */
	float CurYaw = 0.0f;

	/** 个体贴地补偿 (局部 Z, cm): 每兵按地形高度平滑贴合, 坡上不悬空不钻地 */
	float GroundZ = 0.0f;

	bool bAlive = true;

	/** 个体当前生命值 (确保指挥官精准射击与兵团交战共享一致血量池, 射中谁就扣谁的血/死谁) */
	float HP = 0.0f;

	/** 致死击飞方向 (世界 XY; 子弹击杀时沿弹道方向击飞) */
	FVector DeathImpulseDir = FVector::ZeroVector;

	/** 步伐/交战突刺的相位种子 (确定性, 非真随机) */
	float Phase = 0.0f;

	/** 阵亡坍缩进度, >=0 表示正在播死亡动画 (0..1) */
	float DieT = -1.0f;

	/** 布娃娃已开启 (仅骨骼士兵模式) */
	bool bRagdolled = false;

	/** 距下次可开火剩余秒数 (真实弹道射击 Cadence; 逐兵独立带相位抖动) */
	float FireTimer = 0.0f;

	/** 开火动画播放截止的世界时间 (<=0 表示未在播开火动画) */
	float FireAnimEndTime = -1.0f;
};
