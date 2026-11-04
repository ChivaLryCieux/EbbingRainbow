// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Core/BattleTypes.h"
#include "RunTypes.generated.h"

/**
 * 路线节点类型。
 *
 * 扩展方式（保持向后兼容）：
 *   1. 在枚举里加一个新值（放在 Count 之前）；
 *   2. 在 RunTypes.cpp 的 GNodeTypeTable 里加一行元数据；
 *   3. 如需新的生成规则，在 FRouteGenSettings 里加权重/层规则。
 * 生成器、校验、UI 全部只查元数据表，不写死 if/else，所以新增类型不需要改 UI 代码。
 */
UENUM(BlueprintType)
enum class ERouteNodeType : uint8
{
	Safe   UMETA(DisplayName = "安全节点"),
	Battle UMETA(DisplayName = "战斗节点"),
	/** 自然漂浮岛战斗 (Nature 生成器) */
	BattleNature UMETA(DisplayName = "自然战斗"),

	/** 哨兵：不是合法类型，仅用于表长度/遍历。新增类型请加在这一行之前。 */
	Count  UMETA(Hidden)
};

/** 节点类型的元数据（表现 + 规则），单一真相源。 */
struct FRouteNodeTypeInfo
{
	ERouteNodeType Type = ERouteNodeType::Safe;
	/** 中文名，用于 UI 与日志 */
	const TCHAR* DisplayName = TEXT("未知");
	/** 单字字形，画在节点徽章里（避免使用可能缺字形的 emoji） */
	const TCHAR* Glyph = TEXT("?");
	/** 节点主题色 */
	FLinearColor Color = FLinearColor(0.6f, 0.7f, 0.8f, 1.0f);
	/** 是否进入战斗（生成器的"每路径至少 N 场战斗"约束靠它统计） */
	bool bIsCombat = false;
};

/** 按类型取元数据；越界返回 Safe 的元数据，保证 UI 永不崩。 */
EBBINGRAINBOW_API const FRouteNodeTypeInfo& GetRouteNodeTypeInfo(ERouteNodeType Type);
EBBINGRAINBOW_API bool IsRouteNodeCombat(ERouteNodeType Type);

/**
 * 一个路线节点。
 * 约定：Nodes 数组下标 == Id；Layer 从 0 单调递增，边只从 Layer 指向 Layer+1，
 * 因此结构上不可能出现环，也不可能走回头路。
 */
USTRUCT(BlueprintType)
struct EBBINGRAINBOW_API FRouteNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 Id = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 Layer = 0;

	/** 层内列位（0..NumSlots-1），决定横向位置；跨层按 Slot 单调连边即可保证不交叉。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 Slot = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	ERouteNodeType Type = ERouteNodeType::Safe;

	/** 只指向 Layer+1 的节点 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	TArray<int32> Next;

	/** 只来自 Layer-1 的节点（冗余存储，方便 UI 画"已走路径"） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	TArray<int32> Prev;

	/** 该节点的遭遇随机种子 = f(RunSeed, Id)，保证同一局同一节点内容一致。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 EncounterSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	bool bVisited = false;
};

/** 一整张路线图。 */
USTRUCT(BlueprintType)
struct EBBINGRAINBOW_API FRouteGraph
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 NumLayers = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 NumSlots = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 StartNodeId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	int32 EndNodeId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	TArray<FRouteNode> Nodes;

	const FRouteNode* FindNode(int32 Id) const
	{
		return Nodes.IsValidIndex(Id) ? &Nodes[Id] : nullptr;
	}

	FRouteNode* FindNode(int32 Id)
	{
		return Nodes.IsValidIndex(Id) ? &Nodes[Id] : nullptr;
	}

	bool IsValid() const
	{
		return Nodes.Num() > 0 && Nodes.IsValidIndex(StartNodeId) && Nodes.IsValidIndex(EndNodeId);
	}

	int32 NumCombatNodes() const
	{
		int32 Count = 0;
		for (const FRouteNode& Node : Nodes)
		{
			if (IsRouteNodeCombat(Node.Type))
			{
				++Count;
			}
		}
		return Count;
	}

	int32 NumVisitedNodes() const
	{
		int32 Count = 0;
		for (const FRouteNode& Node : Nodes)
		{
			if (Node.bVisited)
			{
				++Count;
			}
		}
		return Count;
	}
};

/**
 * 生成参数。全部可调，方便以后做不同章节/难度。
 * 约束：NumLayers 含起点层与终点层；中间层节点数在 [MinNodesPerLayer, MaxNodesPerLayer]。
 */
USTRUCT(BlueprintType)
struct EBBINGRAINBOW_API FRouteGenSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	int32 NumLayers = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	int32 MinNodesPerLayer = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	int32 MaxNodesPerLayer = 4;

	/** 横向列数：节点 Slot 的取值范围。越大路线越"宽"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	int32 NumSlots = 7;

	/** 每一条从起点到终点的路径至少包含多少场战斗。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	int32 MinBattlesPerPath = 5;

	/** 中间层节点初始为战斗的概率（随后会被"每路径至少 N 战"修正）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	float BattleChance = 0.55f;

	/** 战斗节点是自然岛场景的概率 (其余为体素场景)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	float NatureBattleChance = 0.4f;

	/** 生成失败（校验不过）时最多重试几次；每次重试使用派生种子，仍然完全确定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route")
	int32 MaxAttempts = 64;
};

/**
 * 一局游戏里某个角色的持久状态。
 * 战斗结束后由 BattleGameMode 把 Integrity 写回，实现"血量跨战斗继承"。
 */
USTRUCT(BlueprintType)
struct EBBINGRAINBOW_API FRunUnitState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FName UnitId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FBattleUnitStats Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	float Integrity = 0.0f;

	bool IsAlive() const { return Integrity > 0.0f; }
};
