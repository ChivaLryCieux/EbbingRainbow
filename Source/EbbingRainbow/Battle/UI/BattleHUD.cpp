// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/BattleHUD.h"
#include "Battle/UI/BattleMainWidget.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Engine/Canvas.h"

ABattleHUD::ABattleHUD()
{
}

void ABattleHUD::BeginPlay()
{
	Super::BeginPlay();
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

	// 确保在世界与关卡准备完毕后挂载 UMG
	if (!bWidgetInitialized)
	{
		TryInitWidget();
	}
}
