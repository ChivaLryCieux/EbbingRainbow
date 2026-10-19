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

	virtual bool BuildConstraints(
		const FIntVector& GridSize,
		int32 Seed,
		TMap<int32, int32>& OutClamps) override;

	virtual void PostProcess(int32 Seed) override;

	virtual FVector GetCellWorldSize() const override;

	/**
	 * 基础房间模式（默认开启）：
	 * 矩形房间，四周只有墙，内部满地板，中心是低矮花坛 + 一棵树。
	 * 只使用 Floor_Flat / Wall_Solid / Corner_Outer / 树。
	 * 关闭后回到原来的 WFC 随机模块布局。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style0")
	bool bBasicRoomLayout = true;

	/** 中心树的缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style0", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float TreeScale = 0.25f;

	/** 花坛矮墙高度（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style0", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float PlanterWallHeight = 45.0f;

	/** 花坛土床相对地板抬高（cm），用于避免与底层地板 z-fighting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style0", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float PlanterRise = 5.0f;

private:
	bool BuildBasicRoomModules(
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility);

	void BuildBasicRoomDecorations();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BasicFloorMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BasicWallMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BasicTreeMesh = nullptr;
};
