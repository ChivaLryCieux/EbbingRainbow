// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallHUD.h"
#include "Hall/UI/HallMainWidget.h"
#include "GameFramework/PlayerController.h"

AHallHUD::AHallHUD()
{
}

void AHallHUD::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		HallWidget = CreateWidget<UHallMainWidget>(PC);
		if (HallWidget)
		{
			HallWidget->AddToViewport(10);
		}
	}
}

void AHallHUD::ToggleZenMode()
{
	if (HallWidget)
	{
		HallWidget->ToggleZenMode();
	}
}
