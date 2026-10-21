// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_SciFi01.generated.h"

/**
 * SciFi01：SciFiInterior01 套件的两层复杂结构。
 *
 * 平面概念：
 *   - 14x12 格、4m 格、3m 层高（两层共 6m）。
 *   - 中央 8x8 格通高大厅（Z=0 地面，Z=1 挑空）。
 *   - 大厅外围是一圈走廊；上层在走廊上方形成回廊，回廊内缘设栏杆。
 *   - 固定 3 部楼梯：左下、右下、上中；每部楼梯对应一个上层楼梯井。
 *   - 中心大厅放树；回廊下放 4 根 3m 柱；上层楼梯井和挑空边缘自动加栏杆。
 *
 * 墙体/走廊使用 SciFiInterior01 套件；墙按单面使用。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_SciFi01 : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_SciFi01();

protected:
	virtual bool BuildModules(
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility) override;

	virtual bool BuildConstraints(
		const FIntVector& GridSize,
		int32 Seed,
		TMap<int32, int32>& OutClamps) override;

	virtual void PostProcess(int32 Seed) override;

	virtual FVector GetCellWorldSize() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SciFi01", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float TreeScale = 0.35f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> FloorMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> WallMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CornerPostMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> StairMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RailingMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ColumnMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> TreeMesh = nullptr;

	// 最近一次 BuildConstraints 生成的随机平面，供 PostProcess 放树/柱/栏杆。
	TArray<bool> PlanWalkable;
	TArray<int32> PlanRoomIds;          // -1 = 走廊；>=0 = 房间 ID
	TArray<uint8> PlanUpperWallMasks;   // 上层每格四边墙掩码
	TArray<int32> PlanStairCells;       // 楼梯所在格索引
	TArray<int32> PlanStairYaws;        // 0/90/180/270
	TArray<int32> PlanStairOpenEdges;   // 0=+X,1=+Y,2=-X,3=-Y
	int32 PlanHallRoomId = INDEX_NONE;
	int32 PlanWidth = 0;
	int32 PlanHeight = 0;
};
