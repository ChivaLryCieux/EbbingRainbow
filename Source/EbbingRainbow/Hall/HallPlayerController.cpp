#include "Hall/HallPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Core/ERDebug.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Hall/HallCameraPawn.h"
#include "Hall/HallConsoleActor.h"
#include "Hall/HallFPPawn.h"
#include "Hall/HallFirstPersonCharacter.h"
#include "Hall/HallHUD.h"
#include "InputMappingContext.h"
#include "UI/ERHallHUD.h"

AHallPlayerController::AHallPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// 与 Lvl_Legion 指挥官共用基础移动/鼠标视角上下文; 大厅不加载 IMC_Weapons。
	DefaultMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Input/IMC_Default.IMC_Default")));
	MouseLookMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook")));
}

void AHallPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -82.0f;
		PlayerCameraManager->ViewPitchMax = 82.0f;
	}

	if (ER::Debug::IsHallShowcase())
	{
		SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		return;
	}

	// 战后返回大厅时, 战役子系统仍记得玩家已经进入过会话 —— 跳过标题直接落地。
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (Campaign && Campaign->IsCampaignSessionActive())
	{
		EnterFirstPerson();
	}
	else
	{
		ApplyTitleInputMode();
		ApplyMenuCamera(false);
	}
}

void AHallPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (Cast<AHallFirstPersonCharacter>(InPawn) || Cast<AHallFPPawn>(InPawn))
	{
		if (bFirstPersonActive)
		{
			ApplyFirstPersonInputMode();
		}
		else
		{
			ApplyTitleInputMode();
		}
		return;
	}

	if (AHallCameraPawn* PawnCam = Cast<AHallCameraPawn>(InPawn))
	{
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
	if (!InputComponent) { return; }

	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHallPlayerController::OnKeyEscape);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AHallPlayerController::OnKeyInteract);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AHallPlayerController::OnLeftClick);
}

bool AHallPlayerController::IsAnyHallPanelOpen() const
{
	if (const AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
	{
		return HallHUD->IsGalacticPanelOpen() || HallHUD->IsPersonnelPanelOpen();
	}
	return false;
}

void AHallPlayerController::EnterFirstPerson()
{
	if (ER::Debug::IsHallShowcase() || bFirstPersonActive) { return; }
	bFirstPersonActive = true;

	if (AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
	{
		if (UERHallHUD* HallWidget = HallHUD->GetHallWidget())
		{
			HallWidget->EnterFirstPersonMode();
		}
	}

	ApplyFirstPersonInputMode();
	if (APawn* ControlledPawn = GetPawn())
	{
		SetViewTargetWithBlend(ControlledPawn, 0.55f, EViewTargetBlendFunction::VTBlend_EaseInOut, 1.5f);
	}
	UpdateInputContextState();
}

void AHallPlayerController::ReturnToTitleMenu()
{
	if (ER::Debug::IsHallShowcase() || !bFirstPersonActive) { return; }
	bFirstPersonActive = false;

	ClearInteractionFocus();
	if (AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
	{
		if (UERHallHUD* HallWidget = HallHUD->GetHallWidget())
		{
			HallWidget->ShowTitleMenu();
		}
	}

	ApplyTitleInputMode();
	ApplyMenuCamera(true);
	UpdateInputContextState();
}

void AHallPlayerController::ApplyTitleInputMode()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

void AHallPlayerController::ApplyFirstPersonInputMode()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());
}

void AHallPlayerController::ApplyMenuCamera(bool bBlend)
{
	if (ER::Debug::IsHallShowcase()) { return; }
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHallCameraPawn> It(World); It; ++It)
		{
			if (AHallCameraPawn* MenuCamera = *It)
			{
				SetViewTargetWithBlend(MenuCamera, bBlend ? 0.55f : 0.0f,
					EViewTargetBlendFunction::VTBlend_EaseInOut, 1.5f);
				return;
			}
		}
	}
}

void AHallPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateInputContextState();
	UpdateInteractionFocus(DeltaTime);
}

