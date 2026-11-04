// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RouteGraphGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogRouteGen, Log, All);

namespace
{
	/** 战斗节点细分为体素/自然两种场景 (种子化分流) */
	ERouteNodeType PickBattleType(FRandomStream& Rng, const FRouteGenSettings& Settings)
	{
		return (Rng.FRand() < Settings.NatureBattleChance)
			? ERouteNodeType::BattleNature : ERouteNodeType::Battle;
	}
}

bool FRouteGraphGenerator::Generate(int32 Seed, const FRouteGenSettings& Settings, FRouteGraph& OutGraph, FString* OutReport)
{
	if (Settings.NumLayers < 3 || Settings.NumSlots < 2
		|| Settings.MinNodesPerLayer < 1
		|| Settings.MaxNodesPerLayer < Settings.MinNodesPerLayer
		|| Settings.MaxNodesPerLayer > Settings.NumSlots
		|| Settings.MinBattlesPerPath < 0)
	{
		UE_LOG(LogRouteGen, Error, TEXT("[Route] 生成参数非法: Layers=%d Slots=%d Nodes=[%d,%d] MinBattles=%d"),
			Settings.NumLayers, Settings.NumSlots, Settings.MinNodesPerLayer, Settings.MaxNodesPerLayer, Settings.MinBattlesPerPath);
		return false;
	}

	const int32 Attempts = FMath::Max(1, Settings.MaxAttempts);
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		// 每次重试使用派生种子：同一输入种子仍然完全确定。
		FRandomStream Rng(Seed ^ static_cast<int32>(Attempt * 0x9E3779B9));

		FRouteGraph Graph;
		if (!BuildGraph(Rng, Seed, Settings, Graph))
		{
			continue;
		}

		FString Error;
		if (!Validate(Graph, Settings, Error))
		{
			UE_LOG(LogRouteGen, Verbose, TEXT("[Route] seed=%d attempt=%d 校验失败: %s"), Seed, Attempt, *Error);
			continue;
		}

		OutGraph = MoveTemp(Graph);
		if (OutReport)
		{
			*OutReport = MakeReport(OutGraph);
		}
		UE_LOG(LogRouteGen, Log, TEXT("[Route] %s"), *MakeReport(OutGraph));
		return true;
	}

	UE_LOG(LogRouteGen, Error, TEXT("[Route] seed=%d 连续 %d 次生成失败，请放宽约束或更换种子。"), Seed, Attempts);
	return false;
}

bool FRouteGraphGenerator::BuildGraph(FRandomStream& Rng, int32 Seed, const FRouteGenSettings& Settings, FRouteGraph& OutGraph)
{
	OutGraph = FRouteGraph();
	OutGraph.Seed = Seed;
	OutGraph.NumLayers = Settings.NumLayers;
	OutGraph.NumSlots = Settings.NumSlots;

	const int32 NumLayers = Settings.NumLayers;
	TArray<TArray<int32>> LayerIds;
	LayerIds.SetNum(NumLayers);

	auto AddNode = [&OutGraph](int32 Layer, int32 Slot, ERouteNodeType Type) -> int32
	{
		FRouteNode Node;
		Node.Id = OutGraph.Nodes.Num();
		Node.Layer = Layer;
		Node.Slot = Slot;
		Node.Type = Type;
		OutGraph.Nodes.Add(Node);
		return Node.Id;
	};

	// 起点层：唯一节点
	LayerIds[0].Add(AddNode(0, Rng.RandRange(0, Settings.NumSlots - 1), ERouteNodeType::Safe));

	// 中间层：每层 Min..Max 个节点，列位随机且互不重复
	for (int32 Layer = 1; Layer < NumLayers - 1; ++Layer)
	{
		const int32 Count = FMath::Clamp(Rng.RandRange(Settings.MinNodesPerLayer, Settings.MaxNodesPerLayer),
			Settings.MinNodesPerLayer, Settings.MaxNodesPerLayer);

		TArray<int32> Pool;
		Pool.Reserve(Settings.NumSlots);
		for (int32 Slot = 0; Slot < Settings.NumSlots; ++Slot)
		{
			Pool.Add(Slot);
		}
		for (int32 i = Pool.Num() - 1; i > 0; --i)
		{
			const int32 j = Rng.RandRange(0, i);
			Pool.Swap(i, j);
		}

		TArray<int32> Slots;
		Slots.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Slots.Add(Pool[i]);
		}
		Slots.Sort();

		for (const int32 Slot : Slots)
		{
			LayerIds[Layer].Add(AddNode(Layer, Slot, ERouteNodeType::Safe));
		}
	}

	// 终点层：唯一 Boss 节点，固定为战斗
	LayerIds[NumLayers - 1].Add(AddNode(NumLayers - 1, Settings.NumSlots / 2, PickBattleType(Rng, Settings)));

	// 相邻层连边
	for (int32 Layer = 0; Layer < NumLayers - 1; ++Layer)
	{
		ConnectLayers(OutGraph, LayerIds[Layer], LayerIds[Layer + 1], Rng);
	}

	// 中间层类型
	for (int32 Layer = 1; Layer < NumLayers - 1; ++Layer)
	{
		for (const int32 Id : LayerIds[Layer])
		{
			OutGraph.Nodes[Id].Type = (Rng.FRand() < Settings.BattleChance) ? PickBattleType(Rng, Settings) : ERouteNodeType::Safe;
		}
	}

	OutGraph.StartNodeId = LayerIds[0][0];
	OutGraph.EndNodeId = LayerIds[NumLayers - 1][0];
	OutGraph.Nodes[OutGraph.EndNodeId].Type = PickBattleType(Rng, Settings);

	// 约束修正：保证任意路径上的战斗数达标
	EnforceMinBattles(OutGraph, Settings);

	// 节点遭遇种子
	for (FRouteNode& Node : OutGraph.Nodes)
	{
		Node.EncounterSeed = MakeEncounterSeed(Seed, Node.Id);
	}

	return true;
}

