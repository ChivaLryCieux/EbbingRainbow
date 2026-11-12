// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "BattleRandomStream.generated.h"

/**
 * 战斗随机流 —— 全战斗唯一的随机源 (顶层设计 §8)。
 *
 * 纪律: 战斗内禁止 FMath::Rand / FMath::FRand / FRandomStream 临时实例。
 * 一切随机 (命中/暴击/伤害浮动/AI 抖动) 都从这里出, 同种子同操作序列 ⇒ 同结果,
 * 回放与 AI 回归测试免费获得。种子派生遵守 AGENTS.md: Seed + N*k, 不污染全局。
 */
USTRUCT(BlueprintType)
struct FBattleRandomStream
{
	GENERATED_BODY()

	/** 序列化状态: 跨存档续局保持确定性 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Rng")
	int32 InitialSeed = 0;

	void Init(int32 InSeed)
	{
		InitialSeed = InSeed;
		Stream.Initialize(InSeed);
	}

	/** 序列化: 导出当前状态 */
	int32 GetState() const { return Stream.GetCurrentSeed(); }

	/** 序列化: 恢复状态 */
	void SetState(int32 State)
	{
		if (InitialSeed == 0 && State != 0)
		{
			InitialSeed = State;
		}
		Stream.Initialize(State);
	}

	// ---------------- 唯一取随机入口 ----------------

	float FRand() { return Stream.FRand(); }

	float FRandRange(float Min, float Max) { return Stream.FRandRange(Min, Max); }

	int32 RandRange(int32 Min, int32 Max) { return Stream.RandRange(Min, Max); }

	/** 概率判定 p ∈ [0,1] */
	bool Chance(float Probability) { return Stream.FRand() < FMath::Clamp(Probability, 0.0f, 1.0f); }

	/** 对称浮动: Variation(0.1) → [0.9, 1.1] */
	float Variation(float Pct)
	{
		if (Pct <= 0.0f)
		{
			return 1.0f;
		}
		return 1.0f + Stream.FRandRange(-Pct, Pct);
	}

	/** 从数组随机取一元素 (空数组返回 nullptr 语义: 调用方自查) */
	template <typename T>
	T* Pick(TArray<T>& Items)
	{
		return Items.IsEmpty() ? nullptr : &Items[Stream.RandRange(0, Items.Num() - 1)];
	}

private:
	/** 运行时流本体 (不 UPROPERTY: 状态由 InitialSeed/State 承载) */
	FRandomStream Stream;
};

/** 派生战斗种子: Run 种子 + 遭遇种子, 遵循 Seed + N*k 纪律 */
inline int32 DeriveBattleSeed(int32 RunSeed, int32 EncounterSeed)
{
	// 大质数乘子 + 异或: 避免相邻遭遇种子产生相邻随机序列
	return (RunSeed * 7919) ^ (EncounterSeed * 104729) ^ 0x5F3759DF;
}