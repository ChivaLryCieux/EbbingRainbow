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
		bool bWalkable = false;
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
