// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DSolver.h"

FWFC3DSolver::FWFC3DSolver()
{
}

FWFC3DSolver::~FWFC3DSolver()
{
}

void FWFC3DSolver::Initialize(
	const FIntVector& InDimensions,
	const TArray<FCompiledWFC3DModule>& InCompiledModules,
	const TArray<TArray<TBitArray<>>>& InCompatibilityTable)
{
	Dimensions = InDimensions;
	TotalCells = Dimensions.X * Dimensions.Y * Dimensions.Z;
	CompiledModules = InCompiledModules;
	CompatibilityTable = InCompatibilityTable;
	TotalModules = CompiledModules.Num();

	// 计算所有模块都在叠加态时的初始全集香农熵
	TBitArray<> AllCandidates(true, TotalModules);
	InitialEntropy = CalculateEntropy(AllCandidates);

	ResetGrid();
}

void FWFC3DSolver::ResetGrid()
{
	Slots.SetNum(TotalCells);
	for (int32 i = 0; i < TotalCells; ++i)
	{
		Slots[i].Initialize(TotalModules, InitialEntropy);
	}
	PropagationQueue.Empty();
}

float FWFC3DSolver::CalculateEntropy(const TBitArray<>& Candidates) const
{
	float WeightSum = 0.0f;
	float WeightLogWeightSum = 0.0f;
	int32 ActiveCount = 0;

	for (int32 ModIdx = 0; ModIdx < TotalModules; ++ModIdx)
	{
		if (Candidates[ModIdx])
		{
			const float W = CompiledModules[ModIdx].Weight;
			WeightSum += W;
			WeightLogWeightSum += W * FMath::Loge(W);
			ActiveCount++;
		}
	}

	if (ActiveCount <= 1 || WeightSum <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Loge(WeightSum) - (WeightLogWeightSum / WeightSum);
}

int32 FWFC3DSolver::FindMinEntropySlot()
{
	float MinEntropy = MAX_FLT;
	int32 BestSlotIndex = INDEX_NONE;

	for (int32 i = 0; i < TotalCells; ++i)
	{
		FWFCSlot3D& Slot = Slots[i];

		if (Slot.bCollapsed)
		{
			continue;
		}

		if (Slot.RemainingCandidatesCount == 0)
		{
			// 发生矛盾：该格子候选集为空
			return INDEX_NONE;
		}

		if (Slot.RemainingCandidatesCount == 1)
		{
			// 虽未标记但已自动唯一确定，直接标记
			Slot.bCollapsed = true;
			for (int32 ModIdx = 0; ModIdx < TotalModules; ++ModIdx)
			{
				if (Slot.Candidates[ModIdx])
				{
					Slot.FinalModuleIndex = ModIdx;
					break;
				}
			}
			continue;
		}

		// 带有微小扰动的熵，打破相同熵的平局
		const float Jitter = RandomStream.FRandRange(-0.0001f, 0.0001f);
		const float PerturbedEntropy = Slot.CachedEntropy + Jitter;

		if (PerturbedEntropy < MinEntropy)
		{
			MinEntropy = PerturbedEntropy;
			BestSlotIndex = i;
		}
	}

	return BestSlotIndex;
}

bool FWFC3DSolver::CollapseSlot(int32 SlotIndex)
{
	FWFCSlot3D& Slot = Slots[SlotIndex];

	// 计算当前所有合法候选的权重和
	float TotalCandidateWeight = 0.0f;
	for (int32 ModIdx = 0; ModIdx < TotalModules; ++ModIdx)
	{
		if (Slot.Candidates[ModIdx])
		{
			TotalCandidateWeight += CompiledModules[ModIdx].Weight;
		}
	}

	if (TotalCandidateWeight <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 轮盘赌抽样
	float Pick = RandomStream.FRandRange(0.0f, TotalCandidateWeight);
	int32 ChosenIndex = INDEX_NONE;

	for (int32 ModIdx = 0; ModIdx < TotalModules; ++ModIdx)
	{
		if (Slot.Candidates[ModIdx])
		{
			const float W = CompiledModules[ModIdx].Weight;
			if (Pick <= W)
			{
				ChosenIndex = ModIdx;
				break;
			}
			Pick -= W;
		}
	}

	if (ChosenIndex == INDEX_NONE)
	{
		// 备底保护
		for (int32 ModIdx = 0; ModIdx < TotalModules; ++ModIdx)
		{
			if (Slot.Candidates[ModIdx])
			{
				ChosenIndex = ModIdx;
				break;
			}
		}
	}

	if (ChosenIndex == INDEX_NONE)
	{
		return false;
	}

	// 坍缩固定
	Slot.Candidates.Init(false, TotalModules);
	Slot.Candidates[ChosenIndex] = true;
	Slot.RemainingCandidatesCount = 1;
	Slot.bCollapsed = true;
	Slot.FinalModuleIndex = ChosenIndex;
	Slot.CachedEntropy = 0.0f;

	// 入队触发传播
	PropagationQueue.Add(SlotIndex);
	return true;
}

bool FWFC3DSolver::Propagate()
{
	TBitArray<> AllowedForNeighbor(false, TotalModules);

	while (PropagationQueue.Num() > 0)
	{
		const int32 CurrentIndex = PropagationQueue.Pop(EAllowShrinking::No);
		const FIntVector CurrentCoord = IndexToCoord(CurrentIndex);
		const FWFCSlot3D& CurrentSlot = Slots[CurrentIndex];

		for (int32 DirIdx = 0; DirIdx < 6; ++DirIdx)
		{
			const EWFC3DDirection Dir = WFC3DUtils::IndexToDirection(DirIdx);
			const FIntVector NeighborCoord = CurrentCoord + WFC3DUtils::GetDirectionOffset(Dir);

			if (!IsValidCoord(NeighborCoord))
			{
				continue;
			}

			const int32 NeighborIndex = CoordToIndex(NeighborCoord);
			FWFCSlot3D& NeighborSlot = Slots[NeighborIndex];

			if (NeighborSlot.bCollapsed)
			{
				continue;
			}

			// 计算当前格子的所有有效候选允许邻居具备的模块合集 (Union)
			AllowedForNeighbor.Init(false, TotalModules);
			for (int32 ModAIdx = 0; ModAIdx < TotalModules; ++ModAIdx)
			{
				if (CurrentSlot.Candidates[ModAIdx])
				{
					const TBitArray<>& CompatibleBits = CompatibilityTable[ModAIdx][DirIdx];
					for (int32 BitIdx = 0; BitIdx < TotalModules; ++BitIdx)
					{
						if (CompatibleBits[BitIdx])
						{
							AllowedForNeighbor[BitIdx] = true;
						}
					}
				}
			}

			// 对邻居的候选集进行过滤剪枝 (Intersection)
			bool bCandidatesReduced = false;
			int32 RemainingCount = 0;

			for (int32 BitIdx = 0; BitIdx < TotalModules; ++BitIdx)
			{
				if (NeighborSlot.Candidates[BitIdx])
				{
					if (!AllowedForNeighbor[BitIdx])
					{
						NeighborSlot.Candidates[BitIdx] = false;
						bCandidatesReduced = true;
					}
					else
					{
						RemainingCount++;
					}
				}
			}

			if (RemainingCount == 0)
			{
				// 冲突产生！某个邻居无可选择模块
				return false;
			}

			if (bCandidatesReduced)
			{
				NeighborSlot.RemainingCandidatesCount = RemainingCount;
				NeighborSlot.CachedEntropy = CalculateEntropy(NeighborSlot.Candidates);

				// 如果邻居被削减到只剩1个，可以直接标记坍缩
				if (RemainingCount == 1)
				{
					NeighborSlot.bCollapsed = true;
					for (int32 BitIdx = 0; BitIdx < TotalModules; ++BitIdx)
					{
						if (NeighborSlot.Candidates[BitIdx])
						{
							NeighborSlot.FinalModuleIndex = BitIdx;
							break;
						}
					}
				}

				// 将受到波及的邻居加入队列继续向外扩散
				PropagationQueue.AddUnique(NeighborIndex);
			}
		}
	}

	return true;
}

EWFCStepResult FWFC3DSolver::Step()
{
	// 检查是否已经全部坍缩完成
	bool bAllCollapsed = true;
	for (int32 i = 0; i < TotalCells; ++i)
	{
		if (!Slots[i].bCollapsed)
		{
			bAllCollapsed = false;
			break;
		}
	}

	if (bAllCollapsed)
	{
		return EWFCStepResult::Completed;
	}

	const int32 MinSlot = FindMinEntropySlot();
	if (MinSlot == INDEX_NONE)
	{
		// 再次验证是否全部已完成
		bool bStillUncollapsed = false;
		for (int32 i = 0; i < TotalCells; ++i)
		{
			if (!Slots[i].bCollapsed)
			{
				bStillUncollapsed = true;
				break;
			}
		}

		return bStillUncollapsed ? EWFCStepResult::Contradiction : EWFCStepResult::Completed;
	}

	if (!CollapseSlot(MinSlot))
	{
		return EWFCStepResult::Contradiction;
	}

	if (!Propagate())
	{
		return EWFCStepResult::Contradiction;
	}

	return EWFCStepResult::InProgress;
}

bool FWFC3DSolver::ClampSlot(int32 SlotIndex, int32 ModuleIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !CompiledModules.IsValidIndex(ModuleIndex))
	{
		return false;
	}

	FWFCSlot3D& Slot = Slots[SlotIndex];
	Slot.Candidates.Init(false, TotalModules);
	Slot.Candidates[ModuleIndex] = true;
	Slot.RemainingCandidatesCount = 1;
	Slot.bCollapsed = true;
	Slot.FinalModuleIndex = ModuleIndex;
	Slot.CachedEntropy = 0.0f;

	PropagationQueue.Add(SlotIndex);
	return Propagate();
}

bool FWFC3DSolver::Solve(int32 MaxRetries, int32 Seed, const TMap<int32, int32>& InitialClamps)
{
	if (TotalCells == 0 || TotalModules == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC] Solve failed: Grid or Modules not initialized!"));
		return false;
	}

	const int32 BaseSeed = (Seed != 0) ? Seed : FMath::Rand();

	for (int32 Attempt = 0; Attempt < MaxRetries; ++Attempt)
	{
		RandomStream.Initialize(BaseSeed + Attempt * 1013);
		ResetGrid();

		// 应用初始固定插槽约束 (例如底层全量固定为地面)
		bool bClampsValid = true;
		for (const auto& Pair : InitialClamps)
		{
			if (!ClampSlot(Pair.Key, Pair.Value))
			{
				bClampsValid = false;
				break;
			}
		}

		if (!bClampsValid)
		{
			continue;
		}

		bool bSuccess = true;
		while (true)
		{
			const EWFCStepResult StepResult = Step();
			if (StepResult == EWFCStepResult::Completed)
			{
				UE_LOG(LogTemp, Log, TEXT("[WFC] Solve succeeded on attempt %d with seed %d!"), Attempt + 1, BaseSeed + Attempt * 1013);
				return true;
			}
			else if (StepResult == EWFCStepResult::Contradiction)
			{
				bSuccess = false;
				break;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[WFC] Solve reached max retries (%d) without finding a valid solution."), MaxRetries);
	return false;
}
