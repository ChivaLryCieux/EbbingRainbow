// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "ERSkillCard.generated.h"

class UBorder;
class UButton;
class UERTechFrame;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnERSkillCardClicked, int32, CardIndex, bool, bIsMoveCard);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnERSkillCardHover, UERSkillCard*, Card, bool, bHovered);

/**
 * 战术手牌 (平面风竖条卡):
 *   [大号费用数字]        [快捷键]
 *   [   图标 / 字形   ]
 *   [技能主题色底线]
 *   悬停/选中上浮 + 金线; 不可用整体淡出; 冷却盖右下角数字。
 *   所有可选控件走 BindWidgetOptional: WBP 删减任意文本块或新增边框均可正常工作。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERSkillCard : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|SkillCard")
	void Setup(int32 InIndex, const FText& Name, const FText& Tooltip, const FText& CooldownLabel,
		const FText& Hotkey, int32 APCost, bool bIsMoveCard, FLinearColor InAccent, UTexture2D* Icon);

	UFUNCTION(BlueprintCallable, Category = "ERUI|SkillCard")
	void SetAffordable(bool bInAffordable);

	UFUNCTION(BlueprintCallable, Category = "ERUI|SkillCard")
	void SetSelectedMode(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "ERUI|SkillCard")
	bool IsHoveredState() const { return bHovered; }

	UFUNCTION(BlueprintPure, Category = "ERUI|SkillCard")
	FText GetCardTooltip() const { return CardTooltip; }

	UFUNCTION(BlueprintPure, Category = "ERUI|SkillCard")
	FText GetCardDescription() const { return CardDescription; }

	UFUNCTION(BlueprintPure, Category = "ERUI|SkillCard")
	FText GetCardName() const { return CardName; }

	UFUNCTION(BlueprintPure, Category = "ERUI|SkillCard")
	FText GetCardMeta() const { return CardMeta; }

	UFUNCTION(BlueprintPure, Category = "ERUI|SkillCard")
	FLinearColor GetCardAccent() const { return Accent; }

	void SetCardMeta(const FText& InMeta) { CardMeta = InMeta; }
	void SetCardDescription(const FText& InDesc) { CardDescription = InDesc; }

	UPROPERTY(BlueprintAssignable, Category = "ERUI|SkillCard")
	FOnERSkillCardClicked OnCardClicked;

	UPROPERTY(BlueprintAssignable, Category = "ERUI|SkillCard")
	FOnERSkillCardHover OnCardHover;

	// ---- 可调动效参数 ----

	/** 启用代码上浮/缩放过渡 (若改用 UMG 动画轨 Anim_Hover 可关闭) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|SkillCard")
	bool bEnableProceduralHoverMotion = true;

	/** 悬停/选中时的纵向位移 (负值向上浮起) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|SkillCard")
	float HoverLiftY = -10.0f;

	/** 悬停/选中时的等比缩放倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|SkillCard")
	float HoverScale = 1.05f;

	/** AP 不足或冷却时的卡片不透明度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|SkillCard", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float UnaffordableOpacity = 0.35f;

	/** 插值速度覆盖 (< 0 时取 UERUITheme::HoverInterpSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|SkillCard")
	float HoverInterpSpeedOverride = -1.0f;

	/** 移动卡费用文案 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|SkillCard")
	FText MoveCostLabel = INVTEXT("移");

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	TObjectPtr<UTexture2D> PreviewIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewName = INVTEXT("相位斩击");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewCost = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewHotkey = INVTEXT("1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewCooldown = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FLinearColor PreviewAccent = FLinearColor(0.94f, 0.72f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewAffordable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewSelected = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|SkillCard", meta = (DisplayName = "On Card Setup"))
	void BP_OnCardSetup(int32 InCardIndex, bool bInIsMove, int32 InAPCost, FLinearColor InAccent, UTexture2D* InIcon);

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|SkillCard", meta = (DisplayName = "On Card Visual State"))
	void BP_OnVisualStateChanged(bool bIsHovered, bool bIsSelected, bool bIsAffordable);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHoverBegin();

	UFUNCTION()
	void HandleHoverEnd();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERTechFrame> CardFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CostBadge;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CardBg;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectedFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HotkeyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GlyphText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> AccentLine;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CooldownBg;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Hover;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Select;

private:
	void Build();
	void BindDelegates();
	void ApplyVisualState();

	int32 CardIndex = 0;
	bool bIsMove = false;
	bool bAffordable = true;
	bool bSelected = false;
	bool bHovered = false;
	bool bBuilt = false;
	bool bDelegatesBound = false;

	FERFloatSpring LiftSpring = FERFloatSpring::Tactile(0.0f);
	float LiftT = 0.0f;
	float FadeT = 1.0f;
	float SweepProgress = 1.0f;

	FLinearColor Accent = FLinearColor::White;
	FText CardName;
	FText CardTooltip;
	FText CardDescription;
	FText CardMeta;
};
