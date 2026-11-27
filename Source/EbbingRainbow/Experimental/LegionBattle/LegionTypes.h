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

/** 兵团顶层物理状态机 (保留既有语义以兼容 UI/AI 工具链与自检) */
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

/** 兵团宏观弱 AI 战术指令 (正交于底层物理态) */
UENUM(BlueprintType)
enum class ELegionDirective : uint8
{
	/** 休整待命: 放松巡视, 遇近敌自动警觉自卫 */
	Standby     UMETA(DisplayName = "休整"),
	/** 戒备固守: 锚定阵地/高地扇面警戒, 迎击来犯但不过度追击 */
	Guard       UMETA(DisplayName = "戒备"),
	/** 防区巡逻: 沿巡逻航点往返巡弋并驻留扫视 */
	Patrol      UMETA(DisplayName = "巡逻"),
	/** 探索侦察: 前出侧翼/高地/林缘索敌并向友军广播敌情 */
	Recon       UMETA(DisplayName = "侦察"),
	/** 主动进攻: 发现敌情或受令后包抄/压进交战 */
	Assault     UMETA(DisplayName = "进攻"),
	/** 指挥官直属: 玩家显式下达的机动/进攻/跟随命令 */
	PlayerOrder UMETA(DisplayName = "直属")
};

/** 兵团态势警戒分级 */
UENUM(BlueprintType)
enum class ELegionAlertness : uint8
{
	/** 放松: 垂枪呼吸、随机张望、小步漫步 */
	Relaxed UMETA(DisplayName = "放松"),
	/** 警戒: 举枪戒备、扇面扫视、巡哨碎步 */
	Alert   UMETA(DisplayName = "警戒"),
	/** 交战: 寻敌抢射界、立定射击、战术游走拉扯 */
	Combat  UMETA(DisplayName = "交战")
};

/** 单兵微观自治状态机 (同团内允许个体异步处于不同状态) */
UENUM(BlueprintType)
enum class ELegionSoldierState : uint8
{
	RelaxedIdle     UMETA(DisplayName = "放松待机"),
	RelaxedWander   UMETA(DisplayName = "放松漫步"),
	AlertScan       UMETA(DisplayName = "警戒扫视"),
	AlertPatrolStep UMETA(DisplayName = "警戒巡步"),
	FormationMarch  UMETA(DisplayName = "编队行军"),
	SeekingFireLine UMETA(DisplayName = "抢占射界"),
	CombatAimFire   UMETA(DisplayName = "瞄准射击"),
	CombatSkirmish  UMETA(DisplayName = "游走拉扯"),
	Dying           UMETA(DisplayName = "阵亡")
};

namespace ERLegionMeta
{
	inline const TCHAR* GetDirectiveName(ELegionDirective D)
	{
		switch (D)
		{
		case ELegionDirective::Standby:     return TEXT("休整");
		case ELegionDirective::Guard:       return TEXT("戒备");
		case ELegionDirective::Patrol:      return TEXT("巡逻");
		case ELegionDirective::Recon:       return TEXT("侦察");
		case ELegionDirective::Assault:     return TEXT("进攻");
		case ELegionDirective::PlayerOrder: return TEXT("直属");
		default:                            return TEXT("休整");
		}
	}

	inline const TCHAR* GetAlertnessName(ELegionAlertness A)
	{
		switch (A)
		{
		case ELegionAlertness::Relaxed: return TEXT("放松");
		case ELegionAlertness::Alert:   return TEXT("警戒");
		case ELegionAlertness::Combat:  return TEXT("交战");
		default:                        return TEXT("放松");
		}
	}

