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

class SBattleUnitPortrait;

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
 * 现代战术主界面 Slate 覆层 (苹果液态玻璃 · 与 Hall 大厅同源质感):
 *
 *  1. 顶部先攻队列栏 (玻璃胶囊芯片流转)
 *  2. 左侧我方编队名牌面板 (占位头像 / 名字 / 血量, 可点击聚焦摄像机)
 *  3. 底部手牌与动作栏 (英雄状态 + 战术卡牌 + 金色结束回合玻璃按钮)
 *  4. 动态战报通知条
 *  5. 胜负结算横幅
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

	/** 检查鼠标当前是否正悬停在交互性 UI (名牌 / 手牌 / 按钮) 上, 防止场景点地穿透 */
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
	TSharedPtr<SBorder> EndTurnGlowBorder;

	// 入场动画容器: 顶栏下沉 / 底栏上浮
	TSharedPtr<SBorder> TopPanelBorder;
	TSharedPtr<SHorizontalBox> BottomBarBox;

	TSharedPtr<SBorder> ResultBannerBorder;
	TSharedPtr<STextBlock> ResultBannerText;

	// 左侧编队名牌面板
	TSharedPtr<SVerticalBox> RosterBox;
	TArray<TSharedPtr<SBattleUnitPortrait>> PortraitWidgets;
	TArray<TWeakObjectPtr<ABattleUnit>> CachedPlayerUnits;

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

	// 入场动画
	float EntranceProgress = 0.0f;

	// 样式画刷 (玻璃系统)
	FSlateBrush PanelBrush;          // 大面板 (先攻栏 / 英雄板 / 结算横幅)
	FSlateBrush PanelShadowBrush;    // 面板柔影 (等效投影, 让玻璃从场景里浮起)
	FSlateBrush CardBrush;           // 中等卡片 (先攻芯片)
	FSlateBrush NotificationBrush;
	FSlateBrush EndTurnNormalBrush;
	FSlateBrush EndTurnHoverBrush;
	FSlateBrush EndTurnPressedBrush;
	FSlateBrush EndTurnDisabledBrush;
	FSlateBrush EndTurnGlowBrush;
	FSlateBrush HPTrackBrush;
	FSlateBrush HPFillBrush;
	FButtonStyle EndTurnBtnStyle;

	void RebuildInitiativeBar();
	void RebuildHandCards(ABattleUnit* Unit);
	void RebuildRosterPanel();
	void UpdateHeroPlate(ABattleUnit* Unit);
	void UpdateEndTurnButton(ABattleUnit* Unit, EBattleState State);
	void UpdateResultBanner(EBattleState State);

	FReply HandleEndTurnClicked();
	FReply HandleCardClicked(int32 Index, bool bIsMove);
	FReply HandlePortraitClicked(int32 UnitIndex);
};
