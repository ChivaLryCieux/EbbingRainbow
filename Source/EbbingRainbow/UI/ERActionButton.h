// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "Layout/Margin.h"
#include "ERActionButton.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERActionClicked);

/**
 * 幽灵按钮 (发丝框 + Micro 文字 + 箭头):
 * 常态骨白发丝; 紧急态 (该点结束回合时) 描边转金并轻微呼吸。
 * 支持在 WBP 中直接调整样式/边框/文字/动画, 也可关闭程序化描边完全接管 UButton 外观。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERActionButton : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetLabel(const FText& InLabel);

	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetButtonEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetUrgent(bool bInUrgent);

	UPROPERTY(BlueprintAssignable, Category = "ERUI|Button")
	FOnERActionClicked OnActionClicked;

	/** 按钮主文案 (支持在父 WBP 实例面板上直接改字) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button", meta = (ExposeOnSpawn = "true"))
	FText Label = INVTEXT("结束回合");

	/** 右侧引导符号 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	FText ArrowSymbol = INVTEXT("→");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	bool bShowArrow = true;

	/** 按钮内边距 (驱动程序化描边时应用到 HitButton) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	FMargin ContentPadding = FMargin(18.0f, 10.0f);

	/** 为 true 时由 C++ 按状态驱动发丝框/文字着色; 若在 WBP 中手绘自定义按钮外观可设为 false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	bool bDriveStyleFromCode = true;

	/** 为 false 时自定义下方状态色, 不从 UERUITheme 取色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	bool bUseThemeColors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button", meta = (EditCondition = "!bUseThemeColors"))
	FLinearColor CustomNormalColor = FLinearColor(0.92f, 0.90f, 0.87f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button", meta = (EditCondition = "!bUseThemeColors"))
	FLinearColor CustomUrgentColor = FLinearColor(0.94f, 0.72f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button", meta = (EditCondition = "!bUseThemeColors"))
	FLinearColor CustomDisabledColor = FLinearColor(0.92f, 0.90f, 0.87f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	float UrgentPulseSpeed = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewUrgent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewEnabled = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Button", meta = (DisplayName = "On Button State Updated"))
	void BP_OnStateUpdated(bool bEnabled, bool bIsUrgent, bool bIsHovered);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HitButton;

	/** 可选独立边框图 (若 WBP 放置了 BorderImage, 程序化着色也会同步驱动它) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BorderImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ArrowText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Urgent;

private:
	void Build();
	void BindDelegates();
	void ApplyStyle();

	bool bUrgent = false;
	bool bEnabledState = true;
	bool bBuilt = false;
	bool bDelegatesBound = false;
	float UrgentPhase = 0.0f;
};
