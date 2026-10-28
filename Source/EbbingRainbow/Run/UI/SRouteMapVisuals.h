// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/**
 * 全屏背景：竖向渐变 + 由种子确定的星点。
 * 纯 Slate 绘制，不依赖关卡里的任何资产，保证 Lvl_Route 可以是空关卡。
 */
class SRouteBackdrop : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteBackdrop) {}
		SLATE_ARGUMENT(int32, Seed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	struct FStar
	{
		FVector2D Position = FVector2D::ZeroVector; // 0..1 归一化
		float Radius = 1.0f;
		float Alpha = 0.5f;
	};

	TArray<FStar> Stars;
};

/**
 * 连线层：把 DAG 的边画成贝塞尔曲线，并给"当前节点"画呼吸光环。
 * 用 SLeafWidget 自绘而不是拼 SImage，斜线不会有锯齿/拉伸问题。
 */
class SRouteEdgeLayer : public SLeafWidget
{
public:
	struct FEdgeDraw
	{
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D End = FVector2D::ZeroVector;
		bool bTaken = false;
		bool bAvailable = false;
	};

	SLATE_BEGIN_ARGS(SRouteEdgeLayer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetDrawData(const TArray<FEdgeDraw>& InEdges, const TArray<FVector2D>& InHaloCenters);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	TArray<FEdgeDraw> Edges;
	TArray<FVector2D> HaloCenters;
};
