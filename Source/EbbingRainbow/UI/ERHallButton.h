// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "Styling/SlateBrush.h"
#include "ERHallButton.generated.h"

class UBorder;
class UButton;
class USizeBox;
class UTextBlock;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERHallButtonClicked);

/**
 * 空间站大厅液态玻璃胶囊按钮 (WBP_HallButton):
 *  - 烟熏半透明圆角胶囊底 + 发丝描边 + 状态呼吸灯 + 主/副双层排版
 *  - 全部节点均为标准 UMG 控件, 可在 Designer 中自由拖拽、改圆角、换材质或挂动画轨
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERHallButton : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|HallButton")
	void SetButtonTexts(const FText& InLabel, const FText& InCaption);

	UFUNCTION(BlueprintCallable, Category = "ERUI|HallButton")
	void SetAccentAndPrimary(FLinearColor InAccent, bool bInPrimary);

	UPROPERTY(BlueprintAssignable, Category = "ERUI|HallButton")
	FOnERHallButtonClicked OnButtonClicked;

	// ---- 可在实例面板或 Class Defaults 直接手调的参数 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FText Label = INVTEXT("航线管理");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FText Caption = INVTEXT("R O U T E S");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FLinearColor AccentColor = FLinearColor(0.32f, 0.76f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	bool bPrimary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	bool bApplyWidthOverride = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (EditCondition = "bApplyWidthOverride"))
	float ButtonWidth = 264.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float BodyCornerRadius = 27.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float GlowCornerRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	FLinearColor BaseGlassColor = FLinearColor(0.05f, 0.075f, 0.11f, 0.44f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	FLinearColor HoverGlassColor = FLinearColor(0.09f, 0.135f, 0.20f, 0.60f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	FLinearColor OutlineColor = FLinearColor(0.90f, 0.95f, 1.0f, 1.0f);

	/** 是否由代码每帧驱动呼吸光晕/玻璃提亮/回弹缩放; 关掉后可完全由蓝图材质与动画控制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	bool bDriveGlassAnimationFromCode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float HoverInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float PressInterpSpeed = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ClampMin = "0.8", ClampMax = "1.0"))
	float PressedScale = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float BreathSpeed = 1.6f;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|HallButton", meta = (DisplayName = "On Hall Button Hover Changed"))
	void BP_OnHoverChanged(bool bIsHovered);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> HitButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> ButtonSizeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> GlowBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> BodyBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> LedBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CaptionText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Hover;

private:
	void Build();
	void BindDelegates();
	void SyncStaticProperties();
	void RefreshVisuals(float DeltaTime, bool bDesignPreview);

	FSlateBrush MakeRoundedBrush(float Radius, FLinearColor Fill, float OutlineWidth, FLinearColor Outline) const;

	float HoverAlpha = 0.0f;
	float PressAlpha = 0.0f;
	float ElapsedTime = 0.0f;
	bool bBuilt = false;
	bool bDelegatesBound = false;
};
