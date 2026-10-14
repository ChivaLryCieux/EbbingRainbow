// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"
#include "Battle/Core/BattleTypes.h"

class UBattleManager;
class ABattlePlayerController;
class ABattleUnit;
class SHorizontalBox;
class SVerticalBox;
class STextBlock;
class SButton;
class SBorder;
class SProgressBar;

/**
 * 战术手牌卡片控件 (Slay the Spire 风格)
 * 支持悬停平滑上浮、缩放、金边高亮以及点击出牌/选中。
 */
class EBBINGRAINBOW_API SBattleSkillCardWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBattleSkillCardWidget)
		: _CardIndex(0)
		, _APCost(0.0f)
		, _bIsMoveCard(false)
		, _AccentColor(FLinearColor(0.2f, 0.6f, 1.0f))
	{}
		SLATE_ARGUMENT(int32, CardIndex)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Category)
		SLATE_ARGUMENT(FText, Description)
		SLATE_ARGUMENT(FText, HotkeyText)
		SLATE_ARGUMENT(float, APCost)
		SLATE_ARGUMENT(bool, bIsMoveCard)
		SLATE_ARGUMENT(FLinearColor, AccentColor)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void SetAffordable(bool bInAffordable) { bCanAfford = bInAffordable; }
	void SetSelectedMode(bool bInSelected) { bIsSelected = bInSelected; }
	bool IsHoveredState() const { return bHovered; }

private:
	int32 CardIndex = 0;
	float APCost = 0.0f;
	bool bIsMoveCard = false;
	bool bCanAfford = true;
	bool bIsSelected = false;
	bool bHovered = false;

	float HoverOffset = 0.0f;
	float CurrentScale = 1.0f;

	FLinearColor AccentColor = FLinearColor::White;
	FOnClicked OnClickedDelegate;

	FSlateBrush CardBgBrush;
	FSlateBrush CostGemBrush;
	TSharedPtr<SBorder> BorderWidget;
};

/**
 * 现代战术主界面 Slate 覆层:
 * 1. 顶部先攻队列栏 (芯片流转展示)
 * 2. 底部手牌与动作栏 (英雄状态 + 战术卡牌 + 金色结束回合大按钮)
 * 3. 动态战报通知条
 * 4. 胜负结算横幅
 */
class EBBINGRAINBOW_API SBattleMainOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBattleMainOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UBattleManager>, BattleManager)
		SLATE_ARGUMENT(TWeakObjectPtr<ABattlePlayerController>, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** 检查鼠标当前是否正悬停在交互性 UI (手牌或按钮) 上, 防止场景点地穿透 */
	bool IsMouseOverInteractiveUI() const;

	/** 刷新战报通知 */
	void ShowNotification(const FString& InText);

private:
	TWeakObjectPtr<UBattleManager> BattleManager;
	TWeakObjectPtr<ABattlePlayerController> PlayerController;

	// UI 核心部件引用
	TSharedPtr<SHorizontalBox> InitiativeHBox;
	TSharedPtr<STextBlock> NotificationTextBlock;
	TSharedPtr<SBorder> NotificationBorder;

	TSharedPtr<STextBlock> ActiveHeroNameText;
	TSharedPtr<STextBlock> ActiveHeroHPText;
	TSharedPtr<SProgressBar> ActiveHeroHPBar;
	TSharedPtr<STextBlock> ActiveHeroAPText;
	TSharedPtr<SHorizontalBox> HandCardsBox;

	TSharedPtr<SButton> EndTurnButton;
	TSharedPtr<STextBlock> EndTurnButtonText;

	TSharedPtr<SBorder> ResultBannerBorder;
	TSharedPtr<STextBlock> ResultBannerText;

	TArray<TSharedPtr<SBattleSkillCardWidget>> CardWidgets;
	TArray<TSharedPtr<FSlateBrush>> ChipBrushes;

	// 缓存状态比对
	TWeakObjectPtr<ABattleUnit> CachedActiveUnit;
	int32 CachedQueueHash = 0;
	float CachedAP = -1.0f;
	int32 CachedHP = -1;
	EPlayerInteractionMode CachedMode = EPlayerInteractionMode::Idle;
	EBattleState CachedState = EBattleState::TurnActive;

	float NotificationTimer = 0.0f;
	bool bMouseOverUI = false;

	// 样式画刷
	FSlateBrush TopBarBgBrush;
	FSlateBrush HeroPlateBgBrush;
	FSlateBrush EndTurnNormalBrush;
	FSlateBrush EndTurnHoverBrush;
	FSlateBrush EndTurnPressedBrush;
	FSlateBrush EndTurnDisabledBrush;
	FButtonStyle EndTurnBtnStyle;

	void RebuildInitiativeBar();
	void RebuildHandCards(ABattleUnit* Unit);
	void UpdateHeroPlate(ABattleUnit* Unit);
	void UpdateEndTurnButton(ABattleUnit* Unit, EBattleState State);
	void UpdateResultBanner(EBattleState State);

	FReply HandleEndTurnClicked();
	FReply HandleCardClicked(int32 Index, bool bIsMove);
};
