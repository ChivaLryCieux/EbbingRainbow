// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"

AHallPlayerController::AHallPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AHallPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AHallPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHallPlayerController::OnKeyEscape);
	}
}

void AHallPlayerController::OnKeyEscape()
{
	if (AHallHUD* HHUD = Cast<AHallHUD>(GetHUD()))
	{
		HHUD->ToggleZenMode();
	}
}
