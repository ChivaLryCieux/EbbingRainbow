// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ERGalaCommandPanel.generated.h"

class UBorder;
class UCanvasPanel;
class UERActionButton;
class UEditableTextBox;
class UHorizontalBox;
class UERControlSharePie;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGalaPanelClosed);

/**
 * 虹膜基金会银河军事指挥终端 (大厅内全屏终端面板):
 *  - 展示基金会口号「我们面向全银河提供军事指挥服务」;
 *  - 页签由大厅左部四按钮入口直达 (面板内不设导航);
 *  - 页签 0: 委托合同接取/出击部署 (三卡片从左至右排布);
 *  - 页签 1: 常备虹膜链接兵团编制查看、补员、AI 同步升级与新兵团招募;
 *  - 页签 2: 三大阵营战局控制度、星区态势与上一战况;
 *  - 页签 3: 终端设置 (占位)。
 */
UCLASS()
class EBBINGRAINBOW_API UERGalaCommandPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** bSettingsOnlyMode=true 时只显示设置页并隐藏页签导航, 供标题菜单「设置」使用;
	 *  这样才能保证委托/整备/银河局势只能由大厅控制台唤醒。 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Galactic")
	void OpenPanel(int32 InitialTab = 0, bool bSettingsOnlyMode = false);

	UFUNCTION(BlueprintCallable, Category = "ERUI|Galactic")
	void ClosePanel();

	UFUNCTION(BlueprintPure, Category = "ERUI|Galactic")
	bool IsPanelOpen() const { return bIsOpen; }

	void RefreshAll();

	UPROPERTY(BlueprintAssignable, Category = "ERUI|Galactic")
	FOnGalaPanelClosed OnPanelClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void RefreshCampaignSaveStatus();
	void BuildLayout();
	void RebuildFactionAndSectorList();
	void RebuildContractList();
	void RebuildRosterList();
	void SetActiveTab(int32 TabIndex);

	void OnRefreshContractsClicked();
	void OnReinforceAllClicked();

	/** 终端设置页: AI 战术指挥接入配置 (与战场 AI 面板共用 Saved/ER_AIConfig.json) */
	void SyncSettingsInputsFromService();
	void OnSaveAIConfigClicked();
	void StyleBoneInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword);

	/** 「白底墨字彩条」按钮 (只含中文); OnClick 用 lambda 承载载荷 */
	UERActionButton* MakeBoneButton(const FText& Label, int32 FontSize, bool bArrow, TFunction<void()> OnClick);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CreditsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CycleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CampaignSaveNotice;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> CampaignSaveRetryButton;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> RefreshContractsButton;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> ReinforceAllButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> TerminalTabRow;
	UPROPERTY(Transient) TArray<TObjectPtr<UERActionButton>> TabButtons;
	UPROPERTY(Transient) TObjectPtr<UWidget> ContractsPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> RosterPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> SituationPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> SettingsPage;

	UPROPERTY(Transient) TObjectPtr<UERControlSharePie> ControlSharePie;
	UPROPERTY(Transient) TObjectPtr<UWidget> ControlShareCard;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> FactionListBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> SectorListBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> LastOutcomeBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LastOutcomeText;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ContractListBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RosterListBox;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> RecruitBarBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RosterSummaryText;

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> SettingsEndpointInput;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> SettingsModelInput;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> SettingsApiKeyInput;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SettingsStatusText;

	bool bBuilt = false;
	bool bIsOpen = false;
	bool bSettingsOnlyMode = false;
	int32 ActiveTab = 0;
	/** 出击部署已点击、等待下一帧 OpenLevel: 防止卡顿期间重复点击触发多次部署 */
	bool bDeployInTransit = false;
};
