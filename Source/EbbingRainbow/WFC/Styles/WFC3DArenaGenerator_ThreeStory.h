// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_ThreeStory.generated.h"

/**
 * 三层空间 / 四层地板原型生成器。
 * 使用 FBX_Export 200cm 套件，带天花封顶与层间楼梯强连通保证。
 * 这是一个独立的生成器，不与 Style0 共用任何拓扑代码。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_ThreeStory : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_ThreeStory();

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|ThreeStory")
	bool bIncludeCeilings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|ThreeStory")
	bool bEnforceGroundFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|ThreeStory")
	bool bEnsureStairConnectivity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|ThreeStory", meta = (ClampMin = "1", ClampMax = "4"))
	int32 StairsPerFloorTransition = 1;
};
