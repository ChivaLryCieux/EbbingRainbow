// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/BattleMainWidget.h"
#include "Battle/UI/SBattleMainOverlay.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Player/BattlePlayerController.h"

void UBattleMainWidget::InitializeBattleUI(UBattleManager* InManager, ABattlePlayerController* InPC)
{
	BattleManager = InManager;
	BattlePC = InPC;

	if (SlateOverlay.IsValid())
	{
		// 重建控件
		RebuildWidget();
	}
}

TSharedRef<SWidget> UBattleMainWidget::RebuildWidget()
{
	SlateOverlay = SNew(SBattleMainOverlay)
		.BattleManager(BattleManager)
		.PlayerController(BattlePC);

	return SlateOverlay.ToSharedRef();
}

void UBattleMainWidget::PostNotification(const FString& Message)
{
	if (SlateOverlay.IsValid())
	{
		SlateOverlay->ShowNotification(Message);
	}
}

bool UBattleMainWidget::IsMouseOverInteractiveUI() const
{
	if (SlateOverlay.IsValid())
	{
		return SlateOverlay->IsMouseOverInteractiveUI();
	}
	return false;
}
