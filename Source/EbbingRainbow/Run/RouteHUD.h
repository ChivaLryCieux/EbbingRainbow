// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RouteHUD.generated.h"

class UERRouteHUD;

/** 航线图 HUD：加载航线图 UMG 蓝图 (WBP_RouteHUD)，并处理节点点击后的跳转与 3D 场景几何对齐。 */
UCLASS()
class EBBINGRAINBOW_API ARouteHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 节点被点选（也供调试命令 RouteSelect 复用，保证与鼠标点击同一条路径）。 */
	UFUNCTION()
	void HandleNodeClicked(int32 NodeId);

	/** 画布实际落在视口里的位置 (视口像素原点 + 画布单位->像素缩放); 3D 场景用它与节点对齐。首帧布局前返回 false。 */
	bool GetMapLayout(FVector2D& OutOriginPx, float& OutScalePx) const;

	UERRouteHUD* GetRouteWidget() const { return RouteWidget; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UERRouteHUD> RouteWidget;
};
