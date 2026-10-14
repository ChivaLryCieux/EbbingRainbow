// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HallGameMode.generated.h"

/**
 * 空间站大厅游戏模式:
 * 绑定空间站镜头展示 Pawn、大厅交互控制器与明日方舟风格高科技 HUD。
 */
UCLASS()
class EBBINGRAINBOW_API AHallGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHallGameMode();
};
