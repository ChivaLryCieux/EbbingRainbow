// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "Experimental/AITacticalCommand/UBattleAICommandService.h"
#include "ERAICommandPanel.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UERActionButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERAIPanelCloseRequested);

/**
 * [实验性毕设模块] 战斗 HUD 内嵌 AI 战术指挥面板:
 *  - 通过 InstallOnBattleHUD() 自动挂载右上角“AI 连接”按钮与折叠面板, 零侵入 WBP_BattleHUD
 *  - 可折叠 API 配置栏 (Endpoint / Model / Key / 预设)
 *  - 实时棋子编号速查条 (P1..Pn / E1..En / 当前回合 / 上下文轮数)
 *  - 战术播报与工具调用日志流
 *  - 自然语言指令输入框 + 执行/中止/新对话按钮
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERAICommandPanel : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	/** 在战斗 HUD 上挂载“AI 连接”按钮与面板 (幂等, 已挂载则复用) */
	static UERAICommandPanel* InstallOnBattleHUD(UUserWidget* OwnerHUD, UCanvasPanel* RootCanvas, float ScreenMargin);

	/** 当前是否有任意 AI 面板输入框持有键盘焦点 */
	static bool IsAnyTextInputFocused();

	void BindService(UBattleAICommandService* InService);
	void OnPanelOpened();
	bool IsTextInputFocused() const;

	virtual bool IsInteractiveHovered() const override;
	virtual UWidget* GetHoverCheckWidget() const override;

	UPROPERTY(BlueprintAssignable, Category = "ERUI|AI")
	FOnERAIPanelCloseRequested OnCloseRequested;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	UButton* MakeMiniButton(const FString& Label, const FLinearColor& StrokeTint, const FLinearColor& TextColor, TObjectPtr<UTextBlock>* OutLabel = nullptr);
	void StyleInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword = false);

	void SyncConfigFieldsFromService();
	void RefreshUnitIndexBar();
	void UpdateBusyUI(bool bBusy, const FString& StatusLabel);
	void AppendLogLine(EERAICommandLogKind Kind, const FString& Text);

	UFUNCTION() void HandleToggleButtonClicked();
	UFUNCTION() void HandleToggleSettingsClicked();
	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleSaveConfigClicked();
	UFUNCTION() void HandlePresetDeepSeekClicked();
	UFUNCTION() void HandlePresetOpenAIClicked();
	UFUNCTION() void HandlePresetQwenClicked();
	UFUNCTION() void HandlePresetOllamaClicked();
	UFUNCTION() void HandleSendClicked();
	UFUNCTION() void HandleNewContextClicked();
	UFUNCTION() void HandleClearLogClicked();
	UFUNCTION() void HandleCommandCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void HandleServiceLog(EERAICommandLogKind Kind, const FString& Text);
	void HandleServiceStateChanged(bool bBusy, const FString& StatusLabel);

	static TArray<TWeakObjectPtr<UERAICommandPanel>>& ActivePanels();

	TWeakObjectPtr<UBattleAICommandService> Service;
	TWeakObjectPtr<UERActionButton> LinkedToggleButton;
	FDelegateHandle LogDelegateHandle;
	FDelegateHandle StateDelegateHandle;

	UPROPERTY(Transient) TObjectPtr<UBorder> RootBorder;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusBadgeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SettingsToggleText;
	UPROPERTY(Transient) TObjectPtr<UButton> SettingsToggleButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> SettingsDrawer;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> EndpointInput;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ModelInput;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ApiKeyInput;
	UPROPERTY(Transient) TObjectPtr<UButton> SaveConfigButton;
	UPROPERTY(Transient) TObjectPtr<UButton> PresetDeepSeekBtn;
	UPROPERTY(Transient) TObjectPtr<UButton> PresetOpenAIBtn;
	UPROPERTY(Transient) TObjectPtr<UButton> PresetQwenBtn;
	UPROPERTY(Transient) TObjectPtr<UButton> PresetOllamaBtn;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> UnitIndexText;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> LogScrollBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> CommandInput;
	UPROPERTY(Transient) TObjectPtr<UButton> SendButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SendButtonText;
	UPROPERTY(Transient) TObjectPtr<UButton> NewContextButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ClearLogButton;

	bool bBuilt = false;
	bool bSettingsExpanded = false;
	float IndexRefreshAccum = 0.0f;
};
