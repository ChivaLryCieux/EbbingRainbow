// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LegionHUDWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;

/**
 * 虹膜军团战术终端 HUD:
 *  - 左上: 虹彩条 + 委托/演武标题 + 敌我势力子标;
 *  - 右上: 编制与待降增援波次 + 流速控制 (常速 / 0.2x时缓 / 暂停);
 *  - 底部: 兵团控制牌 (显示兵种/兵力/状态/协议冷却) + 主动协议触发按钮;
 *  - 战终结算窗: 展示经费收益、战损、星区控制权变迁与「返回虹膜基金会大厅」按钮。
 */
UCLASS()
class EBBINGRAINBOW_API ULegionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Build();
	void RefreshStatus();
	void RefreshTimeControls();
	void RefreshLegionBar();
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

	UFUNCTION() void OnSlowMoClicked();
	UFUNCTION() void OnPauseClicked();
	UFUNCTION() void OnSkillClicked();
	UFUNCTION() void OnReturnHallClicked();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusAllyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusEnemyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ParamsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient) TObjectPtr<UButton> SlowMoButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SlowMoLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> PauseButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PauseLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> SkillButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SkillLabel;

	UPROPERTY(Transient) TObjectPtr<UBorder> ResultBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultSummaryText;
	UPROPERTY(Transient) TObjectPtr<UButton> ReturnHallButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultSubText;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootPanel;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> LegionBar;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> LegionPlates;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LegionNameTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LegionCountTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LegionStateTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LegionSkillTexts;

	bool bBuilt = false;
	float PollTimer = 0.0f;
	int32 CachedAllyAlive = -1;
	int32 CachedEnemyAlive = -1;
	int32 CachedPendingWaves = -1;
	bool bResultShown = false;
};