	inline const TCHAR* GetSoldierStateName(ELegionSoldierState S)
	{
		switch (S)
		{
		case ELegionSoldierState::RelaxedIdle:     return TEXT("放松待机");
		case ELegionSoldierState::RelaxedWander:   return TEXT("放松漫步");
		case ELegionSoldierState::AlertScan:       return TEXT("警戒扫视");
		case ELegionSoldierState::AlertPatrolStep: return TEXT("警戒巡步");
		case ELegionSoldierState::FormationMarch:  return TEXT("编队行军");
		case ELegionSoldierState::SeekingFireLine: return TEXT("抢占射界");
		case ELegionSoldierState::CombatAimFire:   return TEXT("瞄准射击");
		case ELegionSoldierState::CombatSkirmish:  return TEXT("游走拉扯");
		case ELegionSoldierState::Dying:           return TEXT("阵亡");
		default:                                   return TEXT("未知");
		}
	}
}

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
 * 士兵只是 HISM/SMC 实例 + 编队槽位 + 独立世界空间运动学与微状态机,
 * 既支持《绝境北地》式同团单兵异步行为与混合空间动画, 也可在大规模下退化为 HISM。
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

	/** 骨骼士兵组件 (仅骨骼士兵模式; 每兵一个 SMC, 单节点动画/混合空间) */
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

	// ---------------- 独立世界运动学与单兵自治微状态机 ----------------

	/** 士兵个体世界空间位置 (XY, cm): 支持围绕编队槽位的噪声漫步、抢射界与个人空间排斥 */
	FVector2D WorldPosXY = FVector2D::ZeroVector;

	/** 士兵个体世界空间速度 (XY, cm/s): 驱动 2D BlendSpace 方向与速率 */
	FVector2D WorldVelXY = FVector2D::ZeroVector;

	/** 相对兵团原点的当前局部偏移 (X, Y, GroundZ), 供骨骼与 HISM 路径共用 */
	FVector LocalOffset = FVector::ZeroVector;

	/** 微状态机驱动的战术偏移 (世界 XY, 如散步点、抢射界横跨步、游走侧滑) */
	FVector2D TacticalOffsetXY = FVector2D::ZeroVector;

	/** 当前单兵微观自治状态 */
	ELegionSoldierState SoldierState = ELegionSoldierState::RelaxedIdle;

	/** 个体感知到的警戒分级 (由 ReactionDelay 错峰同步自军团态势, 实现波浪式举枪) */
	ELegionAlertness PerceivedAlertness = ELegionAlertness::Relaxed;

	/** 当前微状态剩余倒计时 (s) */
	float StateTimer = 0.0f;

	/** 警戒切换反应延迟倒计时 (s) */
	float ReactionDelay = 0.0f;

	/** 空闲/警戒状态下的随机张望/扫视偏转角 (deg) */
	float GlanceYawOffset = 0.0f;

	/** 张望目标重选倒计时 (s) */
	float GlanceTimer = 0.0f;

	/** 确定性 2D 噪声种子 (每个士兵独立走位曲线) */
	float NoiseSeedX = 0.0f;
	float NoiseSeedY = 0.0f;

	/** 个体性格差异: 好动度 (0.5..1.5) / 侵略性 (0.6..1.4) / 步频倍率 (0.90..1.12) */
	float TraitRestlessness = 1.0f;
	float TraitAggression = 1.0f;
	float TraitPlayRate = 1.0f;

	/** 个体锁定的敌兵索引 (优先盯防同一目标减少高频切目标摇头) */
	int32 AssignedTargetSoldier = INDEX_NONE;

	/** 目标重选倒计时 (s) */
	float TargetReselectTimer = 0.0f;

	/** 当前点射连发剩余发数, 归零后触发战术游走拉扯 */
	int32 BurstShotsLeft = 4;

	/** 受压制值 (0..1, 邻兵中弹或擦弹升高, 触发闪避/游走后缓慢衰减) */
	float Suppression = 0.0f;

	/** 开火后坐力脉冲进度 (0..1, 驱动身体微后仰与步枪回弹) */
	float FireRecoilAlpha = 0.0f;

	/** 传给 BlendSpace 的平滑速率与相对移动角 (deg) */
	float SmoothedAnimSpeed = 0.0f;
	float SmoothedAnimDir = 0.0f;

	// ---------------- 运动稳定化与防抖动/抽搐字段 ----------------

	/** 附着在右手骨骼上的枪械静态网格组件 (仅枪械做微后坐力, 不晃动身体骨骼) */
	TWeakObjectPtr<class UStaticMeshComponent> GunComp;

	/** 世界空间平滑海拔 (cm): 解耦父 Actor Z 起伏, 消除双采样跳动 */
	float WorldZ = 0.0f;
	/** 精确射线与双线性网格之间的微偏差 (cm) */
	float RayGroundOffset = 0.0f;
	float SmoothedRayOffset = 0.0f;
	bool bWorldZInitialized = false;

	/** 表现层上次写入的局部变换缓存: 驻立士兵变换不变时整帧跳过组件写入与下游级联 */
	FVector LastRepLoc = FVector(1.0e9f, 0.0f, 0.0f);
	float LastRepYaw = 1.0e9f;

	/** 单次迈步选定的无冲突目标局部偏移 (相对 HomeWorldXY) */
	FVector2D MoveGoalOffsetXY = FVector2D::ZeroVector;
	/** 迈步起步时锁定的稳定行进朝向 (deg) */
	float StepFacingYaw = 0.0f;

	/** BlendSpace 速率死区滞回状态 & 切换最小驻留冷却 */
	bool bAnimWalking = false;
	float AnimSuiteHoldTimer = 0.0f;

	/** 多帧运动稳定性遥测 (测试窗口内记录单帧跃变与高频翻转) */
	float PrevTelZ = 0.0f;
	float PrevTelYaw = 0.0f;
	float PrevTelAnimDir = 0.0f;
	float PrevDeltaZ = 0.0f;
	float PrevDeltaYaw = 0.0f;
	float MaxFrameJumpZ = 0.0f;
	float MaxFrameJumpAnimDir = 0.0f;
	int32 ZFlipCount = 0;
	int32 YawFlipCount = 0;
	bool bTelPrimed = false;
};
