// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "Styling/SlateBrush.h"
#include "ERHallButton.generated.h"

class UBorder;
class UButton;
class UERTechFrame;
class USizeBox;
class UTextBlock;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERHallButtonClicked);

/**
 * 空间站大厅机能风切角主控模块按钮 (WBP_HallButton):
 *  - 黑-白-点睛彩建筑视觉:
 *      次按钮 = 哑光黑曜石底 + 纯白粗体 + 1px 发丝框 + 四角对准刻度
 *      主按钮 = 反白石膏底 + 极墨黑粗体 + 左侧 3px 《虹不我与》窄光谱点睛条
 *  - 二阶弹簧物理驱动悬停微移/按压回弹 + 斜向扫描高光 + 英文副标遥测译码
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

	/** 触发英文副标译码动效与一次扫描光 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|HallButton")
	void TriggerEntrancePulse(float DelaySeconds = 0.0f);

	UPROPERTY(BlueprintAssignable, Category = "ERUI|HallButton")
	FOnERHallButtonClicked OnButtonClicked;

	// ---- 可调参数 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FText IndexCode = INVTEXT("02");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FText Label = INVTEXT("航线管理");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FText Caption = INVTEXT("EXPEDITION ROUTES");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	FLinearColor AccentColor = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ExposeOnSpawn = "true"))
	bool bPrimary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	bool bApplyWidthOverride = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (EditCondition = "bApplyWidthOverride"))
	float ButtonWidth = 312.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float ButtonHeight = 74.0f;

	/** 保留旧属性以兼容已有蓝图序列化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float BodyCornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float GlowCornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	FLinearColor BaseGlassColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	FLinearColor HoverGlassColor = FLinearColor(0.022f, 0.024f, 0.030f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	FLinearColor OutlineColor = FLinearColor(0.92f, 0.92f, 0.90f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	bool bDriveGlassAnimationFromCode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float HoverInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float PressInterpSpeed = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton", meta = (ClampMin = "0.8", ClampMax = "1.0"))
	float PressedScale = 0.975f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HallButton")
	float BreathSpeed = 1.4f;

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
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERTechFrame> TechFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> GlowBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> BodyBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> LedBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> IndexText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CaptionText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ArrowText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Hover;

private:
	void Build();
	void BindDelegates();
	void SyncStaticProperties();
	void RefreshVisuals(float DeltaTime, bool bDesignPreview);

	FERFloatSpring HoverSpring = FERFloatSpring::Tactile(0.0f);
	FERFloatSpring PressSpring = FERFloatSpring::Snappy(0.0f);
	FERCipherReveal CaptionCipher;
	float SweepProgress = 1.0f;
	float ElapsedTime = 0.0f;
	bool bWasHovered = false;
	bool bBuilt = false;
	bool bDelegatesBound = false;
};