void AHallPlayerController::UpdateInputContextState()
{
	const bool bWantsContexts = bFirstPersonActive && !bShowMouseCursor && !IsAnyHallPanelOpen();
	SetInputContextsActive(bWantsContexts);
}

void AHallPlayerController::ResetFirstPersonInputState()
{
	if (AHallFirstPersonCharacter* HallCharacter = Cast<AHallFirstPersonCharacter>(GetPawn()))
	{
		HallCharacter->DoSprintEnd();
		HallCharacter->StopJumping();
	}
}

void AHallPlayerController::SetInputContextsActive(bool bActive)
{
	if (bInputContextsActive == bActive) { return; }
	if (!IsLocalPlayerController()) { return; }

	if (!bActive)
	{
		ResetFirstPersonInputState();
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem) { return; }

	for (const TSoftObjectPtr<UInputMappingContext>& Ctx : { DefaultMappingContext, MouseLookMappingContext })
	{
		if (bActive)
		{
			if (UInputMappingContext* MC = Ctx.LoadSynchronous())
			{
				Subsystem->AddMappingContext(MC, 0);
			}
		}
		else if (UInputMappingContext* MC = Ctx.Get())
		{
			Subsystem->RemoveMappingContext(MC);
		}
	}
	bInputContextsActive = bActive;
}

void AHallPlayerController::UpdateInteractionFocus(float DeltaTime)
{
	if (!bFirstPersonActive || bShowMouseCursor || IsAnyHallPanelOpen())
	{
		ClearInteractionFocus();
		return;
	}

	InteractionScanTimer -= DeltaTime;
	if (InteractionScanTimer > 0.0f) { return; }
	InteractionScanTimer = 0.12f;

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();

	AHallConsoleActor* BestConsole = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHallConsoleActor> It(World); It; ++It)
		{
			AHallConsoleActor* Console = *It;
			float DistanceSq = 0.0f;
			if (Console && Console->IsInteractionCandidate(ViewLocation, ViewForward, DistanceSq)
				&& DistanceSq < BestDistanceSq)
			{
				BestConsole = Console;
				BestDistanceSq = DistanceSq;
			}
		}
	}

	if (BestConsole != FocusedConsole.Get())
	{
		FocusedConsole = BestConsole;
		if (AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
		{
			if (UERHallHUD* HallWidget = HallHUD->GetHallWidget())
			{
				HallWidget->SetInteractionPromptVisible(BestConsole != nullptr,
					BestConsole ? BestConsole->GetInteractionPrompt() : FText::GetEmpty());
			}
		}
	}
}

void AHallPlayerController::ClearInteractionFocus()
{
	if (FocusedConsole.IsValid())
	{
		FocusedConsole = nullptr;
		if (AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
		{
			if (UERHallHUD* HallWidget = HallHUD->GetHallWidget())
			{
				HallWidget->SetInteractionPromptVisible(false);
			}
		}
	}
}

void AHallPlayerController::OnKeyEscape()
{
	// 面板优先吃掉 Esc: 先关面板, 再谈标题/观景切换。
	if (AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
	{
		if (HallHUD->IsGalacticPanelOpen())
		{
			HallHUD->CloseGalacticPanel();
			return;
		}
		if (HallHUD->IsPersonnelPanelOpen())
		{
			HallHUD->ClosePersonnelPanel();
			return;
		}
	}

	if (ER::Debug::IsHallShowcase())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(GetHUD()))
		{
			HallHUD->ToggleZenMode();
		}
		return;
	}

	if (bFirstPersonActive)
	{
		ReturnToTitleMenu();
	}
}

void AHallPlayerController::OnKeyInteract()
{
	if (!bFirstPersonActive || bShowMouseCursor || IsAnyHallPanelOpen()) { return; }
	if (AHallConsoleActor* Console = FocusedConsole.Get())
	{
		Console->Interact(this);
	}
}

void AHallPlayerController::OnLeftClick()
{
	// 防御性兜底: 若外部 UI 曾释放光标但没有打开面板, 点击视口重新捕获第一人称视角。
	if (bFirstPersonActive && bShowMouseCursor && !IsAnyHallPanelOpen())
	{
		ApplyFirstPersonInputMode();
	}
}
