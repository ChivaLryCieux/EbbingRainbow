// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HallPlayerController.generated.h"

class AHallConsoleActor;
class UInputMappingContext;

/**
 * 空间站大厅玩家控制器:
 *  - 启动时保持标题菜单 (UI-only + HallCameraPawn 背景机位);
 *  - 「继续/新游戏/读档」后切入第一人称备战大厅, 并挂载与 Lvl_Legion 相同的
 *    IMC_Default + IMC_MouseLook Enhanced Input 上下文;
 *  - 扫描玩家面前的 AHallConsoleActor, 按 E 唤醒银河指挥终端;
 *  - 第一人称中 Esc 返回标题菜单。
 */
UCLASS()
class EBBINGRAINBOW_API AHallPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHallPlayerController();

	UFUNCTION(BlueprintPure, Category = "Hall")
	bool IsFirstPersonActive() const { return bFirstPersonActive; }
	bool IsAnyHallPanelOpen() const;

	/** 从标题菜单进入第一人称大厅 (相机混合回 Pawn, 隐藏主菜单, 启用移动/视角输入) */
	UFUNCTION(BlueprintCallable, Category = "Hall")
	void EnterFirstPerson();

	/** 第一人称中按 Esc 返回标题菜单; 不重置战役内存状态 */
	UFUNCTION(BlueprintCallable, Category = "Hall")
	void ReturnToTitleMenu();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void OnKeyEscape();
	void OnKeyInteract();
	void OnLeftClick();

	void ApplyTitleInputMode();
	void ApplyFirstPersonInputMode();
	void ApplyMenuCamera(bool bBlend);
	void UpdateInputContextState();
	void SetInputContextsActive(bool bActive);
	void ResetFirstPersonInputState();
	void UpdateInteractionFocus(float DeltaTime);
	void ClearInteractionFocus();

	bool bFirstPersonActive = false;
	bool bInputContextsActive = false;
	float InteractionScanTimer = 0.0f;

	TWeakObjectPtr<AHallConsoleActor> FocusedConsole;

	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;
	TSoftObjectPtr<UInputMappingContext> MouseLookMappingContext;
};
