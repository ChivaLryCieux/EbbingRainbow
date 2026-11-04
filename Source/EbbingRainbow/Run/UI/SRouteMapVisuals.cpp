// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/UI/SRouteMapVisuals.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FSlateBrush* GetWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	/** 三次贝塞尔采样，用于把跨层连线画成柔和的 S 形（层级沿 X 轴推进，所以切向沿 X）。 */
	void SampleBezier(const FVector2D& Start, const FVector2D& End, TArray<FVector2D>& OutPoints, int32 Segments = 18)
	{
		const double DeltaX = End.X - Start.X;
		const FVector2D Control1(Start.X + DeltaX * 0.45, Start.Y);
		const FVector2D Control2(End.X - DeltaX * 0.45, End.Y);

		OutPoints.Reset(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const double T = static_cast<double>(Index) / static_cast<double>(Segments);
			const double U = 1.0 - T;
			const double W0 = U * U * U;
			const double W1 = 3.0 * U * U * T;
			const double W2 = 3.0 * U * T * T;
			const double W3 = T * T * T;

			OutPoints.Add(FVector2D(
				Start.X * W0 + Control1.X * W1 + Control2.X * W2 + End.X * W3,
				Start.Y * W0 + Control1.Y * W1 + Control2.Y * W2 + End.Y * W3));
		}
	}

	void MakeCirclePoints(const FVector2D& Center, float Radius, TArray<FVector2D>& OutPoints, int32 Segments = 40)
	{
		OutPoints.Reset(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = (2.0f * PI * static_cast<float>(Index)) / static_cast<float>(Segments);
			OutPoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
	}
}

void SRouteBackdrop::Construct(const FArguments& InArgs)
{
	const int32 Seed = InArgs._Seed != 0 ? InArgs._Seed : 12345;
	FRandomStream Rng(Seed);

	Stars.Reset(180);
	for (int32 Index = 0; Index < 180; ++Index)
	{
		FStar Star;
		Star.Position = FVector2D(Rng.FRand(), Rng.FRand());
		Star.Radius = Rng.FRandRange(0.6f, 2.1f);
		Star.Alpha = Rng.FRandRange(0.12f, 0.75f);
		Stars.Add(Star);
	}
}

FVector2D SRouteBackdrop::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(100.0f, 100.0f);
}

int32 SRouteBackdrop::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FSlateBrush* WhiteBrush = GetWhiteBrush();
	if (!WhiteBrush || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return LayerId;
	}

	// 单一半透明底色: 让 3D 航线场景透出来 (竖向色带会在像素网格上出莫尔条纹,
	// 渐变已由 3D 雾提供)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.010f, 0.018f, 0.040f, 0.32f));

	// 星点：位置由种子决定，同一局同一张图。
	for (const FStar& Star : Stars)
	{
		const FVector2D Position = Star.Position * Size;
		const float Radius = Star.Radius;
		const FLinearColor Color(0.80f, 0.88f, 1.00f, Star.Alpha);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(Radius * 2.0f, Radius * 2.0f),
				FSlateLayoutTransform(Position - FVector2D(Radius, Radius))),
			WhiteBrush,
			ESlateDrawEffect::None,
			Color);
	}

	// 顶部与底部的暗角，让中间的地图更聚焦。
	const int32 VignetteBands = 12;
	for (int32 Band = 0; Band < VignetteBands; ++Band)
	{
		const float Alpha = 0.30f * (1.0f - static_cast<float>(Band) / static_cast<float>(VignetteBands));
		const float Height = Size.Y * 0.18f / static_cast<float>(VignetteBands);
		const FLinearColor Color(0.0f, 0.0f, 0.0f, Alpha);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, Height), FSlateLayoutTransform(FVector2D(0.0, Height * Band))),
			WhiteBrush,
			ESlateDrawEffect::None,
			Color);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, Height),
				FSlateLayoutTransform(FVector2D(0.0, Size.Y - Height * (Band + 1)))),
			WhiteBrush,
			ESlateDrawEffect::None,
			Color);
	}

	return LayerId + 2;
}

void SRouteEdgeLayer::Construct(const FArguments& InArgs)
{
}

FVector2D SRouteEdgeLayer::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(100.0f, 100.0f);
}

void SRouteEdgeLayer::SetDrawData(const TArray<FEdgeDraw>& InEdges, const TArray<FVector2D>& InHaloCenters)
{
	Edges = InEdges;
	HaloCenters = InHaloCenters;
}

int32 SRouteEdgeLayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TArray<FVector2D> Points;

	// 先画锁定/已走的边，再画可选的边，保证高亮压在最上面。
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FEdgeDraw& Edge : Edges)
		{
			const bool bHighlight = Edge.bAvailable;
			if (bHighlight != (Pass == 1))
			{
				continue;
			}

			FLinearColor Color;
			float Thickness = 1.4f;
			if (Edge.bTaken)
			{
				Color = FLinearColor(1.00f, 0.78f, 0.32f, 0.55f);
				Thickness = 1.8f;
			}
			else if (Edge.bAvailable)
			{
				Color = FLinearColor(0.42f, 0.85f, 1.00f, 0.90f);
				Thickness = 2.4f;
			}
			else
			{
				Color = FLinearColor(0.55f, 0.66f, 0.82f, 0.30f);
			}

			SampleBezier(Edge.Start, Edge.End, Points);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + (bHighlight ? 1 : 0),
				AllottedGeometry.ToPaintGeometry(),
				Points,
				ESlateDrawEffect::None,
				Color,
				true,
				Thickness);
		}
	}

	// 当前节点光环
	for (const FVector2D& Center : HaloCenters)
	{
		MakeCirclePoints(Center, 54.0f, Points);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, FLinearColor(1.0f, 0.82f, 0.28f, 0.55f), true, 2.0f);

		MakeCirclePoints(Center, 64.0f, Points);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, FLinearColor(1.0f, 0.82f, 0.28f, 0.16f), true, 6.0f);
	}

	return LayerId + 2;
}
