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
	UncollapsedSlots.SetNumUninitialized(TotalCells);
	SlotToUncollapsedIndex.SetNumUninitialized(TotalCells);

	for (int32 i = 0; i < TotalCells; ++i)
	{
		Slots[i].Initialize(TotalModules, InitialEntropy);
		UncollapsedSlots[i] = i;
		SlotToUncollapsedIndex[i] = i;
	}

	PropagationQueue.Empty();
	InPropagationQueue.Init(false, TotalCells);
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
	if (UncollapsedSlots.Num() == 0)
	{
		return INDEX_NONE;
	}

	float MinEntropy = MAX_FLT;
	int32 BestSlotIndex = INDEX_NONE;

	for (int32 i = 0; i < UncollapsedSlots.Num(); ++i)
	{
		const int32 SlotIdx = UncollapsedSlots[i];
		FWFCSlot3D& Slot = Slots[SlotIdx];

		if (Slot.bCollapsed)
		{
			RemoveUncollapsedSlot(SlotIdx);
			--i;
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
			RemoveUncollapsedSlot(SlotIdx);
			--i;
			continue;
		}

		// 带有微小扰动的熵，打破相同熵的平局
		const float Jitter = RandomStream.FRandRange(-0.0001f, 0.0001f);
		const float PerturbedEntropy = Slot.CachedEntropy + Jitter;

		if (PerturbedEntropy < MinEntropy)
		{
			MinEntropy = PerturbedEntropy;
			BestSlotIndex = SlotIdx;
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

	RemoveUncollapsedSlot(SlotIndex);

	// 入队触发传播
	if (!InPropagationQueue[SlotIndex])
	{
		InPropagationQueue[SlotIndex] = true;
		PropagationQueue.Add(SlotIndex);
	}
	return true;
}

bool FWFC3DSolver::Propagate()
{
	TBitArray<> AllowedForNeighbor(false, TotalModules);

	while (PropagationQueue.Num() > 0)
	{
		const int32 CurrentIndex = PropagationQueue.Pop(EAllowShrinking::No);
		InPropagationQueue[CurrentIndex] = false;

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
				// 如果当前格也已坍缩，必须显式校验这对固定格；
				// 否则两个 clamp（或先坍缩的格子）之间的冲突会被静默跳过。
				if (CurrentSlot.bCollapsed)
				{
					const int32 CurrentModule = CurrentSlot.FinalModuleIndex;
					const int32 NeighborModule = NeighborSlot.FinalModuleIndex;
					if (!CompiledModules.IsValidIndex(CurrentModule)
						|| !CompiledModules.IsValidIndex(NeighborModule)
						|| !CompatibilityTable[CurrentModule][DirIdx][NeighborModule])
					{
						UE_LOG(LogTemp, Warning, TEXT("[WFC] Contradiction between collapsed cells (%d,%d,%d) dir %d."),
							CurrentCoord.X, CurrentCoord.Y, CurrentCoord.Z, DirIdx);
						return false;
					}
				}
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
					RemoveUncollapsedSlot(NeighborIndex);
				}

				// 将受到波及的邻居加入队列继续向外扩散 (O(1) 查重入队)
				if (!InPropagationQueue[NeighborIndex])
				{
					InPropagationQueue[NeighborIndex] = true;
					PropagationQueue.Add(NeighborIndex);
				}
			}
		}
	}

	return true;
}

EWFCStepResult FWFC3DSolver::Step()
{
	// 检查是否已经全部坍缩完成 (O(1))
	if (UncollapsedSlots.Num() == 0)
	{
		return EWFCStepResult::Completed;
	}

	const int32 MinSlot = FindMinEntropySlot();
	if (MinSlot == INDEX_NONE)
	{
		return (UncollapsedSlots.Num() > 0) ? EWFCStepResult::Contradiction : EWFCStepResult::Completed;
	}

	if (!CollapseSlot(MinSlot))
	{
		return EWFCStepResult::Contradiction;
	}

	if (!Propagate())
	{
		return EWFCStepResult::Contradiction;
	}

	return (UncollapsedSlots.Num() == 0) ? EWFCStepResult::Completed : EWFCStepResult::InProgress;
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

	RemoveUncollapsedSlot(SlotIndex);

	if (!InPropagationQueue[SlotIndex])
	{
		InPropagationQueue[SlotIndex] = true;
		PropagationQueue.Add(SlotIndex);
	}
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

		// 应用初始固定插槽约束 (例如底层全量固定为地面，或中庭开敞挑空)
		for (const auto& Pair : InitialClamps)
		{
			const int32 SlotIndex = Pair.Key;
			const int32 ModuleIndex = Pair.Value;
			if (!Slots.IsValidIndex(SlotIndex) || !CompiledModules.IsValidIndex(ModuleIndex))
			{
				continue;
			}

			FWFCSlot3D& Slot = Slots[SlotIndex];
			Slot.Candidates.Init(false, TotalModules);
			Slot.Candidates[ModuleIndex] = true;
			Slot.RemainingCandidatesCount = 1;
			Slot.bCollapsed = true;
			Slot.FinalModuleIndex = ModuleIndex;
			Slot.CachedEntropy = 0.0f;

			RemoveUncollapsedSlot(SlotIndex);

			if (!InPropagationQueue[SlotIndex])
			{
				InPropagationQueue[SlotIndex] = true;
				PropagationQueue.Add(SlotIndex);
			}
		}

		if (InitialClamps.Num() > 0)
		{
			if (!Propagate())
			{
				// 固定约束在一次 Solve 内是常量，换 seed 不可能解决冲突，直接失败。
				UE_LOG(LogTemp, Warning, TEXT("[WFC] Initial clamp propagation contradiction (seed %d, clamps=%d)."),
					BaseSeed + Attempt * 1013, InitialClamps.Num());
				return false;
			}
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
