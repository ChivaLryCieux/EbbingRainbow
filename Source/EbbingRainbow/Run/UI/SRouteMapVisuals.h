// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"
#include "Run/RunTypes.h"

/** 航线图自绘共用的小几何工具 (节点徽章 / 图例 / 连线共用同一套形状语言)。 */
namespace RouteVis
{
	void MakeCircle(TArray<FVector2D>& Out, const FVector2D& Center, float Radius, int32 Segments = 48);
	void MakeArc(TArray<FVector2D>& Out, const FVector2D& Center, float Radius, float A0, float A1, int32 Segments = 8);
	/** 闭合正多边形, RotRad 为首顶点角度 */
	void MakePolygon(TArray<FVector2D>& Out, const FVector2D& Center, float Radius, int32 Sides, float RotRad);
	/** 类型轮廓: 安全=圆 / 战斗=菱形 / 自然=六边形 */
	void MakeTypeShape(TArray<FVector2D>& Out, ERouteNodeType Type, const FVector2D& Center, float Radius);
	/** 实心圆画刷 (RoundedBox + 半高圆角, 宽高相等即正圆) */
	void ConfigureCircleBrush(FSlateBrush& Brush);
}

/**
 * 全屏背景: 只做可读性压暗 (3D 场景负责星尘/弥散光/虹弧)。
 * 纯 Slate 绘制, 不依赖关卡里的任何资产。
 */
class SRouteBackdrop : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteBackdrop)
		: _Seed(0)
	{}
		SLATE_ARGUMENT(int32, Seed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
};

/**
 * 连线层: 把 DAG 的边画成贝塞尔曲线。
 * 已走=金色实线+辉光; 可选=青白亮线+流光; 前路(可达)=细实线; 其余=极淡虚线。
 * 两端按节点半径裁掉, 线不会钻进徽章。
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
		/** 位于"当前节点之后仍可达"的路径上 */
		bool bReachable = false;
	};

	SLATE_BEGIN_ARGS(SRouteEdgeLayer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetDrawData(const TArray<FEdgeDraw>& InEdges);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	TArray<FEdgeDraw> Edges;
	FSlateBrush CircleBrush;
};

/** 画布底部的层级标尺: 细线 + 刻度 + 层号, 已走过的部分点金。 */
class SRouteLayerRuler : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteLayerRuler) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** LayerXs: 每层在画布中的 X; CurrentLayer: 当前所在层 (0 起) */
	void SetData(const TArray<float>& InLayerXs, int32 InCurrentLayer);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	TArray<float> LayerXs;
	int32 CurrentLayer = 0;
};

/** 图例小图标: 画出某节点类型的轮廓形状。 */
class SRouteTypeIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteTypeIcon)
		: _Type(ERouteNodeType::Safe)
		, _Accent(FLinearColor::White)
	{}
		SLATE_ARGUMENT(ERouteNodeType, Type)
		SLATE_ARGUMENT(FLinearColor, Accent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	ERouteNodeType Type = ERouteNodeType::Safe;
	FLinearColor Accent = FLinearColor::White;
};
