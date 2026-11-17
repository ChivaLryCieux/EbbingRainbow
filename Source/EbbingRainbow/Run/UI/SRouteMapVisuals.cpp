// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/UI/SRouteMapVisuals.h"
#include "Run/RouteLayout.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FSlateBrush* GetWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	FPaintGeometry BoxGeo(const FGeometry& G, const FVector2D& Pos, const FVector2D& Size)
	{
		return G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos));
	}
}

namespace RouteVis
{
	void MakeCircle(TArray<FVector2D>& Out, const FVector2D& Center, float Radius, int32 Segments)
	{
		MakeArc(Out, Center, Radius, 0.0f, 2.0f * PI, Segments);
	}

	void MakeArc(TArray<FVector2D>& Out, const FVector2D& Center, float Radius, float A0, float A1, int32 Segments)
	{
		Out.Reset(Segments + 1);
		for (int32 I = 0; I <= Segments; ++I)
		{
			const float A = FMath::Lerp(A0, A1, (float)I / (float)Segments);
			Out.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
	}

	void MakePolygon(TArray<FVector2D>& Out, const FVector2D& Center, float Radius, int32 Sides, float RotRad)
	{
		Out.Reset(Sides + 1);
		for (int32 I = 0; I <= Sides; ++I)
		{
			const float A = RotRad + 2.0f * PI * (float)(I % Sides) / (float)Sides;
			Out.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
	}

	void MakeTypeShape(TArray<FVector2D>& Out, ERouteNodeType Type, const FVector2D& Center, float Radius)
	{
		switch (Type)
		{
		case ERouteNodeType::Battle:       MakePolygon(Out, Center, Radius * 1.12f, 4, -PI * 0.5f); break;
		case ERouteNodeType::BattleNature: MakePolygon(Out, Center, Radius * 1.06f, 6, -PI * 0.5f); break;
		default:                           MakeCircle(Out, Center, Radius, 56); break;
		}
	}

	void ConfigureCircleBrush(FSlateBrush& Brush)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.Width = 0.0f;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0, 0, 0, 0));
	}
}

// ---------------------------------------------------------------- 背景

void SRouteBackdrop::Construct(const FArguments& InArgs)
{
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

	// 顶/底渐隐暗带: 给标题与标尺垫底, 中部完全透出 3D 场景
	const int32 Bands = 14;
	for (int32 Band = 0; Band < Bands; ++Band)
	{
		const float F = 1.0f - (float)Band / (float)Bands;
		const float Alpha = 0.34f * F * F;
		const float TopH = Size.Y * 0.16f / Bands;
		const float BotH = Size.Y * 0.14f / Bands;
		const FLinearColor Color(0.0f, 0.004f, 0.012f, Alpha);

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, BoxGeo(AllottedGeometry, FVector2D(0.0, TopH * Band), FVector2D(Size.X, TopH + 0.5f)),
			WhiteBrush, ESlateDrawEffect::None, Color);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, BoxGeo(AllottedGeometry, FVector2D(0.0, Size.Y - BotH * (Band + 1)), FVector2D(Size.X, BotH + 0.5f)),
			WhiteBrush, ESlateDrawEffect::None, Color);
	}
	return LayerId;
}

// ---------------------------------------------------------------- 连线

void SRouteEdgeLayer::Construct(const FArguments& InArgs)
{
	RouteVis::ConfigureCircleBrush(CircleBrush);
	ForceVolatile(true);
}

FVector2D SRouteEdgeLayer::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(100.0f, 100.0f);
}

void SRouteEdgeLayer::SetDrawData(const TArray<FEdgeDraw>& InEdges)
{
	Edges = InEdges;
}

int32 SRouteEdgeLayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry();
	TArray<FVector2D> Pts;

	// 克制: 连线一律细实线, 不做辉光/流光; 只靠明度和色相区分状态
	// 先画远的/暗的, 再画已走, 最后画可选, 保证重要的压在上面
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		for (const FEdgeDraw& E : Edges)
		{
			const int32 Rank = E.bAvailable ? 3 : (E.bTaken ? 2 : (E.bReachable ? 1 : 0));
			if (Rank != Pass) { continue; }

			ERRouteLayout::SampleEdge(E.Start, E.End, Pts, 64);
			if (Pts.Num() < 2) { continue; }

			FLinearColor Color;
			float Thickness;
			switch (Rank)
			{
			case 3:  Color = FLinearColor(0.78f, 0.90f, 1.00f, 0.80f); Thickness = 1.7f; break;
			case 2:  Color = FLinearColor(1.00f, 0.82f, 0.46f, 0.70f); Thickness = 1.5f; break;
			case 1:  Color = FLinearColor(0.62f, 0.74f, 0.95f, 0.30f); Thickness = 1.2f; break;
			default: Color = FLinearColor(0.55f, 0.65f, 0.85f, 0.10f); Thickness = 1.0f; break;
			}
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Pass, Geo, Pts, ESlateDrawEffect::None, Color, true, Thickness);
		}
	}

	return LayerId + 4;
}
// ---------------------------------------------------------------- 层级标尺

