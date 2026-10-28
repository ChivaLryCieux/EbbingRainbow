// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"

class SBorder;
class SButton;
class STextBlock;
class SVerticalBox;

/**
 * 结算面板里的一行「得失」数据。
 *
 * 目前只填战斗回合 / 存活队员 / 队伍完整度, 后续加掉落、经验、好感度、任务进度时
 * 只需要往这个数组里多塞几行, 面板本身不用改。
 */
struct FBattleResultRow
{
	FText Label;
	FText Value;
	FLinearColor ValueColor = FLinearColor::White;

	FBattleResultRow() = default;
	FBattleResultRow(const FText& InLabel, const FText& InValue, const FLinearColor& InValueColor = FLinearColor::White)
		: Label(InLabel), Value(InValue), ValueColor(InValueColor) {}
};

/**
 * 战斗结算弹窗 (纯 Slate):
 *
 *   ┌──────────────────────────────┐
 *   │  战 斗 结 算 · BATTLE RESULT │
 *   │         战斗胜利             │
 *   │      敌方全灭 · 节点 3       │
 *   │  ─────────────────────────   │
 *   │  战斗回合           4        │
 *   │  存活队员           3 / 4    │
 *   │  队伍完整度         112 / 210│
 *   │  战利品             —        │
 *   │        [ 返回航线图 ]        │
 *   └──────────────────────────────┘
 *
 * 半透明全屏遮罩 + 中央玻璃面板, 入场做淡入 + 上浮。
 * 面板只负责「显示 + 一个主按钮」, 跳转逻辑由外部委托决定。
 */
class EBBINGRAINBOW_API SBattleResultPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBattleResultPanel) {}
		/** 主按钮点击 (返回航线图 / 返回大厅) */
		SLATE_EVENT(FSimpleDelegate, OnPrimaryClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** 填充结算内容并弹出 (每次战斗结束调用一次) */
	void SetResult(bool bInVictory, const FText& InTitle, const FText& InSubtitle,
		const TArray<FBattleResultRow>& InRows, const FText& InButtonText);

private:
	FSimpleDelegate OnPrimaryClicked;

	TSharedPtr<SBorder> RootBorder;
	TSharedPtr<STextBlock> KickerText;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> SubtitleText;
	TSharedPtr<SVerticalBox> RowsBox;
	TSharedPtr<SButton> PrimaryButton;
	TSharedPtr<STextBlock> PrimaryButtonText;

	FSlateBrush PanelBrush;
	FSlateBrush PanelShadowBrush;
	FSlateBrush DividerBrush;
	FSlateBrush BackdropBrush;

	FButtonStyle VictoryBtnStyle;
	FButtonStyle DefeatBtnStyle;
	FSlateBrush VictoryNormal;
	FSlateBrush VictoryHover;
	FSlateBrush VictoryPressed;
	FSlateBrush DefeatNormal;
	FSlateBrush DefeatHover;
	FSlateBrush DefeatPressed;

	/** 入场动画进度 0..1 */
	float Entrance = 0.0f;
	bool bVictory = true;
	/** 是否已经打过一次布局日志 (验证面板确实以正常尺寸出现在屏幕中央) */
	bool bLoggedGeometry = false;
};
