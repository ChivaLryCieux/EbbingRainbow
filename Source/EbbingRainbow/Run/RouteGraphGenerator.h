// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Run/RunTypes.h"

/**
 * 分层 DAG 路线图生成器（杀戮尖塔式）。
 *
 * 结构保证：
 *  - 边只从 Layer i 指向 Layer i+1 → 结构上无环、不可能走回头路；
 *  - 上一层按 Slot 排序、下一层按 Slot 排序后单调连边 → 连线不交叉；
 *  - 每个非终点节点至少 1 个后继、每个非起点节点至少 1 个前驱 → 无死路；
 *  - 生成后做可达性/层数/边合法性校验，并按需翻转安全节点，
 *    直到"任意起点→终点路径上的战斗数 >= MinBattlesPerPath"。
 *
 * 确定性：所有随机数都来自 FRandomStream(Seed ^ 尝试序号)，
 * 同一 Seed + 同一 Settings 必然得到逐字节相同的图（含 EncounterSeed）。
 */
class EBBINGRAINBOW_API FRouteGraphGenerator
{
public:
	/** 生成一张图；失败（重试 MaxAttempts 次仍不合法）返回 false。 */
	static bool Generate(int32 Seed, const FRouteGenSettings& Settings, FRouteGraph& OutGraph, FString* OutReport = nullptr);

	/** 结构 + 约束校验。失败时 OutError 给出第一条不满足的规则。 */
	static bool Validate(const FRouteGraph& Graph, const FRouteGenSettings& Settings, FString& OutError);

	/** 所有起点→终点路径中，战斗场次的最小值（DP，O(V+E)）。 */
	static int32 ComputeMinBattlesPerPath(const FRouteGraph& Graph);

	/** 节点遭遇种子：由 RunSeed 与节点 Id 派生，保证同一局同一节点内容一致。 */
	static int32 MakeEncounterSeed(int32 RunSeed, int32 NodeId);

	/** 一行摘要，写日志/显示在 UI 上都用这个格式。 */
	static FString MakeReport(const FRouteGraph& Graph);

private:
	static bool BuildGraph(FRandomStream& Rng, int32 Seed, const FRouteGenSettings& Settings, FRouteGraph& OutGraph);
	static void ConnectLayers(FRouteGraph& Graph, const TArray<int32>& Upper, const TArray<int32>& Lower, FRandomStream& Rng);
	static void EnforceMinBattles(FRouteGraph& Graph, const FRouteGenSettings& Settings);
	static void ComputeMinBattlesTable(const FRouteGraph& Graph, TArray<int32>& OutTable);
};
