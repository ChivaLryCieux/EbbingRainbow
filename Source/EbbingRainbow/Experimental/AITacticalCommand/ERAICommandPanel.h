// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "Experimental/AITacticalCommand/UBattleAICommandService.h"
#include "ERAICommandPanel.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UERActionButton;
class UERTechDivider;
class UERTechFrame;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERAIPanelCloseRequested);

/**
 * [实验性毕设模块] 战斗 HUD 内嵌 AI 战术指挥终端 (PRTS 机能风 × 控制:共振平面重构):
 *  - 通过 InstallOnBattleHUD() 自动挂载右上角“AI 链路”切角按钮与战术终端面板
 *  - 黑曜石 45° 切角巨构底板 (UERTechFrame) + 左侧虹彩窄光谱条 + 四角光学刻度
 *  - 顶部《控制:共振》式粗细巨幅对比标题 + 遥测译码 + 反白状态胶囊
 *  - 结构化标签战报日志流 (CMD / TOOL / RET / AI / ERR / SYS)
 *  - 反白石膏高对比主执行按钮 + 折叠 API 配置抽屉 + 展开动效
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERAICommandPanel : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	static UERAICommandPanel* InstallOnBattleHUD(UUserWidget* OwnerHUD, UCanvasPanel* RootCanvas, float ScreenMargin);
	/** 挂载到大兵团等高线地图视角左上角 (「接入 AI」按钮 + 左侧展开终端) */
	static UERAICommandPanel* InstallOnLegionMapHUD(UUserWidget* OwnerHUD, UCanvasPanel* RootCanvas, float ScreenMargin);
	static bool IsAnyTextInputFocused();
	/** 任一 AI 面板或其开关按钮当前被鼠标悬停或拥有输入焦点 (用于拦截地图拖拽/缩放/点选) */
	static bool IsAnyPanelHoveredOrFocused();

	void BindService(UBattleAICommandService* InService);
	void SetHostVisible(bool bVisible);
	void SetPanelOpen(bool bOpen);
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
	UButton* MakeTechButton(const FString& Label, bool bInvertedPrimary, const FLinearColor& FillTint,
		const FLinearColor& StrokeTint, const FLinearColor& TextColor, TObjectPtr<UTextBlock>* OutLabel = nullptr,
		TObjectPtr<UBorder>* OutBg = nullptr);
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
	UPROPERTY(Transient) TObjectPtr<UERTechFrame> PanelFrame;
	UPROPERTY(Transient) TObjectPtr<UERTechDivider> HeaderDivider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HeaderCaptionText;
	UPROPERTY(Transient) TObjectPtr<UBorder> StatusBadgeBorder;
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
	UPROPERTY(Transient) TObjectPtr<UBorder> SendButtonBg;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SendButtonText;
	UPROPERTY(Transient) TObjectPtr<UButton> NewContextButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ClearLogButton;

	FERRevealTrack OpenTrack;
	FERCipherReveal CaptionCipher;
	bool bBuilt = false;
	bool bSettingsExpanded = false;
	bool bLegionModeUI = false;
	float IndexRefreshAccum = 0.0f;
};