void FRouteGraphGenerator::ConnectLayers(FRouteGraph& Graph, const TArray<int32>& Upper, const TArray<int32>& Lower, FRandomStream& Rng)
{
	const int32 N = Upper.Num();
	const int32 M = Lower.Num();
	if (N == 0 || M == 0)
	{
		return;
	}

	auto AddEdge = [&Graph](int32 From, int32 To)
	{
		FRouteNode& FromNode = Graph.Nodes[From];
		FRouteNode& ToNode = Graph.Nodes[To];
		if (!FromNode.Next.Contains(To))
		{
			FromNode.Next.Add(To);
		}
		if (!ToNode.Prev.Contains(From))
		{
			ToNode.Prev.Add(From);
		}
	};

	// 单点层：起点/终点层直接全部连过去
	if (M == 1)
	{
		for (const int32 Id : Upper)
		{
			AddEdge(Id, Lower[0]);
		}
		return;
	}
	if (N == 1)
	{
		for (const int32 Id : Lower)
		{
			AddEdge(Upper[0], Id);
		}
		return;
	}

	// 阶梯式单调连边：第 i 个上层只连下标落在 [floor(i*M/N), floor((i+1)*M/N)] 的下层。
	// 区间随 i 单调不减 → 结构上不可能出现交叉线；同时保证每个上层/下层至少有一条边。
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Lo = (i * M) / N;
		const int32 Hi = FMath::Min(M - 1, ((i + 1) * M) / N);
		for (int32 j = Lo; j <= Hi; ++j)
		{
			AddEdge(Upper[i], Lower[j]);
		}
	}

	// 随机删掉一部分边制造疏密变化；只删两端度数都 >1 的，保证不会产生死路，也不会破坏单调性。
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 i = 0; i < N; ++i)
		{
			if (Graph.Nodes[Upper[i]].Next.Num() <= 1)
			{
				continue;
			}

			for (int32 j = 0; j < M; ++j)
			{
				if (Graph.Nodes[Upper[i]].Next.Num() <= 1)
				{
					break;
				}
				if (Graph.Nodes[Lower[j]].Prev.Num() <= 1)
				{
					continue;
				}
				if (!Graph.Nodes[Upper[i]].Next.Contains(Lower[j]))
				{
					continue;
				}

				if (Rng.FRand() < 0.30f)
				{
					Graph.Nodes[Upper[i]].Next.Remove(Lower[j]);
					Graph.Nodes[Lower[j]].Prev.Remove(Upper[i]);
				}
			}
		}
	}
}

void FRouteGraphGenerator::ComputeMinBattlesTable(const FRouteGraph& Graph, TArray<int32>& OutTable)
{
	OutTable.Init(0, Graph.Nodes.Num());

	// 边只指向更高 Id（逐层追加），倒序遍历即可自底向上 DP。
	for (int32 Id = Graph.Nodes.Num() - 1; Id >= 0; --Id)
	{
		const FRouteNode& Node = Graph.Nodes[Id];
		int32 BestSuffix = 0;
		if (Node.Next.Num() > 0)
		{
			BestSuffix = MAX_int32;
			for (const int32 NextId : Node.Next)
			{
				BestSuffix = FMath::Min(BestSuffix, OutTable[NextId]);
			}
		}
		OutTable[Id] = (IsRouteNodeCombat(Node.Type) ? 1 : 0) + BestSuffix;
	}
}

