// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "Layout/Margin.h"
#include "ERActionButton.generated.h"

class UButton;
class UERTechFrame;
class UHorizontalBox;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERActionClicked);

/**
 * 全工程通用按钮 —— 「白底墨字彩条」样式 (源自大厅「银河委托」):
 *  反白石膏底 + 极墨黑粗体中文 + 左侧虹彩/点睛窄条 + 45° 切角 + 四角对准刻度。
 *  只允许中文文案, 字号默认 22 (可按需加大); 悬停提亮 + 扫描高光, 按压回弹。
 * 紧急态 (该点结束回合 / 当前激活) 描边加粗并轻微呼吸。
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

	/** 点睛条颜色; bPrism=true 时改用《虹不我与》三段光谱条 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetAccentColor(FLinearColor InAccent, bool bPrism);

	/** 字号 (中文粗体), 同步按字号重算内边距 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetFontSize(int32 InFontSize);

	/** 最小尺寸 (0 = 随内容) */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetMinSize(float InMinWidth, float InMinHeight);

	/** 切换为《控制:共振》式纯白排印 + 黑色错位阴影 HUD 风格 (无白底卡片) */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Button")
	void SetResonantHudStyle(bool bInResonant, bool bInLeftAlign = false);

	/** 原生回调 (带载荷的动态列表按钮用 lambda, 免去 OnClicked0..N 样板函数) */
	void SetNativeClick(TFunction<void()> InCallback) { NativeCallback = MoveTemp(InCallback); }

	/** 在 Owner 的控件树里直接构造一个「白底墨字彩条」按钮 */
	static UERActionButton* Make(UUserWidget* Owner, const FText& InLabel, int32 InFontSize = 22,
		bool bInShowArrow = false, bool bInPrism = true);

	UPROPERTY(BlueprintAssignable, Category = "ERUI|Button")
	FOnERActionClicked OnActionClicked;

	/** 中文粗体字号 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	int32 FontSize = 22;

	/** 《控制:共振》式纯白字+错位黑影 HUD 模式 (无白底卡片) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	bool bResonantHudStyle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button", meta = (EditCondition = "bResonantHudStyle"))
	bool bResonantLeftAlign = false;

	/** 点睛条是否用三段光谱条 (否则用 AccentColor 单色) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	bool bPrismAccent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	FLinearColor AccentColor = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	float MinButtonWidth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Button")
	float MinButtonHeight = 0.0f;

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

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ContentRow;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> ResonantBarContainer;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ResonantBarShadow;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ResonantBar;

	/** 机能风切角底板 (白底墨字彩条) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERTechFrame> ButtonFrame;

	/** 可选独立边框图 (兼容旧蓝图) */
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
	void ApplyStyle(float DeltaTime = 0.0f);

	TFunction<void()> NativeCallback;
	bool bUrgent = false;
	bool bEnabledState = true;
	bool bWasHovered = false;
	bool bBuilt = false;
	bool bDelegatesBound = false;
	float UrgentPhase = 0.0f;
	float SweepProgress = 1.0f;
	FERFloatSpring HoverSpring = FERFloatSpring::Tactile(0.0f);
	FERFloatSpring PressSpring = FERFloatSpring::Snappy(0.0f);
};
