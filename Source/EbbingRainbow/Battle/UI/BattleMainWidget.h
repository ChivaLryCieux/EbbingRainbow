// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleMainWidget.generated.h"

class UBattleManager;
class ABattlePlayerController;
class SBattleMainOverlay;

/**
 * 现代战术主界面 UMG 控件:
 * 承载 Slate 高性能现代 UI (先攻队列条、尖塔手牌区、英雄属性板、结束回合按钮)
 */
UCLASS()
class EBBINGRAINBOW_API UBattleMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeBattleUI(UBattleManager* InManager, ABattlePlayerController* InPC);

	/** 显示动态战报通知 */
	UFUNCTION(BlueprintCallable, Category = "Battle UI")
	void PostNotification(const FString& Message);

	/** 检查鼠标当前是否正悬停在交互性 UI (卡牌/按钮) 上, 防止场景点地穿透 */
	bool IsMouseOverInteractiveUI() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SBattleMainOverlay> SlateOverlay;
	TWeakObjectPtr<UBattleManager> BattleManager;
	TWeakObjectPtr<ABattlePlayerController> BattlePC;
};
