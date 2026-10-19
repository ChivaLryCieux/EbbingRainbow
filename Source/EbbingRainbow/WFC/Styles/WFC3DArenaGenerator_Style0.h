// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_Style0.generated.h"

/**
 * Style 0：FBX_Export 200cm 模块化套件。
 * 第一套正式风格：地板/通道/格栅 + 实墙/窗墙/门洞 + 外转角 + 圆/八角立柱。
 * 单层战术竞技场，无天花板（俯视相机与 NavMesh 都需要通透）。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_Style0 : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_Style0();

protected:
	virtual bool BuildModules(
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility) override;

	virtual FVector GetCellWorldSize() const override;
};
