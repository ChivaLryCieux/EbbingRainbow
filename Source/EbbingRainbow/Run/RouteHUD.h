// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RouteHUD.generated.h"

class SRouteMapOverlay;

/** 航线图 HUD：把纯 Slate 的 SRouteMapOverlay 挂到视口，并处理节点点击后的跳转。 */
UCLASS()
class EBBINGRAINBOW_API ARouteHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 节点被点选（也供调试命令 RouteSelect 复用，保证与鼠标点击同一条路径）。 */
	void HandleNodeClicked(int32 NodeId);

private:
	TSharedPtr<SRouteMapOverlay> Overlay;
};
