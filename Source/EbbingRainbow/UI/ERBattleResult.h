// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "ERBattleResult.generated.h"

class UCanvasPanel;
class UERActionButton;
class UImage;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERResultPrimary);

/** 结算明细行 (标签 + 值 + 值着色) */
USTRUCT(BlueprintType)
struct FERResultRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FText Label;

	UPROPERTY(BlueprintReadWrite)
	FText Value;

	UPROPERTY(BlueprintReadWrite)
	FLinearColor ValueTint = FLinearColor::White;
};

/**
 * 战斗结算: 上下 letterbox 黑条 + 超大词 + 明细行 + 幽灵返回按钮。
 * 全屏 HitTest, 结算期间点击不穿透。
 */
UCLASS()
class EBBINGRAINBOW_API UERBattleResult : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	void SetResult(bool bWon, const FText& Title, const FText& Subtitle,
		const TArray<FERResultRow>& Rows, const FText& ButtonLabel);

	UPROPERTY(BlueprintAssignable, Category = "ERUI")
	FOnERResultPrimary OnPrimaryClicked;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override { return const_cast<UERBattleResult*>(this); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void Build();

	UFUNCTION()
	void HandlePrimaryClicked();

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

	float EntranceT = 0.0f;
	bool bBuilt = false;
};
