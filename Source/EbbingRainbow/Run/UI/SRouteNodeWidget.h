// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Input/Reply.h"

class SBorder;
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
 * 路线图上的一个节点徽章：玻璃圆盘 + 单字字形 + 类型名。
 * 交互交给内部的 SButton，视觉状态由外部每帧/每次刷新驱动。
 */
class SRouteNodeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteNodeWidget)
		: _NodeId(-1)
	{}
		SLATE_ARGUMENT(int32, NodeId)
		SLATE_ARGUMENT(FText, Glyph)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Tooltip)
		SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetVisual(ERouteNodeVisual InVisual);
	int32 GetNodeId() const { return NodeId; }

private:
	void ApplyVisual();

	int32 NodeId = INDEX_NONE;
	ERouteNodeVisual Visual = ERouteNodeVisual::Locked;
	float PulseTime = 0.0f;
	FLinearColor Accent = FLinearColor::White;

	FSlateBrush BadgeBrush;
	TSharedPtr<SBorder> Badge;
	TSharedPtr<STextBlock> GlyphText;
	TSharedPtr<STextBlock> LabelText;
	TSharedPtr<SButton> Button;
};
