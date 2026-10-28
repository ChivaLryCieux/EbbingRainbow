// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Battle/UI/SBattleGlassKit.h"

class SBorder;
class SButton;
class STextBlock;
class SProgressBar;
class SHorizontalBox;
class SVerticalBox;
class ABattleUnit;
/**
 * 我方单位名牌卡片 (苹果液态玻璃 · 竖排编队面板的一行)。
 *
 * 一行包含: 占位头像 (首字母徽章) / 单位名 / 血量条与数值 / AP 行动点。
 * 状态层级 (由弱到强):
 *   常规 → 悬停提亮 → 当前行动回合 (金色呼吸) → 已聚焦 (冰蓝描边 + 头像点亮)
 * 阵亡单位整体去饱和, 名字置灰, 血条归零并标注 [已损毁]。
 */
class EBBINGRAINBOW_API SBattleUnitPortrait : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBattleUnitPortrait)
		: _UnitIndex(0)
		, _CardWidth(252.0f)
		, _EntranceDelay(0.0f)
	{}
		SLATE_ARGUMENT(int32, UnitIndex)
		SLATE_ARGUMENT(float, CardWidth)
		SLATE_ARGUMENT(float, EntranceDelay)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/** 绑定要展示的单位 ( nullptr 表示该槽位暂无单位 ) */
	void SetUnit(ABattleUnit* InUnit);

	/** 聚焦态: 摄像机已对准该单位 */
	void SetFocused(bool bInFocused);

	ABattleUnit* GetUnit() const { return BoundUnit.Get(); }
	bool IsHoveredState() const { return bHovered; }

private:
	/** 依据单位实时数据刷新文字与血条 */
	void RefreshFromUnit();

	/** 每帧推进玻璃质感动画 */
	void RefreshVisuals(double CurrentTime, float DeltaTime);

	int32 UnitIndex = 0;
	TWeakObjectPtr<ABattleUnit> BoundUnit;

	// 子控件
	TSharedPtr<SButton> RootButton;
	TSharedPtr<SBorder> GlowBorder;
	TSharedPtr<SBorder> BodyBorder;
	TSharedPtr<SBorder> TopHighlightBorder;
	TSharedPtr<SBorder> AvatarBorder;
	TSharedPtr<STextBlock> AvatarLetter;
	TSharedPtr<STextBlock> NameText;
	TSharedPtr<STextBlock> HPText;
	TSharedPtr<STextBlock> APText;
	TSharedPtr<SProgressBar> HPBar;
	TSharedPtr<SBorder> FocusRail;

	// 画刷
	FSlateBrush GlowBrush;
	FSlateBrush BodyBrush;
	FSlateBrush TopHighlightBrush;
	FSlateBrush AvatarBrush;
	FSlateBrush RailBrush;
	FSlateBrush HPFillBrush;
	FSlateBrush HPTrackBrush;

	// 状态
	bool bHovered = false;
	bool bFocused = false;
	bool bActiveTurn = false;
	bool bDead = false;

	// 动画状态 (0..1 指数平滑)
	float HoverAlpha = 0.0f;
	float PressAlpha = 0.0f;
	float FocusAlpha = 0.0f;
	float ActiveAlpha = 0.0f;
	float DeathAlpha = 0.0f;

	// 血量条平滑
	float DisplayedHpPercent = 1.0f;
	int32 CachedMaxAP = -1;

	// 文本缓存: 仅在这些值变化时才重建字符串, 避免每帧分配
	FString CachedName;
	float CachedIntegrity = -1.0f;
	float CachedBarrier = -1.0f;
	int32 CachedAP = -1;
	bool bCachedDead = false;

	// 入场动画
	float EntranceDelay = 0.0f;
	float EntranceProgress = 0.0f;
	double EntranceStartTime = -1.0;

	FLinearColor AccentColor = BattleGlass::AccentPlayer;

	FOnClicked OnClickedDelegate;
};
