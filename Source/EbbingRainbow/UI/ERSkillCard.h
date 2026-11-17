// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "ERSkillCard.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnERSkillCardClicked, int32, CardIndex, bool, bIsMoveCard);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnERSkillCardHover, UERSkillCard*, Card, bool, bHovered);

/**
 * 战术手牌 (平面风竖条卡):
 *   [大号费用数字]        [快捷键]
 *   [   图标 / 移 字   ]
 *   [2px 技能主题色底线]
 *   悬停/选中上浮 + 金线; 不可用整体 35% 淡出; 冷却盖右下角数字。
 */
UCLASS()
class EBBINGRAINBOW_API UERSkillCard : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InIndex, const FText& Name, const FText& Tooltip, const FText& CooldownText,
		const FText& Hotkey, int32 APCost, bool bIsMove, FLinearColor InAccent, UTexture2D* Icon);

	void SetAffordable(bool bInAffordable);
	void SetSelectedMode(bool bInSelected);
	bool IsHoveredState() const { return bHovered; }
	FText GetCardTooltip() const { return CardTooltip; }
	FText GetCardName() const { return CardName; }
	FText GetCardMeta() const { return CardMeta; }

	void SetCardMeta(const FText& InMeta) { CardMeta = InMeta; }

	UPROPERTY(BlueprintAssignable, Category = "ERUI")
	FOnERSkillCardClicked OnCardClicked;

	UPROPERTY(BlueprintAssignable, Category = "ERUI")
	FOnERSkillCardHover OnCardHover;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHoverBegin();

	UFUNCTION()
	void HandleHoverEnd();

private:
	void Build();
	void ApplyVisualState();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HotkeyText;

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

	int32 CardIndex = 0;
	bool bIsMove = false;
	bool bAffordable = true;
	bool bSelected = false;
	bool bHovered = false;
	bool bBuilt = false;

	float LiftT = 0.0f;      // 上浮插值 0..1
	float FadeT = 1.0f;      // 可用性淡出 0..1

	FLinearColor Accent = FLinearColor::White;
	FText CardName;
	FText CardTooltip;
	FText CardMeta;
};
