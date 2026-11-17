// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
#include "Input/Reply.h"
#include "Run/RunTypes.h"

class SButton;
class STextBlock;

/** 节点的四种表现状态。 */
enum class ERouteNodeVisual : uint8
{
	Locked,
	Available,
	Current,
	Visited
};

/**
 * 节点徽章的自绘部分 ("印记"): 辉光 + 暗盘 + 双环 + 类型轮廓 (圆/菱形/六边形)
 * + 状态装饰 (可选=旋转断环, 当前=双向旋转环, 已走=对勾, 终点=放射光芒)。
 * 动画只用 FPaintArgs 时间 + 外部喂的悬停/按下插值, 不依赖 Tick 重建控件。
 */
class SRouteNodeSigil : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteNodeSigil)
		: _Type(ERouteNodeType::Safe)
		, _Accent(FLinearColor::White)
		, _bFinal(false)
		, _Phase(0.0f)
	{}
		SLATE_ARGUMENT(ERouteNodeType, Type)
		SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_ARGUMENT(bool, bFinal)
		SLATE_ARGUMENT(float, Phase)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetState(ERouteNodeVisual InVisual, bool bInReachable);
	void Advance(float DeltaTime, bool bHovered, bool bPressed);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	ERouteNodeType Type = ERouteNodeType::Safe;
	FLinearColor Accent = FLinearColor::White;
	bool bFinal = false;
	float Phase = 0.0f;

	ERouteNodeVisual Visual = ERouteNodeVisual::Locked;
	bool bReachable = false;
	float HoverT = 0.0f;
	float PressT = 0.0f;

	FSlateBrush CircleBrush;
};

/**
 * 路线图上的一个节点: 印记 + 单字字形 + 类型名。
 * 交互交给内部的 SButton, 视觉状态由外部每次刷新驱动。
 * 节点控件尺寸固定为 ERRouteLayout::NodeW x NodeH, 徽章中心正好是控件中心。
 */
class SRouteNodeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteNodeWidget)
		: _NodeId(-1)
		, _NodeType(ERouteNodeType::Safe)
		, _Accent(FLinearColor::White)
		, _bFinal(false)
	{}
		SLATE_ARGUMENT(int32, NodeId)
		SLATE_ARGUMENT(ERouteNodeType, NodeType)
		SLATE_ARGUMENT(FText, Glyph)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Tooltip)
		SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_ARGUMENT(bool, bFinal)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetVisual(ERouteNodeVisual InVisual, bool bInReachable);
	int32 GetNodeId() const { return NodeId; }

private:
	void ApplyVisual();

	int32 NodeId = INDEX_NONE;
	ERouteNodeVisual Visual = ERouteNodeVisual::Locked;
	bool bReachable = false;
	bool bVisualInit = false;
	FLinearColor Accent = FLinearColor::White;

	TSharedPtr<SRouteNodeSigil> Sigil;
	TSharedPtr<STextBlock> GlyphText;
	TSharedPtr<STextBlock> LabelText;
	TSharedPtr<SButton> Button;
};
