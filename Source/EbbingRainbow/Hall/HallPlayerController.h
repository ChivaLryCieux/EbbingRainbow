// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HallPlayerController.generated.h"

/**
 * 空间站大厅玩家控制器:
 * 开启鼠标指针与悬停事件, 支持按 Esc 键退出或恢复全屏观景。
 */
UCLASS()
class EBBINGRAINBOW_API AHallPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHallPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnKeyEscape();
};
