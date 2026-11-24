// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * 军团沙盘轻量 2D 高度场导航网格 (Grid A* + 坡度约束):
 *  - 开局从地形 (ECC_WorldStatic) 一次性采样构建, 耗时仅十余毫秒;
 *  - 依据坡度阈值自动标记悬崖/陡坡为不可通行, 让兵团绕山脊与谷道行军;
 *  - 提供高地优势系数查询与起点/终点邻近可通行点吸附。
 */
class EBBINGRAINBOW_API FLegionTerrainGrid
{
public:
	void Build(UWorld* World, const FVector& InCenter, float InHalfExtent,
		float InCellSize = 250.0f, float InMaxWalkableSlopeDeg = 38.0f);

	bool IsBuilt() const { return GridWidth > 0 && GridHeight > 0 && Cells.Num() == GridWidth * GridHeight; }

	/** 查询世界坐标 (X, Y) 处地表高度与坡度; 越界返回 false */
	bool SampleHeightAndSlope(const FVector& WorldPos, float& OutZ, float& OutSlopeDeg) const;

	/** 判断世界坐标处是否可通行 */
	bool IsWalkableWorld(const FVector& WorldPos) const;

	/** 将任意目标点吸附到最近的可通行地面中心 */
	FVector ClampToWalkable(const FVector& WorldPos) const;

	/** 寻找从 Start 到 Goal 的平滑路径点序列 (世界空间 XY, Z 取地表高) */
	bool FindPath(const FVector& StartWorld, const FVector& GoalWorld, TArray<FVector>& OutWaypoints) const;

	/** 计算高低差战术加成倍率 (攻击方相对受击方的高度优势, 范围约 0.80 ~ 1.35) */
	static float ComputeElevationMultiplier(float AttackerZ, float DefenderZ);

	// ---------------- 林地 (树木障碍与掩体) ----------------

	/** 清空树木影响, 恢复纯坡度可通行性 */
	void ResetTrees();

	/** 登记一棵树: 树冠半径内所有格中心的 TreeCount +1 */
	void AddTreeInfluence(const FVector& TreeWorld, float Radius);

	/** 按 TreeCount >= BlockThreshold 把密林格标为不可通行 (坡度可通行性不变) */
	void ApplyTreeObstacles(int32 BlockThreshold);

	/** 世界坐标所在格的树冠覆盖数 (越界返回 0) */
	int32 GetTreeCountWorld(const FVector& WorldPos) const;

	/** 远程攻击命中该位置时的林地掩体倍率 (1 = 无掩体) */
	float GetRangedCoverMultiplier(const FVector& WorldPos) const;

	/** 统计: 因树被封锁的格数 / 有树冠覆盖的格数 / 总格数 */
	void GetTreeStats(int32& OutBlocked, int32& OutCovered, int32& OutTotal) const;

	float GetCellSize() const { return CellSize; }

	/** 生成黑白等高线小地图像素数据 (RGBA8, 上北+Y / 右东+X) */
	void BuildMinimapPixels(int32 SizePx, TArray<FColor>& OutPixels) const;

	FVector GetGridCenter() const { return FVector(OriginXY.X + GridWidth * CellSize * 0.5f, OriginXY.Y + GridHeight * CellSize * 0.5f, 0.0f); }
	float GetWorldSpan() const { return GridWidth * CellSize; }

private:
	struct FCell
	{
		float HeightZ = 0.0f;
		float SlopeDeg = 0.0f;
		bool bValid = false;
		/** 当前可通行 = 坡度可通行 && 未被密林封锁 */
		bool bWalkable = false;
		/** 仅由坡度决定的可通行性 */
		bool bBaseWalkable = false;
		/** 覆盖本格中心的树冠数 */
		uint8 TreeCount = 0;
	};

	FORCEINLINE int32 IndexOf(int32 GX, int32 GY) const { return GY * GridWidth + GX; }
	bool WorldToGrid(const FVector& Pos, int32& OutGX, int32& OutGY) const;
	FVector GridToWorld(int32 GX, int32 GY) const;
	bool FindNearestWalkableCell(int32 StartGX, int32 StartGY, int32 MaxRadius, int32& OutGX, int32& OutGY) const;
	bool HasWalkableLineOfSight(int32 X0, int32 Y0, int32 X1, int32 Y1) const;

	FVector OriginXY = FVector::ZeroVector;
	float CellSize = 250.0f;
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	TArray<FCell> Cells;
};
