// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HallHUD.generated.h"

class UERHallHUD;
class UERPersonnelPanel;
class UUserWidget;

/**
 * 空间站大厅 HUD:
 * 加载并呈现空间站主界面蓝图 (WBP_HallHUD, 在 Project Settings -> ER UI 指认), 支持观景模式切换。
 * 同时负责「人员管理」面板的创建 / 输入模式 / 关闭清理。
 */
UCLASS()
class EBBINGRAINBOW_API AHallHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHallHUD();

	virtual void BeginPlay() override;

	/** 切换观景模式 */
	void ToggleZenMode();

	UERHallHUD* GetHallWidget() const { return HallWidget; }

	/** 打开人员管理面板 (WBP 由 Project Settings -> ER UI 指认) */
	UFUNCTION(BlueprintCallable, Category = "Hall|Personnel")
	void OpenPersonnelPanel();

	/** 关闭人员管理面板 (播完退场动画后由面板广播真正关闭) */
	UFUNCTION(BlueprintCallable, Category = "Hall|Personnel")
	void ClosePersonnelPanel();

	UFUNCTION(BlueprintPure, Category = "Hall|Personnel")
	bool IsPersonnelPanelOpen() const;

	UFUNCTION(BlueprintPure, Category = "Hall|Personnel")
	UERPersonnelPanel* GetPersonnelPanel() const { return PersonnelPanel; }

private:
	UFUNCTION()
	void HandlePersonnelPanelClosed();

	/** 面板打开时强制释放光标; 关闭时恢复到打开前的输入状态 */
	void ApplyPanelInputMode(bool bPanelOpen);

	UPROPERTY(Transient)
	TObjectPtr<UERHallHUD> HallWidget;

	UPROPERTY(Transient)
	TObjectPtr<UERPersonnelPanel> PersonnelPanel;

	/** 打开面板前的光标状态 (关闭时精确还原) */
	bool bCursorWasVisibleBeforePanel = true;
};