void SRouteLayerRuler::Construct(const FArguments& InArgs)
{
}

FVector2D SRouteLayerRuler::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(100.0f, 100.0f);
}

void SRouteLayerRuler::SetData(const TArray<float>& InLayerXs, int32 InCurrentLayer)
{
	LayerXs = InLayerXs;
	CurrentLayer = InCurrentLayer;
}

int32 SRouteLayerRuler::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* White = GetWhiteBrush();
	if (!White || LayerXs.Num() < 2) { return LayerId; }

	const float Y = ERRouteLayout::RulerY;
	const float X0 = LayerXs[0] - 44.0f;
	const float X1 = LayerXs.Last() + 44.0f;
	const int32 Cur = FMath::Clamp(CurrentLayer, 0, LayerXs.Num() - 1);
	const FLinearColor Gold = ERRouteLayout::GoldColor();

	auto Rect = [&](float X, float YY, float W, float H, const FLinearColor& C)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, BoxGeo(AllottedGeometry, FVector2D(X, YY), FVector2D(W, H)),
			White, ESlateDrawEffect::None, C);
	};

	Rect(X0, Y - 0.5f, X1 - X0, 1.0f, FLinearColor(0.60f, 0.72f, 0.92f, 0.20f));
	const float XC = LayerXs[Cur];
	Rect(X0, Y - 3.0f, XC - X0, 6.0f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.07f));
	Rect(X0, Y - 0.8f, XC - X0, 1.6f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.80f));

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo FontBold = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	for (int32 I = 0; I < LayerXs.Num(); ++I)
	{
		const float X = LayerXs[I];
		const bool bPast = I < Cur, bNow = I == Cur;
		const bool bLast = I == LayerXs.Num() - 1;
		const FLinearColor TickC = bNow ? FLinearColor(1.0f, 0.90f, 0.60f, 1.0f)
			: bPast ? FLinearColor(Gold.R, Gold.G, Gold.B, 0.65f)
			: FLinearColor(0.62f, 0.74f, 0.94f, 0.34f);

		if (bNow)
		{
			TArray<FVector2D> D;
			RouteVis::MakePolygon(D, FVector2D(X, Y), 7.0f, 4, -PI * 0.5f);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), D,
				ESlateDrawEffect::None, TickC, true, 1.6f);
			Rect(X - 2.0f, Y - 2.0f, 4.0f, 4.0f, TickC);
		}
		else
		{
			Rect(X - 0.5f, Y - (bLast ? 6.0f : 4.0f), 1.0f, bLast ? 12.0f : 8.0f, TickC);
		}

		const FString Label = bLast ? FString(TEXT("终")) : FString::FromInt(I + 1);
		const FSlateFontInfo& F = (bNow || bLast) ? FontBold : Font;
		const FVector2D TS = Measure->Measure(Label, F);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
			BoxGeo(AllottedGeometry, FVector2D(X - TS.X * 0.5f, Y + 13.0f), TS),
			Label, F, ESlateDrawEffect::None,
			bNow ? FLinearColor(1.0f, 0.88f, 0.55f, 0.95f)
			: bPast ? FLinearColor(Gold.R, Gold.G, Gold.B, 0.55f)
			: bLast ? FLinearColor(0.85f, 0.92f, 1.0f, 0.55f)
			: FLinearColor(0.70f, 0.80f, 0.95f, 0.38f));
	}

	return LayerId + 2;
}

// ---------------------------------------------------------------- 图例图标

void SRouteTypeIcon::Construct(const FArguments& InArgs)
{
	Type = InArgs._Type;
	Accent = InArgs._Accent;
}

FVector2D SRouteTypeIcon::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(20.0f, 20.0f);
}

int32 SRouteTypeIcon::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D C = AllottedGeometry.GetLocalSize() * 0.5;
	TArray<FVector2D> P;
	RouteVis::MakeTypeShape(P, Type, C, 7.0f);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P,
		ESlateDrawEffect::None, FLinearColor(Accent.R, Accent.G, Accent.B, 0.95f), true, 1.6f);
	RouteVis::MakeCircle(P, C, 2.2f, 12);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P,
		ESlateDrawEffect::None, FLinearColor(Accent.R, Accent.G, Accent.B, 0.75f), true, 2.4f);
	return LayerId;
}

