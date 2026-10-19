// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Battle/Core/BattleTypes.h"
#include "BattleHUD.generated.h"

class UBattleManager;
class ABattleUnit;
class UBattleMainWidget;

/**
 * 战术战斗 HUD:
 * 管理并展示现代 UMG 战斗主界面 (先攻队列、尖塔手牌、英雄属性、结束回合按钮)。
 * 兼具战况日志收集与输入阻挡判定。
 */
UCLASS()
class EBBINGRAINBOW_API ABattleHUD : public AHUD
{
	GENERATED_BODY()

public:
	ABattleHUD();

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	/** 处理鼠标点击 UI。若点中了 UI 控件返回 true, 阻止穿透到 3D 场景 */
	bool HandleMouseClick(const FVector2D& MousePos);

	/** 检查鼠标当前是否正悬停在交互性 UI (卡牌/按钮) 上 */
	bool IsMouseOverUI() const;

	/** 添加屏幕状态日志 */
	UFUNCTION()
	void AddBattleLog(const FString& Message);

	UBattleMainWidget* GetMainWidget() const { return MainWidget; }

private:
	UBattleManager* GetBattleManager() const;

	UPROPERTY(Transient)
	TObjectPtr<UBattleMainWidget> MainWidget;

	TArray<FString> RecentLogs;
	bool bWidgetInitialized = false;

	void TryInitWidget();
};