int32 FRouteGraphGenerator::ComputeMinBattlesPerPath(const FRouteGraph& Graph)
{
	if (!Graph.IsValid())
	{
		return 0;
	}

	TArray<int32> Table;
	ComputeMinBattlesTable(Graph, Table);
	return Table[Graph.StartNodeId];
}

void FRouteGraphGenerator::EnforceMinBattles(FRouteGraph& Graph, const FRouteGenSettings& Settings)
{
	// 种子化分流: EnforceMinBattles 没有传入 Rng, 从图种子派生一条确定性流
	FRandomStream TypeRng(Graph.Seed * 31 + 7);

	// 最多翻转"中间层节点总数"次，足够把最差路径补到目标值。
	const int32 MaxFlips = FMath::Max(1, Graph.Nodes.Num());
	for (int32 Flip = 0; Flip < MaxFlips; ++Flip)
	{
		TArray<int32> Table;
		ComputeMinBattlesTable(Graph, Table);

		if (Table[Graph.StartNodeId] >= Settings.MinBattlesPerPath)
		{
			return;
		}

		// 沿一条"战斗最少"的路径，把第一个安全节点升级为战斗
		int32 Id = Graph.StartNodeId;
		bool bFlipped = false;
		while (Graph.Nodes.IsValidIndex(Id))
		{
			FRouteNode& Node = Graph.Nodes[Id];
			if (Id != Graph.StartNodeId && !IsRouteNodeCombat(Node.Type))
			{
				Node.Type = PickBattleType(TypeRng, Settings);
				bFlipped = true;
				break;
			}

			int32 BestNext = INDEX_NONE;
			int32 BestValue = MAX_int32;
			for (const int32 NextId : Node.Next)
			{
				if (Table[NextId] < BestValue)
				{
					BestValue = Table[NextId];
					BestNext = NextId;
				}
			}
			Id = BestNext;
		}

		if (!bFlipped)
		{
			return;
		}
	}
}

