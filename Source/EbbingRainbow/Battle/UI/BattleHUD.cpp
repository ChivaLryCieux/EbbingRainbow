// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/BattleHUD.h"
#include "Battle/UI/BattleMainWidget.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Hall/UI/HallMainWidget.h"
#include "Engine/Canvas.h"

ABattleHUD::ABattleHUD()
{
}

void ABattleHUD::BeginPlay()
{
	Super::BeginPlay();

	// 防御性保护：若意外在 Hall 关卡下运行了 BattleHUD，自动回退显示大厅 UI，避免用户看到战斗黑屏或错误界面
	if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("Hall")))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleHUD] Hall level detected under BattleHUD! Spawning HallMainWidget fallback."));
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->bShowMouseCursor = true;
			PC->bEnableClickEvents = true;
			PC->bEnableMouseOverEvents = true;

			if (UHallMainWidget* HallWidget = CreateWidget<UHallMainWidget>(PC))
			{
				HallWidget->AddToViewport(10);
			}
		}
		return;
	}

	TryInitWidget();
}

UBattleManager* ABattleHUD::GetBattleManager() const
{
	if (UWorld* World = GetWorld())
	{
		if (ABattleGameMode* GM = Cast<ABattleGameMode>(World->GetAuthGameMode()))
		{
			return GM->GetBattleManager();
		}
	}
	return nullptr;
}

void ABattleHUD::TryInitWidget()
{
	if (bWidgetInitialized && MainWidget)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	UBattleManager* Mgr = GetBattleManager();
	if (!PC || !Mgr)
	{
		return;
	}

	if (!MainWidget)
	{
		MainWidget = CreateWidget<UBattleMainWidget>(PC);
		if (MainWidget)
		{
			MainWidget->InitializeBattleUI(Mgr, Cast<ABattlePlayerController>(PC));
			MainWidget->AddToViewport(10);
		}
	}
	else
	{
		MainWidget->InitializeBattleUI(Mgr, Cast<ABattlePlayerController>(PC));
	}

	if (Mgr)
	{
		Mgr->OnBattleStatusMessage.RemoveAll(this);
		Mgr->OnBattleStatusMessage.AddDynamic(this, &ABattleHUD::AddBattleLog);
	}

	bWidgetInitialized = true;
}

void ABattleHUD::AddBattleLog(const FString& Message)
{
	RecentLogs.Add(Message);
	if (RecentLogs.Num() > 8)
	{
		RecentLogs.RemoveAt(0);
	}

	if (MainWidget)
	{
		MainWidget->PostNotification(Message);
	}
}

bool ABattleHUD::IsMouseOverUI() const
{
	if (MainWidget)
	{
		return MainWidget->IsMouseOverInteractiveUI();
	}
	return false;
}

bool ABattleHUD::HandleMouseClick(const FVector2D& MousePos)
{
	return IsMouseOverUI();
}

void ABattleHUD::DrawHUD()
{
	Super::DrawHUD();

	if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("Hall")))
	{
		return;
	}

	// 确保在世界与关卡准备完毕后挂载 UMG
	if (!bWidgetInitialized)
	{
		TryInitWidget();
	}
}
