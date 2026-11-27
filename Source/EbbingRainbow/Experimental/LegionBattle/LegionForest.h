// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LegionForest.generated.h"

class FLegionTerrainGrid;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/** 沿战线方向的带状清空区 (支持任意交战轴向角) */
struct FLegionOrientedBand
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Forward = FVector2D(0.0f, 1.0f);
	float HalfDepth = 2800.0f;
};

/**
 * 军团战场林地 (战术植被):
 *  - 种子化散布 Fishermans_Cabin 的 SM_Fir_Tree_01..08 (HISM + Nanite, 无碰撞, 不影响三类 WorldStatic 地面射线);
 *  - 噪声成林 + 抖动格点采样, 避开部署带与陡坡;
 *  - 树冠同步登记进 FLegionTerrainGrid: 密林格封锁寻路 / 疏林格给远程掩体。
 * 同一 Seed + 同一 DensityScale 必然生成同一片林子。
 */
UCLASS()
class EBBINGRAINBOW_API ALegionForest : public AActor
{
	GENERATED_BODY()

public:
	ALegionForest();

	/** 清空旧树, 按种子重新散布并写入地形网格; 返回成功种下的树数 */
	int32 Generate(UWorld* World, FLegionTerrainGrid& Grid, int32 Seed, float DensityScale,
		const TArray<FLegionOrientedBand>& ExclusionBands);

	void ClearTrees();

	int32 GetTreeCount() const { return TreeCount; }

	// ---------------- 调参 ----------------

	/** 抖动格点间距 (cm) */
	float CandidateSpacing = 380.0f;

	/** 林子大块噪声波长 (cm) */
	float ForestWavelength = 11000.0f;

	/** 噪声高于此值才成林 (越大林子越少) */
	float ForestThreshold = 0.06f;

	/** 林外零星孤树概率 */
	float SparseChance = 0.025f;

	/** 超过此坡度不种树 (度) */
	float MaxTreeSlopeDeg = 22.0f;

	/** 树冠影响半径 (cm): 覆盖到的格 TreeCount +1 */
	float CrownRadius = 330.0f;

	/** TreeCount >= 此值的格视为密林, 禁止通行 */
	int32 BlockThreshold = 2;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> TreeComps;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> TreeMeshes;

	int32 TreeCount = 0;
};
