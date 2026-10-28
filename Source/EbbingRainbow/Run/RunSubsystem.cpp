// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RunSubsystem.h"

#include "Battle/Core/BattleGameMode.h"
#include "Battle/Units/BattleUnit.h"
#include "Run/RouteGraphGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogRun, Log, All);

bool URunSubsystem::StartNewRun(int32 Seed)
{
	// 第一章：10 层、每层 2~4 节点、任意路径至少 5 场战斗。
	FRouteGenSettings Settings;
	Settings.NumLayers = 10;
	Settings.MinNodesPerLayer = 2;
	Settings.MaxNodesPerLayer = 4;
	Settings.NumSlots = 7;
	Settings.MinBattlesPerPath = 5;
	Settings.BattleChance = 0.55f;

	FString Report;
	FRouteGraph NewGraph;
	if (!FRouteGraphGenerator::Generate(Seed, Settings, NewGraph, &Report))
	{
		UE_LOG(LogRun, Error, TEXT("[Run] 生成路线图失败 seed=%d"), Seed);
		return false;
	}

	Graph = MoveTemp(NewGraph);
	RunSeed = Seed;
	CurrentNodeId = Graph.StartNodeId;
	PendingNodeId = INDEX_NONE;
	BattlesFought = 0;
	bRunActive = true;
	Graph.Nodes[CurrentNodeId].bVisited = true;

	Party.Reset();
	for (const FBattleUnitStats& Stats : ABattleGameMode::GetDefaultPlayerRoster())
	{
		FRunUnitState Unit;
		Unit.UnitId = FName(*Stats.DisplayName);
		Unit.Stats = Stats;
		Unit.Integrity = Stats.MaxIntegrity;
		Party.Add(Unit);
	}

	UE_LOG(LogRun, Log, TEXT("[Run] 新的一局开始 %s"), *Report);
	return true;
}

bool URunSubsystem::StartNewRandomRun()
{
	const int32 Seed = FMath::RandRange(1, 2147483646);
	return StartNewRun(Seed);
}

void URunSubsystem::EndRun()
{
	if (!bRunActive)
	{
		return;
	}

	bRunActive = false;
	PendingNodeId = INDEX_NONE;
	UE_LOG(LogRun, Log, TEXT("[Run] 本局结束 seed=%d 已走节点=%d"), RunSeed, Graph.NumVisitedNodes());
}

bool URunSubsystem::IsNodeSelectable(int32 NodeId) const
{
	if (!bRunActive || !Graph.IsValid())
	{
		return false;
	}

	const FRouteNode* Current = Graph.FindNode(CurrentNodeId);
	const FRouteNode* Node = Graph.FindNode(NodeId);
	if (!Current || !Node || Node->bVisited)
	{
		return false;
	}

	return Current->Next.Contains(NodeId);
}

TArray<int32> URunSubsystem::GetSelectableNodes() const
{
	TArray<int32> Result;
	const FRouteNode* Current = Graph.FindNode(CurrentNodeId);
	if (!bRunActive || !Current)
	{
		return Result;
	}

	for (const int32 NextId : Current->Next)
	{
		if (IsNodeSelectable(NextId))
		{
			Result.Add(NextId);
		}
	}
	return Result;
}

bool URunSubsystem::AdvanceToNode(int32 NodeId)
{
	if (!IsNodeSelectable(NodeId))
	{
		return false;
	}

	FRouteNode& Node = Graph.Nodes[NodeId];
	Node.bVisited = true;
	CurrentNodeId = NodeId;
	PendingNodeId = INDEX_NONE;

	if (IsRouteNodeCombat(Node.Type))
	{
		++BattlesFought;
	}

	UE_LOG(LogRun, Log, TEXT("[Run] 前进到节点 %d (第 %d 层, %s)"), Node.Id, Node.Layer + 1, GetRouteNodeTypeInfo(Node.Type).DisplayName);
	return true;
}

void URunSubsystem::SetPendingNode(int32 NodeId)
{
	if (Graph.FindNode(NodeId) != nullptr)
	{
		PendingNodeId = NodeId;
	}
}

void URunSubsystem::ClearPendingNode()
{
	PendingNodeId = INDEX_NONE;
}

void URunSubsystem::CompleteBattle(bool bWon)
{
	if (!bRunActive)
	{
		return;
	}

	if (bWon)
	{
		if (PendingNodeId != INDEX_NONE)
		{
			AdvanceToNode(PendingNodeId);
		}
		PendingNodeId = INDEX_NONE;
	}
	else
	{
		EndRun();
	}
}

void URunSubsystem::WriteBackParty(const TArray<ABattleUnit*>& Units, const TArray<int32>& PartyIndices)
{
	for (int32 Index = 0; Index < Units.Num() && Index < PartyIndices.Num(); ++Index)
	{
		ABattleUnit* Unit = Units[Index];
		const int32 PartyIndex = PartyIndices[Index];
		if (!Unit || !Party.IsValidIndex(PartyIndex))
		{
			continue;
		}

		Party[PartyIndex].Integrity = FMath::Clamp(Unit->Integrity, 0.0f, Party[PartyIndex].Stats.MaxIntegrity);
	}
}
