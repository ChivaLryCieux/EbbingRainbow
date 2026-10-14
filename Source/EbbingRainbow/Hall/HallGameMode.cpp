// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallGameMode.h"
#include "Hall/HallCameraPawn.h"
#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"

AHallGameMode::AHallGameMode()
{
	DefaultPawnClass = AHallCameraPawn::StaticClass();
	PlayerControllerClass = AHallPlayerController::StaticClass();
	HUDClass = AHallHUD::StaticClass();
}
