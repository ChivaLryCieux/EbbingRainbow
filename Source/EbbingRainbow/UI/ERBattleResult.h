// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ERInteractiveWidget.h"
#include "ERBattleResult.generated.h"

class UCanvasPanel;
class UERActionButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERResultPrimary);

/** 结算明细行 (标签 + 值 + 值着色) */
USTRUCT(BlueprintType)
struct FERResultRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Result")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Result")
	FText Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Result")
	FLinearColor ValueTint = FLinearColor::White;
};

/**
 * 结算单行子控件 (WBP_BattleResultRow):
 * 供用户在蓝图中自由调整结算统计行的字号、对齐、间距与装饰线。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERBattleResultRow : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Result")
	void SetRowData(const FERResultRow& InRow);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewLabel = INVTEXT("存活");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewValue = INVTEXT("3 / 3");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FLinearColor PreviewValueTint = FLinearColor(0.92f, 0.90f, 0.87f, 1.0f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

private:
	void Build();
	bool bBuilt = false;
};

/**
 * 战斗结算: 上下 letterbox 黑条 + 超大词 + 明细行 + 幽灵返回按钮。
 * 全屏 HitTest, 结算期间点击不穿透。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERBattleResult : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Result")
	void SetResult(bool bWon, const FText& Title, const FText& Subtitle,
		const TArray<FERResultRow>& Rows, const FText& ButtonLabel);

	UPROPERTY(BlueprintAssignable, Category = "ERUI|Result")
	FOnERResultPrimary OnPrimaryClicked;

	// ---- 可调参数 ----

	/** 明细行蓝图类 (例如 WBP_BattleResultRow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERBattleResultRow> RowWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Result")
	float RowSpacing = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	bool bEnableEntranceAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float LetterboxSlideOffset = 20.0f;

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewVictory = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewVictoryTitle = INVTEXT("胜  利");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewDefeatTitle = INVTEXT("败  北");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewSubtitle = INVTEXT("敌方全灭 · 推演完成");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewButtonLabel = INVTEXT("返回航线图");

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Result", meta = (DisplayName = "On Result Shown"))
	void BP_OnResultShown(bool bPlayerWon);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override { return const_cast<UERBattleResult*>(this); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TopBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BottomBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TitleRule;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RowsBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERActionButton> PrimaryButton;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Intro;

private:
	void Build();
	void BindDelegates();
	void PopulateRows(const TArray<FERResultRow>& Rows);

	UFUNCTION()
	void HandlePrimaryClicked();

	float EntranceT = 1.0f;
	bool bBuilt = false;
	bool bDelegatesBound = false;
};
