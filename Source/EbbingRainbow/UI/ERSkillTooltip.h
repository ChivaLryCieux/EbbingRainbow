// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERSkillTooltip.generated.h"

class UERTechDivider;
class UERTechFrame;
class UImage;
class UTextBlock;

/**
 * 技能/移动卡悬停提示面板 (WBP_SkillTooltip):
 * 可单独在蓝图中调整背景、内边距、标题/描述/元信息排版及颜色。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERSkillTooltip : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Tooltip")
	void SetTooltipContent(const FText& InTitle, const FText& InDescription, const FText& InMeta, FLinearColor InAccent);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewTitle = INVTEXT("相位斩击");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewDescription = INVTEXT("突进劈砍单体目标，对无掩体目标暴击率提升。");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewMeta = INVTEXT("射 300 · 伤 35 · 2 AP");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FLinearColor PreviewAccent = FLinearColor(0.94f, 0.72f, 0.30f, 1.0f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERTechFrame> TooltipFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERTechDivider> TooltipDivider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TooltipBg;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> AccentRule;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MetaText;

private:
	void Build();
	bool bBuilt = false;
};
