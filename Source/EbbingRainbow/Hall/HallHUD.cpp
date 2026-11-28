// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallHUD.h"
#include "Core/ERLog.h"
#include "Hall/HallFPPawn.h"
#include "Hall/HallPlayerController.h"
#include "UI/ERGalaCommandPanel.h"
#include "UI/ERHallHUD.h"
#include "UI/ERPersonnelPanel.h"
#include "UI/ERUITheme.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

AHallHUD::AHallHUD()
{
}

void AHallHUD::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		TSubclassOf<UUserWidget> WidgetClass = UERHallHUD::StaticClass();
		if (const UERUISettings* UISettings = UERUISettings::Get())
		{
			if (!UISettings->HallHUDWidgetClass.IsNull())
			{
				if (UClass* Loaded = UISettings->HallHUDWidgetClass.LoadSynchronous())
				{
					WidgetClass = Loaded;
				}
			}
		}
		HallWidget = CreateWidget<UERHallHUD>(PC, WidgetClass);
		if (HallWidget)
		{
			HallWidget->AddToViewport(10);
			if (AHallPlayerController* HallPC = Cast<AHallPlayerController>(PC))
			{
				if (HallPC->IsFirstPersonActive()) { HallWidget->EnterFirstPersonMode(); }
				else { HallWidget->ShowTitleMenu(); }
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("-HallShot")))
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Tmp;
			World->GetTimerManager().SetTimer(Tmp, []()
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("HallShot"), true, true);
			}, 1.5f, false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("-GalaShot")))
	{
		if (UWorld* World = GetWorld())
		{
			int32 GalaTab = 0;
			FParse::Value(FCommandLine::Get(), TEXT("GalaTab="), GalaTab);
			TWeakObjectPtr<AHallHUD> Self(this);
			FTimerHandle T1, T2;
			World->GetTimerManager().SetTimer(T1, [Self, GalaTab]()
			{
				if (Self.IsValid()) { Self->OpenGalacticPanel(GalaTab); }
			}, 1.2f, false);
			World->GetTimerManager().SetTimer(T2, []()
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("GalaShot"), true, true);
			}, 1.8f, false);
		}
	}
#endif
}

void AHallHUD::ToggleZenMode()
{
	if (HallWidget)
	{
		HallWidget->ToggleZenMode();
	}
}

void AHallHUD::OpenPersonnelPanel()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (!PersonnelPanel)
	{
		TSubclassOf<UUserWidget> PanelClass = UERPersonnelPanel::StaticClass();
		if (const UERUISettings* UISettings = UERUISettings::Get())
		{
			if (!UISettings->PersonnelPanelWidgetClass.IsNull())
			{
				if (UClass* Loaded = UISettings->PersonnelPanelWidgetClass.LoadSynchronous())
				{
					PanelClass = Loaded;
				}
			}
		}

		PersonnelPanel = CreateWidget<UERPersonnelPanel>(PC, PanelClass);
		if (!PersonnelPanel)
		{
			UE_LOG(LogERHall, Error, TEXT("[Hall] Failed to create personnel panel widget (%s)."), *PanelClass->GetName());
			return;
		}
		PersonnelPanel->OnPanelClosed.AddUniqueDynamic(this, &AHallHUD::HandlePersonnelPanelClosed);
		PersonnelPanel->AddToViewport(20);
	}

	if (PersonnelPanel->IsPanelOpen())
	{
		return;
	}

	// 大厅是 FP 漫游时点不到 Dock, 能点到按钮说明光标已释放; 仍然记录真实状态以便还原
	bCursorWasVisibleBeforePanel = PC->bShowMouseCursor;
	ApplyPanelInputMode(true);
	PersonnelPanel->OpenPanel();

	UE_LOG(LogERHall, Log, TEXT("[Hall] Personnel panel opened."));
}

void AHallHUD::ClosePersonnelPanel()
{
	if (!PersonnelPanel || !PersonnelPanel->IsPanelOpen())
	{
		return;
	}
	PersonnelPanel->ClosePanel();
	ApplyPanelInputMode(false);
}

bool AHallHUD::IsPersonnelPanelOpen() const
{
	return PersonnelPanel && PersonnelPanel->IsPanelOpen();
}

void AHallHUD::HandlePersonnelPanelClosed()
{
	// 退场动画已结束: 面板自行 Collapsed, 这里只保证输入状态正确
	ApplyPanelInputMode(false);
	UE_LOG(LogERHall, Log, TEXT("[Hall] Personnel panel closed."));
}

void AHallHUD::OpenGalacticPanel(int32 InitialTab, bool bSettingsOnlyMode)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) { return; }

	if (!GalacticPanel)
	{
		GalacticPanel = CreateWidget<UERGalaCommandPanel>(PC, UERGalaCommandPanel::StaticClass());
		if (!GalacticPanel)
		{
			UE_LOG(LogERHall, Error, TEXT("[Hall] Failed to create GalacticPanel."));
			return;
		}
		GalacticPanel->OnPanelClosed.AddUniqueDynamic(this, &AHallHUD::HandleGalacticPanelClosed);
		GalacticPanel->AddToViewport(25);
	}

	if (!GalacticPanel->IsPanelOpen())
	{
		bCursorWasVisibleBeforePanel = PC->bShowMouseCursor;
		ApplyPanelInputMode(true);
	}
	GalacticPanel->OpenPanel(InitialTab, bSettingsOnlyMode);
	UE_LOG(LogERHall, Log, TEXT("[Hall] Galactic Command Panel opened (tab=%d, settingsOnly=%d)."), InitialTab, (int32)bSettingsOnlyMode);
}

void AHallHUD::CloseGalacticPanel()
{
	if (!GalacticPanel || !GalacticPanel->IsPanelOpen()) { return; }
	GalacticPanel->ClosePanel();
	ApplyPanelInputMode(false);
}

bool AHallHUD::IsGalacticPanelOpen() const
{
	return GalacticPanel && GalacticPanel->IsPanelOpen();
}

void AHallHUD::HandleGalacticPanelClosed()
{
	ApplyPanelInputMode(false);
	UE_LOG(LogERHall, Log, TEXT("[Hall] Galactic Command Panel closed."));
}

void AHallHUD::ApplyPanelInputMode(bool bPanelOpen)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (bPanelOpen)
	{
		if (bCursorWasVisibleBeforePanel && HallWidget)
		{
			// 面板根画布的透明空洞不应把点击漏回标题菜单; 模态期间直接隐藏底层菜单。
			HallWidget->SetTitleMenuHidden(true);
		}
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		return;
	}

	// 还原到打开面板前的状态: 原本在第一人称漫游 -> 收回光标; 原本在 UI 模式 -> 保持光标
	if (bCursorWasVisibleBeforePanel && HallWidget)
	{
		HallWidget->SetTitleMenuHidden(false);
	}
	PC->bShowMouseCursor = bCursorWasVisibleBeforePanel;
	if (bCursorWasVisibleBeforePanel)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}
