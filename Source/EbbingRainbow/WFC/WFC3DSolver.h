// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DTypes.h"

enum class EWFCStepResult : uint8
{
	InProgress,
	Completed,
	Contradiction
};

/**
 * 3D 波函数坍缩 (WFC) 高性能求解器
 * 负责网格叠加态管理、最低香农熵优先观测 (Observation)、弧相容约束传播 (AC-3 Propagation) 与冲突重试。
 */
class EBBINGRAINBOW_API FWFC3DSolver
{
public:
	FWFC3DSolver();
	~FWFC3DSolver();

	/**
	 * 初始化求解器网格与规则
	 * @param InDimensions 3D 网格长宽高 (X, Y, Z)
	 * @param InCompiledModules 预编译的模块列表
	 * @param InCompatibilityTable 预计算的邻接相容表 [ModA][Dir] -> BitArray(合法ModB)
	 */
	void Initialize(
		const FIntVector& InDimensions,
		const TArray<FCompiledWFC3DModule>& InCompiledModules,
		const TArray<TArray<TBitArray<>>>& InCompatibilityTable
	);

	/**
	 * 求解整个网格 (支持多轮重试)
	 * @param MaxRetries 最大冲突重试次数
	/**
	 * 求解整个网格 (支持多轮重试)
	 * @param MaxRetries 最大冲突重试次数
	 * @param Seed 随机数种子 (0 代表根据当前时间随机)
	 * @param InitialClamps 初始固定单元格索引与模块映射 (例如 Z=0 固定为地面)
	 * @return 是否求解成功
	 */
	bool Solve(int32 MaxRetries = 30, int32 Seed = 0, const TMap<int32, int32>& InitialClamps = TMap<int32, int32>());

	/** 强制固定指定单元格的模块索引并触发初始约束扩散 */
	bool ClampSlot(int32 SlotIndex, int32 ModuleIndex);

	/** 单步步进（用于在视口中逐帧观察算法坍缩演进过程）
	 * @return 单步执行结果: InProgress(继续), Completed(完成), Contradiction(冲突)
	 */
	EWFCStepResult Step();

	/** 重置所有网格为初始叠加态 */
	void ResetGrid();

	/** 获取计算完成的 3D 网格模块索引 */
	const TArray<FWFCSlot3D>& GetSlots() const { return Slots; }

	/** 坐标转一维索引 */
	FORCEINLINE int32 CoordToIndex(int32 X, int32 Y, int32 Z) const
	{
		return X + Y * Dimensions.X + Z * Dimensions.X * Dimensions.Y;
	}

	FORCEINLINE int32 CoordToIndex(const FIntVector& Coord) const
	{
		return CoordToIndex(Coord.X, Coord.Y, Coord.Z);
	}

	/** 一维索引转坐标 */
	FORCEINLINE FIntVector IndexToCoord(int32 Index) const
	{
		const int32 Area = Dimensions.X * Dimensions.Y;
		const int32 Z = Index / Area;
		const int32 Rem = Index % Area;
		const int32 Y = Rem / Dimensions.X;
		const int32 X = Rem % Dimensions.X;
		return FIntVector(X, Y, Z);
	}

	/** 检查坐标是否在网格边界内 */
	FORCEINLINE bool IsValidCoord(const FIntVector& Coord) const
	{
		return Coord.X >= 0 && Coord.X < Dimensions.X &&
		       Coord.Y >= 0 && Coord.Y < Dimensions.Y &&
		       Coord.Z >= 0 && Coord.Z < Dimensions.Z;
	}

	/** 获取网格尺寸 */
	const FIntVector& GetDimensions() const { return Dimensions; }

private:
	/** 选取当前未坍缩且熵最小的 Slot */
	int32 FindMinEntropySlot();

	/** 随机坍缩指定单元格 */
	bool CollapseSlot(int32 SlotIndex);

	/** 约束传播 (AC-3) */
	bool Propagate();

	/** 计算指定 Slot 的香农熵 */
	float CalculateEntropy(const TBitArray<>& Candidates) const;

private:
	FIntVector Dimensions = FIntVector::ZeroValue;
	int32 TotalCells = 0;
	int32 TotalModules = 0;

	TArray<FCompiledWFC3DModule> CompiledModules;
	TArray<TArray<TBitArray<>>> CompatibilityTable;

	TArray<FWFCSlot3D> Slots;
	TArray<int32> PropagationQueue;

	FRandomStream RandomStream;
	float InitialEntropy = 0.0f;
};