bool FRouteGraphGenerator::Validate(const FRouteGraph& Graph, const FRouteGenSettings& Settings, FString& OutError)
{
	auto Fail = [&OutError](const FString& Message) -> bool
	{
		OutError = Message;
		return false;
	};

	if (!Graph.IsValid())
	{
		return Fail(TEXT("图无效：缺少起点或终点"));
	}
	if (Graph.NumLayers != Settings.NumLayers)
	{
		return Fail(FString::Printf(TEXT("层数不符：%d != %d"), Graph.NumLayers, Settings.NumLayers));
	}

	TArray<TArray<int32>> LayerSlots;
	LayerSlots.SetNum(Graph.NumLayers);

	for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
	{
		const FRouteNode& Node = Graph.Nodes[Index];
		if (Node.Id != Index)
		{
			return Fail(FString::Printf(TEXT("节点 Id 与下标不一致：%d != %d"), Node.Id, Index));
		}
		if (!LayerSlots.IsValidIndex(Node.Layer))
		{
			return Fail(FString::Printf(TEXT("节点 %d 的层号越界：%d"), Node.Id, Node.Layer));
		}
		if (Node.Slot < 0 || Node.Slot >= Graph.NumSlots)
		{
			return Fail(FString::Printf(TEXT("节点 %d 的列位越界：%d"), Node.Id, Node.Slot));
		}
		if (LayerSlots[Node.Layer].Contains(Node.Slot))
		{
			return Fail(FString::Printf(TEXT("第 %d 层列位重复：%d"), Node.Layer, Node.Slot));
		}
		LayerSlots[Node.Layer].Add(Node.Slot);
	}

	for (int32 Layer = 0; Layer < Graph.NumLayers; ++Layer)
	{
		const int32 Count = LayerSlots[Layer].Num();
		const bool bEdgeLayer = (Layer == 0 || Layer == Graph.NumLayers - 1);
		if (bEdgeLayer && Count != 1)
		{
			return Fail(FString::Printf(TEXT("第 %d 层应为 1 个节点，实际 %d"), Layer, Count));
		}
		if (!bEdgeLayer && (Count < Settings.MinNodesPerLayer || Count > Settings.MaxNodesPerLayer))
		{
			return Fail(FString::Printf(TEXT("第 %d 层节点数 %d 超出 [%d,%d]"),
				Layer, Count, Settings.MinNodesPerLayer, Settings.MaxNodesPerLayer));
		}
	}

	if (Graph.Nodes[Graph.StartNodeId].Layer != 0)
	{
		return Fail(TEXT("起点不在第 0 层"));
	}
	if (Graph.Nodes[Graph.EndNodeId].Layer != Graph.NumLayers - 1)
	{
		return Fail(TEXT("终点不在最后一层"));
	}
	if (!IsRouteNodeCombat(Graph.Nodes[Graph.EndNodeId].Type))
	{
		return Fail(TEXT("终点必须是战斗节点"));
	}

	// 边合法性 + 双向一致性
	for (const FRouteNode& Node : Graph.Nodes)
	{
		if (Node.Next.Contains(Node.Id))
		{
			return Fail(FString::Printf(TEXT("节点 %d 指向自身"), Node.Id));
		}
		for (const int32 NextId : Node.Next)
		{
			if (!Graph.Nodes.IsValidIndex(NextId))
			{
				return Fail(FString::Printf(TEXT("节点 %d 的后继 %d 不存在"), Node.Id, NextId));
			}
			if (Graph.Nodes[NextId].Layer != Node.Layer + 1)
			{
				return Fail(FString::Printf(TEXT("边 %d->%d 跨层非法"), Node.Id, NextId));
			}
			if (!Graph.Nodes[NextId].Prev.Contains(Node.Id))
			{
				return Fail(FString::Printf(TEXT("边 %d->%d 的 Prev 未同步"), Node.Id, NextId));
			}
		}
		if (Node.Id != Graph.EndNodeId && Node.Next.Num() == 0)
		{
			return Fail(FString::Printf(TEXT("节点 %d 是死路（没有后继）"), Node.Id));
		}
		if (Node.Id != Graph.StartNodeId && Node.Prev.Num() == 0)
		{
			return Fail(FString::Printf(TEXT("节点 %d 不可达（没有前驱）"), Node.Id));
		}
		for (const int32 PrevId : Node.Prev)
		{
			if (!Graph.Nodes.IsValidIndex(PrevId) || !Graph.Nodes[PrevId].Next.Contains(Node.Id))
			{
				return Fail(FString::Printf(TEXT("节点 %d 的 Prev 边 %d 未同步"), Node.Id, PrevId));
			}
		}
	}

	// 正向可达
	{
		TArray<bool> Reached;
		Reached.Init(false, Graph.Nodes.Num());
		TArray<int32> Stack;
		Stack.Add(Graph.StartNodeId);
		Reached[Graph.StartNodeId] = true;
		while (Stack.Num() > 0)
		{
			const int32 Id = Stack.Pop();
			for (const int32 NextId : Graph.Nodes[Id].Next)
			{
				if (!Reached[NextId])
				{
					Reached[NextId] = true;
					Stack.Add(NextId);
				}
			}
		}
		for (int32 Id = 0; Id < Reached.Num(); ++Id)
		{
			if (!Reached[Id])
			{
				return Fail(FString::Printf(TEXT("节点 %d 从起点不可达"), Id));
			}
		}
	}

	// 反向可达（都能走到终点）
	{
		TArray<bool> Reached;
		Reached.Init(false, Graph.Nodes.Num());
		TArray<int32> Stack;
		Stack.Add(Graph.EndNodeId);
		Reached[Graph.EndNodeId] = true;
		while (Stack.Num() > 0)
		{
			const int32 Id = Stack.Pop();
			for (const int32 PrevId : Graph.Nodes[Id].Prev)
			{
				if (!Reached[PrevId])
				{
					Reached[PrevId] = true;
					Stack.Add(PrevId);
				}
			}
		}
		for (int32 Id = 0; Id < Reached.Num(); ++Id)
		{
			if (!Reached[Id])
			{
				return Fail(FString::Printf(TEXT("节点 %d 无法到达终点"), Id));
			}
		}
	}

	const int32 MinBattles = ComputeMinBattlesPerPath(Graph);
	if (MinBattles < Settings.MinBattlesPerPath)
	{
		return Fail(FString::Printf(TEXT("最差路径只有 %d 场战斗，要求至少 %d"), MinBattles, Settings.MinBattlesPerPath));
	}

	OutError.Reset();
	return true;
}

int32 FRouteGraphGenerator::MakeEncounterSeed(int32 RunSeed, int32 NodeId)
{
	uint32 Hash = static_cast<uint32>(RunSeed) * 2654435761u;
	Hash ^= static_cast<uint32>(NodeId) * 2246822519u;
	Hash ^= Hash >> 13;
	Hash *= 3266489917u;
	Hash ^= Hash >> 16;

	// 避免 0：0 在很多系统里表示"随机"。
	return static_cast<int32>(Hash & 0x7FFFFFFFu) | 1;
}

FString FRouteGraphGenerator::MakeReport(const FRouteGraph& Graph)
{
	int32 EdgeCount = 0;
	for (const FRouteNode& Node : Graph.Nodes)
	{
		EdgeCount += Node.Next.Num();
	}

	return FString::Printf(TEXT("seed=%d layers=%d nodes=%d edges=%d battles=%d minBattlesPerPath=%d"),
		Graph.Seed, Graph.NumLayers, Graph.Nodes.Num(), EdgeCount,
		Graph.NumCombatNodes(), ComputeMinBattlesPerPath(Graph));
}
