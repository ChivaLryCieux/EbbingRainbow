// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERUnitLabelWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;

/**
 * 战棋单位头顶铭牌 UMG 控件:
 * 由 ABattleUnit 的屏幕空间 WidgetComponent 承载, 使用复合字体 F_ERUI_Composite,
 * 解决 UTextRenderComponent 默认离线位图字体无法渲染中文 (显示方框) 的问题。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERUnitLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLabelText(const FText& InText);
	void SetLabelColor(const FLinearColor& InColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void Build();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackdropBorder;

	UPROPERTY(Transient)
	TObjectPtr<UImage> AccentBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	FText CachedText;
	FLinearColor CachedColor = FLinearColor(0.92f, 0.90f, 0.87f, 1.0f);
	bool bBuilt = false;
};
