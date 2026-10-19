// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_Dungeon.generated.h"

/**
 * 调试/原型风格：100cm Cube 拼装的地牢。
 * 用于快速验证插槽规则、门洞/转角拓扑，不用于正式美术。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_Dungeon : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_Dungeon();

protected:
	virtual bool BuildModules(
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility) override;

	virtual FVector GetCellWorldSize() const override;
};
