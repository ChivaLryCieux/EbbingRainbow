// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoutePlayerController.generated.h"

/** 航线图输入：只负责鼠标指针与 UI 输入模式。 */
UCLASS()
class EBBINGRAINBOW_API ARoutePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARoutePlayerController();

	virtual void BeginPlay() override;

	/** 调试：把当前航线图（节点/边/类型）打到日志，用于同种子对比。 */
	UFUNCTION(Exec)
	void RouteDump();

	/** 调试：按节点 Id 走一次和鼠标点击完全相同的逻辑。 */
	UFUNCTION(Exec)
	void RouteSelect(int32 NodeId);
};
