// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "Components/Button.h"
#include "ERGalaCommandPanel.generated.h"

class UBorder;
class UCanvasPanel;
class UERActionButton;
class UERControlSharePie;
class UEditableTextBox;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGalaPanelClosed);

/**
 * 虹膜基金会银河军事指挥终端 (大厅内全屏终端面板 / 战场暂停设置弹窗):
 *  - 采用《控制:共振》式极简架构排印 + 深墨半透明底幕 + 1px 结构发丝线 + 纯白错位黑影汉字;
 *  - 页签 0: 接取委托 (三栏等宽建筑分栏, 无白底卡片);
 *  - 页签 1: 兵团整备 (极简账本式纵列与实时兵员数值);
 *  - 页签 2: 银河局势 (暗底高对比控制率环图 + 势力简报 + 重点星区归属 + 上一战况);
 *  - 页签 3: 终端设置 (AI 战术指挥接入配置)。
 */
UCLASS()
class EBBINGRAINBOW_API UERGalaCommandPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** bSettingsOnlyMode=true 时只显示设置页并隐藏页签导航, 供标题菜单与战场暂停「设置」使用;
	 *  保证委托/整备/银河局势只能由大厅控制台唤醒。 */
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
	struct FResonantTabItem
	{
		TObjectPtr<UButton> Button = nullptr;
		TObjectPtr<UTextBlock> Label = nullptr;
		TObjectPtr<UImage> Indicator = nullptr;
	};

	void RefreshCampaignSaveStatus();
	void BuildLayout();
	void RebuildFactionAndSectorList();
	void RebuildContractList();
	void RebuildRosterList();
	void SetActiveTab(int32 TabIndex);

	void OnRefreshContractsClicked();
	void OnReinforceAllClicked();

	UFUNCTION() void OnTabClicked0() { SetActiveTab(0); }
	UFUNCTION() void OnTabClicked1() { SetActiveTab(1); }
	UFUNCTION() void OnTabClicked2() { SetActiveTab(2); }
	UFUNCTION() void OnTabClicked3() { SetActiveTab(3); }

	/** 终端设置页: AI 战术指挥接入配置 (与战场 AI 面板共用 Saved/ER_AIConfig.json) */
	void SyncSettingsInputsFromService();
	void OnSaveAIConfigClicked();
	void StyleDarkInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword);

	/** 《控制:共振》式纯白字+错位黑影轻量按钮 */
	UERActionButton* MakeResonantButton(const FText& Label, int32 FontSize, bool bArrow, bool bLeftAlign, TFunction<void()> OnClick);

	/** UI 结构微元 */
	UImage* MakeHairlineH(float Alpha = 0.16f);
	UImage* MakeHairlineV(float Alpha = 0.14f);
	UWidget* MakeSectionKicker(const FText& Text, const FLinearColor& AccentCol);
	UWidget* MakeFactionSeal(EFactionId Faction, float StampSize);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CreditsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CycleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CampaignSaveNotice;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> CampaignSaveRetryButton;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> RefreshContractsButton;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> ReinforceAllButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> TerminalTabRow;
	TArray<FResonantTabItem> NavTabs;
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
