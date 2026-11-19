// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallHUD.h"
#include "Core/ERLog.h"
#include "Hall/HallFPPawn.h"
#include "UI/ERHallHUD.h"
#include "UI/ERPersonnelPanel.h"
#include "UI/ERUITheme.h"
#include "GameFramework/PlayerController.h"

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

void AHallHUD::ApplyPanelInputMode(bool bPanelOpen)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (bPanelOpen)
	{
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
