// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleArena.generated.h"

class AWFC3DGenerator;
class UNavigationSystemV1;

DECLARE_LOG_CATEGORY_EXTERN(LogBattleArena, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArenaGenerated, bool, bSuccess);

/**
 * 回合制战斗竞技场。
 * 流程: 用 AWFC3DGenerator 生成白模地形 -> 重建 NavMesh -> 基于 NavMesh 校验连通性 -> 选出双方出生点。
 * 校验失败会换种子重试。关卡中需要放置一个覆盖竞技场的 NavMeshBoundsVolume,并将导航 Runtime Generation 设为 Dynamic。
 */
UCLASS(Blueprintable)
class EBBINGRAINBOW_API ABattleArena : public AActor
{
	GENERATED_BODY()

public:
	ABattleArena();

	// ---------- 配置 ----------

	/** WFC 网格尺寸 (X, Y, Z 层数)。原型使用单层 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	FIntVector GridCells = FIntVector(14, 14, 1);

	/** 单个 WFC 单元格的世界尺寸 (cm)。内置 Dungeon 预设单元为 100cm,通过生成器 Actor 缩放得到 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena", meta = (ClampMin = "100"))
	float CellWorldSize = 300.0f;

	/** 基础种子。0 = 每次随机 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	int32 BaseSeed = 0;

	/** BeginPlay 时自动生成 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	bool bAutoGenerateOnBeginPlay = true;

	/** 校验失败时的最大重试次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena", meta = (ClampMin = "1"))
	int32 MaxGenerationAttempts = 150;

	// ---------- 校验与出生 ----------

	/** 每方单位数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawn", meta = (ClampMin = "1"))
	int32 UnitsPerTeam = 4;

	/** 出生点之间最小间距 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawn")
	float MinSpawnSpacing = 250.0f;

	/** 出生区占可达区域 X 方向跨度的比例 (左侧=我方,右侧=敌方) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawn", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	float SpawnZoneFraction = 0.3f;

	/** 可达采样点去重 (按 200cm 格) 后至少需要的格数,用于判定地图是否够大 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Validation")
	int32 MinReachableCells = 60;

	/** 可达区域在 X 方向的跨度至少占竞技场宽度的比例 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Validation", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MinReachableSpanFraction = 0.5f;

	/** 用于连通性估计的随机可达点采样数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Validation")
	int32 ReachabilitySamples = 600;

	/** 绘制调试出生点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Debug")
	bool bDrawDebugSpawns = true;

	// ---------- 事件 ----------

	UPROPERTY(BlueprintAssignable, Category = "Arena")
	FOnArenaGenerated OnArenaGenerated;

	// ---------- 接口 ----------

	/** 生成竞技场 (含重试)。成功返回 true */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	bool GenerateArena();

	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsArenaReady() const { return bArenaReady; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	const TArray<FVector>& GetPlayerSpawnPoints() const { return PlayerSpawnPoints; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	const TArray<FVector>& GetEnemySpawnPoints() const { return EnemySpawnPoints; }

	/** 实际使用的种子 (用于复现) */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetUsedSeed() const { return UsedSeed; }

	/** 竞技场 XY 包围盒 (由配置确定,无需先生成) */
	UFUNCTION(BlueprintPure, Category = "Arena")
	FBox GetArenaBounds() const;

	UFUNCTION(BlueprintPure, Category = "Arena")
	FVector GetArenaCenter() const { return GetArenaBounds().GetCenter(); }

protected:
	virtual void BeginPlay() override;

private:
	AWFC3DGenerator* EnsureGenerator();
	bool TryBuildAndValidate(UNavigationSystemV1* NavSys, int32 Seed);

	UPROPERTY(Transient)
	TObjectPtr<AWFC3DGenerator> Generator = nullptr;

	TArray<FVector> PlayerSpawnPoints;
	TArray<FVector> EnemySpawnPoints;
	int32 UsedSeed = 0;
	bool bArenaReady = false;
};
