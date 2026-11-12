#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"
#include "Hall/HallCameraPawn.h"
#include "Hall/HallFPPawn.h"
#include "EngineUtils.h"

AHallPlayerController::AHallPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AHallPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Possess 完成时才知道 Pawn 类型 —— 输入模式按类型分派
	// (BeginPlay 时 GetPawn() 可能还为 null)
	if (Cast<AHallFPPawn>(InPawn))
	{
		// 第一人称: 锁定鼠标进视口, 藏光标 -> 进入 look 模式
		SetShowMouseCursor(false);
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		return;
	}

	if (AHallCameraPawn* PawnCam = Cast<AHallCameraPawn>(InPawn))
	{
		// 观景模式: 视口视角与朝向绑定到摆放的 HallCameraPawn
		SetViewTarget(PawnCam);
		SetControlRotation(PawnCam->GetActorRotation());
		SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
}

void AHallPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHallPlayerController::OnKeyEscape);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,
			this, &AHallPlayerController::OnLeftClick);
	}
}

void AHallPlayerController::OnKeyEscape()
{
	if (Cast<AHallFPPawn>(GetPawn()))
	{
		// FP: Esc 释放光标回 UI 菜单 (可点底部 Dock 的航线管理)
		SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		return;
	}

	// 观景模式: Esc = 全屏观景开关 (原行为)
	if (AHallHUD* HHUD = Cast<AHallHUD>(GetHUD()))
	{
		HHUD->ToggleZenMode();
	}
}

void AHallPlayerController::OnLeftClick()
{
	// FP + 光标已释放时点视口: 收回鼠标返回 look 模式
	// (点到 UI 按钮时按钮先吃事件, 此处只在点空处触发)
	if (Cast<AHallFPPawn>(GetPawn()) && bShowMouseCursor)
	{
		SetShowMouseCursor(false);
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
}
