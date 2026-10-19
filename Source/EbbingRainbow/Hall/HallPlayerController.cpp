#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"
#include "Hall/HallCameraPawn.h"
#include "EngineUtils.h"

AHallPlayerController::AHallPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AHallPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 保证视口视角与朝向 100% 绑定到摆放的 HallCameraPawn
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHallCameraPawn> It(World); It; ++It)
		{
			if (AHallCameraPawn* PawnCam = *It)
			{
				SetViewTarget(PawnCam);
				SetControlRotation(PawnCam->GetActorRotation());
				break;
			}
		}
	}

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
