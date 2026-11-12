// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleTerrainQuery.generated.h"

class ABattleUnit;

/** 掩体等级 */
UENUM(BlueprintType)
enum class ECoverQuality : uint8
{
	None UMETA(DisplayName = "无掩体"),
	Half UMETA(DisplayName = "半掩 (命中-15%, 伤害-25%)"),
	Full UMETA(DisplayName = "全掩 (无视线)")
};

/** 目标朝向关系 */
UENUM(BlueprintType)
enum class EFacingRelation : uint8
{
	Front UMETA(DisplayName = "正面"),
	Side  UMETA(DisplayName = "侧翼 (闪避减半)"),
	Rear  UMETA(DisplayName = "背后 (闪避归零, 暴击+15%)")
};

/**
 * 一次地形结算上下文 —— 结算管线读取, 查询本身无副作用。
 */
USTRUCT(BlueprintType)
struct FBattleTerrainContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	bool bHasLineOfSight = true;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	ECoverQuality Cover = ECoverQuality::None;

	/** 攻击者脚底 - 目标脚底 的 Z 差 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	float HeightDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	EFacingRelation Facing = EFacingRelation::Front;

	// ---- 规则修正 (设计文档 §7) ----

	/** 高低差伤害倍率 (真实伤害不受此修正) */
	float GetHeightDamageMultiplier() const
	{
		if (HeightDelta > 150.0f)  return 1.10f; // 高打低
		if (HeightDelta < -150.0f) return 0.90f; // 低打高
		return 1.0f;
	}

	/** 掩体伤害倍率 (AoE 不受掩体修正) */
	float GetCoverDamageMultiplier() const
	{
		return (Cover == ECoverQuality::Half) ? 0.75f : 1.0f;
	}

	/** 命中率修正 */
	float GetHitChanceDelta() const
	{
		return (Cover == ECoverQuality::Half) ? -0.15f : 0.0f;
	}

	/** 目标闪避倍率 (侧翼减半 / 背后归零) */
	float GetEvasionMultiplier() const
	{
		switch (Facing)
		{
		case EFacingRelation::Side: return 0.5f;
		case EFacingRelation::Rear: return 0.0f;
		default: return 1.0f;
		}
	}

	/** 暴击率加成 (背后 +15%) */
	float GetCritChanceBonus() const
	{
		return (Facing == EFacingRelation::Rear) ? 0.15f : 0.0f;
	}
};

/**
 * 战场地形查询 (顶层设计 §7) —— WFC 立体战场进入战斗规则的唯一通道。
 *
 * 射线走 ER_TRACE_BATTLELOS 专用通道: 战斗单位初始化时对该通道 Ignore,
 * 地形几何按默认响应 Block —— 单位天然不挡枪线, 也不需要每查询收集忽略列表。
 * 体素/自然岛/楼梯生成器零改动即获得 掩体/视线/高低差 规则。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleTerrainQuery : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBattleTerrainQuery* Get(const UWorld* World);

	/** 视线检测: 从攻击者胸口到目标 脚/胸/头 三点 */
	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	bool EvaluateLineOfSight(const FVector& From, const FVector& To) const;

	/** 完整地形上下文 (视线/掩体/高低差/朝向) */
	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	FBattleTerrainContext Evaluate(ABattleUnit* Attacker, ABattleUnit* Target) const;

	/** 便捷: 攻击者到目标是否有视线 */
	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	bool HasLineOfSight(ABattleUnit* Attacker, ABattleUnit* Target) const;

private:
	/** 单条视线射线 (专用通道, 单位已 Ignore) */
	bool TraceLine(const FVector& From, const FVector& To) const;
};