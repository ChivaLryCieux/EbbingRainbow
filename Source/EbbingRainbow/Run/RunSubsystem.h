// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Run/RunTypes.h"
#include "RunSubsystem.generated.h"

class ABattleUnit;

/**
 * 一局 Roguelike 的全局状态容器。
 *
 * 为什么是 GameInstanceSubsystem：OpenLevel 会销毁 World，只有 GameInstance 级别的对象
 * 能跨 Hall / Route / Arena 三个关卡存活。路线图、当前节点、队伍血量、种子都存在这里。
 */
UCLASS()
class EBBINGRAINBOW_API URunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 用指定种子开始新的一局；生成失败返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "Run")
	bool StartNewRun(int32 Seed);

	/** 随机种子开始新的一局。 */
	UFUNCTION(BlueprintCallable, Category = "Run")
	bool StartNewRandomRun();

	/** 结束当前局（失败/放弃）。 */
	UFUNCTION(BlueprintCallable, Category = "Run")
	void EndRun();

	UFUNCTION(BlueprintPure, Category = "Run")
	bool HasActiveRun() const { return bRunActive; }

	UFUNCTION(BlueprintPure, Category = "Run")
	int32 GetRunSeed() const { return RunSeed; }

	UFUNCTION(BlueprintPure, Category = "Run")
	int32 GetCurrentNodeId() const { return CurrentNodeId; }

	UFUNCTION(BlueprintPure, Category = "Run")
	int32 GetBattlesFought() const { return BattlesFought; }

	const FRouteGraph& GetGraph() const { return Graph; }
	const FRouteNode* FindNode(int32 NodeId) const { return Graph.FindNode(NodeId); }
	const FRouteNode* GetCurrentNode() const { return FindNode(CurrentNodeId); }
	const FRouteNode* GetPendingNode() const { return FindNode(PendingNodeId); }

	/** 只有"当前节点的直接后继"且未访问过才可选。 */
	bool IsNodeSelectable(int32 NodeId) const;
	TArray<int32> GetSelectableNodes() const;

	/** 前进到节点（安全节点直接调用；战斗节点在战斗胜利后由 CompleteBattle 调用）。 */
	bool AdvanceToNode(int32 NodeId);

	void SetPendingNode(int32 NodeId);
	void ClearPendingNode();
	int32 GetPendingNodeId() const { return PendingNodeId; }

	/** 战斗结束：胜利则推进到待定节点，失败则结束本局。 */
	void CompleteBattle(bool bWon);

	TArray<FRunUnitState>& GetParty() { return Party; }
	const TArray<FRunUnitState>& GetParty() const { return Party; }

	/**
	 * 技能构筑接口 (肉鸽奖励层用): 给队员添加技能。
	 * 技能 Id 由 UBattleSkillLibrary 目录解析, 下一场战斗自动装配。
	 */
	UFUNCTION(BlueprintCallable, Category = "Run|Skills")
	bool GrantSkillToParty(int32 PartyIndex, FName SkillId);

	/** 移除队员技能 (构筑取舍/遗忘) */
	UFUNCTION(BlueprintCallable, Category = "Run|Skills")
	bool RemoveSkillFromParty(int32 PartyIndex, FName SkillId);

	/** 查询队员已构筑技能 */
	UFUNCTION(BlueprintPure, Category = "Run|Skills")
	TArray<FName> GetPartySkillIds(int32 PartyIndex) const;

	/** 把战斗中的存活状态写回队伍（PartyIndices 是战斗中单位对应的队伍下标）。 */
	void WriteBackParty(const TArray<ABattleUnit*>& Units, const TArray<int32>& PartyIndices);

private:
	UPROPERTY()
	FRouteGraph Graph;

	UPROPERTY()
	TArray<FRunUnitState> Party;

	int32 RunSeed = 0;
	int32 CurrentNodeId = INDEX_NONE;
	int32 PendingNodeId = INDEX_NONE;
	int32 BattlesFought = 0;
	bool bRunActive = false;
};
