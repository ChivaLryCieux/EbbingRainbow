// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_MegaAtrium.generated.h"

/**
 * 超大型鸟巢/商场贯通中庭原型生成器。
 * 中央椭圆/矩形通高中庭 + 环形漫游回廊 + 层间观景楼梯。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_MegaAtrium : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_MegaAtrium();

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium")
	bool bIncludeCeilings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium")
	bool bEnsureStairConnectivity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium", meta = (ClampMin = "1", ClampMax = "4"))
	int32 StairsPerFloorTransition = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium", meta = (ClampMin = "2", ClampMax = "12"))
	int32 AtriumWalkwayDepth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium")
	bool bOvalAtriumShape = true;
};
