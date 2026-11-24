// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ERGalaCommandPanel.generated.h"

class UBorder;
class UCanvasPanel;
class UERActionButton;
class UHorizontalBox;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGalaPanelClosed);

/**
 * 虹膜基金会银河军事指挥终端 (大厅内全屏终端面板):
 *  - 展示基金会口号「我们面向全银河提供军事指挥服务」;
 *  - 页签 0: 三大阵营战局控制度、星区态势与委托合同接取/出击部署;
 *  - 页签 1: 常备虹膜链接兵团编制查看、补员、AI 同步升级与新兵团招募。
 */
UCLASS()
class EBBINGRAINBOW_API UERGalaCommandPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Galactic")
	void OpenPanel(int32 InitialTab = 0);

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
	void BuildLayout();
	void RebuildFactionAndSectorList();
	void RebuildContractList();
	void RebuildRosterList();
	void SetActiveTab(int32 TabIndex);

	void OnRefreshContractsClicked();
	void OnReinforceAllClicked();

	/** 「白底墨字彩条」按钮 (只含中文); OnClick 用 lambda 承载载荷 */
	UERActionButton* MakeBoneButton(const FText& Label, int32 FontSize, bool bArrow, TFunction<void()> OnClick);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CreditsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CycleText;
	UPROPERTY(Transient) TObjectPtr<UWidget> ContractsPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> RosterPage;

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> FactionListBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> SectorListBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> LastOutcomeBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LastOutcomeText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ContractListBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RosterListBox;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> RecruitBarBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RosterSummaryText;

	bool bBuilt = false;
	bool bIsOpen = false;
	int32 ActiveTab = 0;
};
