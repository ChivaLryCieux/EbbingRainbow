// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_SpiralGarden.generated.h"

/**
 * 螺旋花园高塔原型生成器。
 * 单一 Cube 拼装的螺旋缎带：中央贯通天井 + 逐层旋转的扇形步道 + 层间楼梯/坡道 + 拱门/花坛。
 * 全部拓扑由 BuildConstraints 以固定格形式写入，WFC 只负责校验与铺块。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_SpiralGarden : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_SpiralGarden();

protected:
	virtual bool BuildModules(
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility) override;

	virtual bool BuildConstraints(
		const FIntVector& GridSize,
		int32 Seed,
		TMap<int32, int32>& OutClamps) override;

	virtual FVector GetCellWorldSize() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float SpiralTwistPerLayer = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "6.0", ClampMax = "80.0"))
	float SpiralArcOverlapDeg = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "3", ClampMax = "16"))
	int32 SpiralRibbonInnerRadius = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "4", ClampMax = "30"))
	int32 SpiralRibbonOuterRadius = 21;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralRibbonWidenChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden")
	bool bGenerateSpiralArchways = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralArchwayChance = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SpiralArchwayMinSpacing = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden")
	bool bGenerateSpiralPlanters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralPlanterEdgeChance = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralPlanterChance = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SpiralPlanterMinSpacing = 2;

	/** BuildModules 时从 Cube 包围盒计算出的实际格子边长（cm） */
	float ComputedCubeEdge = 200.0f;
};
