// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LegionHUDWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UERActionButton;
class UERAICommandPanel;
class UERTechFrame;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

/**
 * 虹膜军团战术终端 HUD:
 *  - 左上: 虹彩条 + 委托/演武标题 + 敌我势力子标;
 *  - 左下: 敌我旅数与兵员总览;
 *  - 底部居中: 部队卡片 (仅显示 部队名称 + Logo + 兵员数量 三要素);
 *  - 战终结算窗: 展示经费收益、战损、星区控制权变迁与「返回虹膜基金会大厅」按钮。
 */
UCLASS()
class EBBINGRAINBOW_API ULegionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 等高线地图视角开关时显隐底部部队卡片与左上「接入 AI」终端 (地图上已直接显示番号与 Logo) */
	void SetStrategicMapMode(bool bMapOpen);

	/** 暂停菜单开关时隐藏全部战场常驻 HUD */
	void SetPauseMenuMode(bool bPaused);

	UERAICommandPanel* GetAICommandPanel() const { return AICommandPanel.Get(); }

	bool IsResultOverlayVisible() const;
	bool IsReturnHallButtonReady() const;
	void ClickReturnHallForTest() { OnReturnHallClicked(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Build();
	void UpdateHUDVisibility();
	void RefreshStatus();
	void RefreshLegionBar();
	void RefreshPlateHover();
	void HandleBattleEnded(bool bPlayerWon);
	void ShowResult(bool bPlayerWon);

	void BuildLegionBar(int32 Count);
	void SelectPlate(int32 Index);

	UFUNCTION() void OnPlateClicked0() { SelectPlate(0); }
	UFUNCTION() void OnPlateClicked1() { SelectPlate(1); }
	UFUNCTION() void OnPlateClicked2() { SelectPlate(2); }
	UFUNCTION() void OnPlateClicked3() { SelectPlate(3); }
	UFUNCTION() void OnPlateClicked4() { SelectPlate(4); }
	UFUNCTION() void OnPlateClicked5() { SelectPlate(5); }
	UFUNCTION() void OnPlateClicked6() { SelectPlate(6); }
	UFUNCTION() void OnPlateClicked7() { SelectPlate(7); }

	UFUNCTION() void OnReturnHallClicked();

	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> TitleRow;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> StatusCol;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> FactionLogoRow;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusAllyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusEnemyText;

	UPROPERTY(Transient) TObjectPtr<UBorder> ResultOverlay;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UButton> ReturnHallButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReturnHallLabel;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootPanel;
	UPROPERTY(Transient) TObjectPtr<UERAICommandPanel> AICommandPanel;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> LegionBar;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> LegionPlateButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> LegionPlateBars;
	TArray<bool> LegionPlateSelected;
	TArray<bool> LegionPlateDead;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LegionNameTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> LegionLogoImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LegionCountTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> CachedCardLogos;

	bool bBuilt = false;
	float PollTimer = 0.0f;
	int32 CachedAllyAlive = -1;
	int32 CachedEnemyAlive = -1;
	int32 CachedPendingWaves = -1;
	bool bResultShown = false;
	bool bPauseMenuOpen = false;
	bool bMapOpenCached = false;
	bool bReturningToHall = false;
	FDelegateHandle BattleEndHandle;
};
