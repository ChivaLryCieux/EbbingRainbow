// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/UI/SRouteNodeWidget.h"
#include "Run/RouteLayout.h"
#include "Run/UI/SRouteMapVisuals.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FLinearColor WithAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }
}

// ---------------------------------------------------------------- 印记

void SRouteNodeSigil::Construct(const FArguments& InArgs)
{
	Type = InArgs._Type;
	Accent = InArgs._Accent;
	bFinal = InArgs._bFinal;
	Phase = InArgs._Phase;
	RouteVis::ConfigureCircleBrush(CircleBrush);
	ForceVolatile(true);
}

FVector2D SRouteNodeSigil::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(88.0f, 88.0f);
}

void SRouteNodeSigil::SetState(ERouteNodeVisual InVisual, bool bInReachable)
{
	Visual = InVisual;
	bReachable = bInReachable;
}

void SRouteNodeSigil::Advance(float DeltaTime, bool bHovered, bool bPressed)
{
	const bool bLive = (Visual == ERouteNodeVisual::Available);
	HoverT = FMath::FInterpTo(HoverT, (bHovered && bLive) ? 1.0f : 0.0f, DeltaTime, 12.0f);
	PressT = FMath::FInterpTo(PressT, (bPressed && bLive) ? 1.0f : 0.0f, DeltaTime, 20.0f);
}

int32 SRouteNodeSigil::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D C = AllottedGeometry.GetLocalSize() * 0.5;
	const float Time = (float)Args.GetCurrentTime() + Phase;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 1.3f);
	const float Sc = (bFinal ? 1.22f : 1.0f) * (1.0f + 0.08f * HoverT - 0.05f * PressT);
	const float R = 34.0f * Sc;
	const FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry();
	const FLinearColor Gold = ERRouteLayout::GoldColor();

	FLinearColor Ring, Fill, Soft;
	float RingW = 1.4f, GlowAmt = 0.0f, ShapeA = 0.4f, InnerA = 0.0f;
	switch (Visual)
	{
	case ERouteNodeVisual::Available:
		Ring = Accent; RingW = 2.3f;
		Fill = FLinearColor(0.020f + Accent.R * 0.07f, 0.034f + Accent.G * 0.07f, 0.060f + Accent.B * 0.07f, 0.95f);
		Soft = Accent; GlowAmt = 0.0f; ShapeA = 0.70f; InnerA = 0.06f + 0.06f * HoverT;
		break;
	case ERouteNodeVisual::Current:
		Ring = FLinearColor(1.0f, 0.88f, 0.52f, 1.0f); RingW = 2.7f;
		Fill = FLinearColor(0.085f, 0.062f, 0.020f, 0.96f);
		Soft = Gold; GlowAmt = 1.05f + 0.22f * Pulse; ShapeA = 0.95f; InnerA = 0.20f;
		break;
	case ERouteNodeVisual::Visited:
		Ring = WithAlpha(Gold, 0.52f); RingW = 1.5f;
		Fill = FLinearColor(0.026f, 0.026f, 0.034f, 0.92f);
		Soft = Gold; GlowAmt = 0.10f; ShapeA = 0.34f;
		break;
	default:
		Ring = WithAlpha(Accent, bReachable ? 0.58f : 0.24f); RingW = 1.3f;
		Fill = FLinearColor(0.018f, 0.028f, 0.052f, bReachable ? 0.88f : 0.72f);
		Soft = Accent; GlowAmt = bReachable ? 0.10f : 0.0f; ShapeA = bReachable ? 0.32f : 0.14f;
		break;
	}

	auto Disc = [&](float Radius, const FLinearColor& Col, int32 L)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + L,
			AllottedGeometry.ToPaintGeometry(FVector2D(Radius * 2.0f, Radius * 2.0f), FSlateLayoutTransform(C - FVector2D(Radius, Radius))),
			&CircleBrush, ESlateDrawEffect::None, Col);
	};
	TArray<FVector2D> P;
	auto Line = [&](const FLinearColor& Col, float W, int32 L)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + L, Geo, P, ESlateDrawEffect::None, Col, true, W);
	};

	// 1. 辉光 (叠层近似径向衰减)
	// 只给一圈紧贴徽章的极淡内晕; 大范围辉光交给 3D 的 M_Route_Halo (真径向 shader + bloom, 叠圆会出台阶边)
	if (GlowAmt > 0.01f)
	{
		Disc(R + 5.0f * Sc, WithAlpha(Soft, GlowAmt * 0.05f), 0);
		Disc(R + 2.5f * Sc, WithAlpha(Soft, GlowAmt * 0.06f), 0);
	}

	// 2. 暗盘 + 内部柔光
	Disc(R, Fill, 1);
	if (InnerA > 0.0f)
	{
		Disc(R * 0.86f, WithAlpha(Soft, InnerA * 0.5f), 1);
		Disc(R * 0.62f, WithAlpha(Soft, InnerA * 0.5f), 1);
	}

	// 3. 主环 + 内发丝环
	RouteVis::MakeCircle(P, C, R, 64);
	if (Visual == ERouteNodeVisual::Available || Visual == ERouteNodeVisual::Current)
	{
		Line(WithAlpha(Ring, 0.16f), RingW + 5.0f, 2);
	}
	Line(Ring, RingW, 3);
	if (Visual != ERouteNodeVisual::Locked || bReachable)
	{
		RouteVis::MakeCircle(P, C, R - 6.5f * Sc, 56);
		Line(WithAlpha(Ring, 0.28f), 1.0f, 3);
	}

	// 4. 类型轮廓
	RouteVis::MakeTypeShape(P, Type, C, R + 8.0f * Sc);
	Line(WithAlpha(Ring, ShapeA), (Visual == ERouteNodeVisual::Available || Visual == ERouteNodeVisual::Current) ? 1.6f : 1.1f, 3);

	// 5. 状态装饰
	if (Visual == ERouteNodeVisual::Current)
	{
		// 三段长弧缓慢同向旋转 (弧段点数给足, 低速旋转时不抖); 不再叠第二圈细碎虚线, 那种细碎线转动时会闪
		const float Spin = Time * 0.20f;
		for (int32 K = 0; K < 3; ++K)
		{
			const float A0 = Spin + K * (2.0f * PI / 3.0f);
			RouteVis::MakeArc(P, C, R + 13.0f * Sc, A0, A0 + 1.30f, 32);
			Line(WithAlpha(Ring, 0.55f), 1.6f, 3);
		}
	}	else if (Visual == ERouteNodeVisual::Visited)
	{
		const FVector2D K = C + FVector2D(R * 0.80f, -R * 0.80f);
		Disc(10.0f, WithAlpha(Gold, 0.92f), 4);
		P = { K + FVector2D(-4.0f, 0.5f), K + FVector2D(-1.2f, 3.4f), K + FVector2D(4.2f, -3.2f) };
		Line(FLinearColor(0.06f, 0.04f, 0.01f, 1.0f), 1.9f, 5);
	}

	// 6. 终点: 一圈外环 (不发光)
	if (bFinal)
	{
		const bool bLive = (Visual != ERouteNodeVisual::Locked);
		RouteVis::MakeCircle(P, C, R + 15.0f, 72);
		Line(WithAlpha(Ring, bLive ? 0.34f : 0.18f), 1.0f, 3);
	}

	return LayerId + 8;
}

// ---------------------------------------------------------------- 节点控件

void SRouteNodeWidget::Construct(const FArguments& InArgs)
{
	NodeId = InArgs._NodeId;
	Accent = InArgs._Accent;

	const FSlateFontInfo GlyphFont = FCoreStyle::GetDefaultFontStyle("Bold", InArgs._bFinal ? 28 : 23);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	constexpr float BadgeBox = 88.0f;

	ChildSlot
	[
		SAssignNew(Button, SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.ContentPadding(FMargin(0.0f))
		.OnClicked(InArgs._OnClicked)
		.ToolTipText(InArgs._Tooltip)
		[
			SNew(SBox)
			.WidthOverride(ERRouteLayout::NodeW)
			.HeightOverride(ERRouteLayout::NodeH)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, (ERRouteLayout::NodeH - BadgeBox) * 0.5f, 0.0f, 0.0f))
				[
					SNew(SBox)
					.WidthOverride(BadgeBox)
					.HeightOverride(BadgeBox)
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
						[
							SAssignNew(Sigil, SRouteNodeSigil)
							.Type(InArgs._NodeType)
							.Accent(InArgs._Accent)
							.bFinal(InArgs._bFinal)
							.Phase(InArgs._NodeId * 0.83f)
						]

						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SAssignNew(GlyphText, STextBlock)
							.Font(GlyphFont)
							.Justification(ETextJustify::Center)
							.ShadowOffset(FVector2D(0.0f, 1.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
							.Text(InArgs._Glyph)
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, InArgs._bFinal ? 10.0f : 5.0f, 0.0f, 0.0f))
				[
					SAssignNew(LabelText, STextBlock)
					.Font(LabelFont)
					.Justification(ETextJustify::Center)
					.ShadowOffset(FVector2D(0.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
					.Text(InArgs._Label)
				]
			]
		]
	];

	ApplyVisual();
}

void SRouteNodeWidget::SetVisual(ERouteNodeVisual InVisual, bool bInReachable)
{
	if (bVisualInit && Visual == InVisual && bReachable == bInReachable)
	{
		return;
	}

	bVisualInit = true;
	Visual = InVisual;
	bReachable = bInReachable;
	ApplyVisual();
}

void SRouteNodeWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (Sigil.IsValid() && Button.IsValid())
	{
		Sigil->Advance(InDeltaTime, Button->IsHovered(), Button->IsPressed());
	}
}

void SRouteNodeWidget::ApplyVisual()
{
	const FLinearColor Gold = ERRouteLayout::GoldColor();
	FLinearColor GlyphColor, LabelColor;

	switch (Visual)
	{
	case ERouteNodeVisual::Available:
		GlyphColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		LabelColor = FLinearColor(0.78f + Accent.R * 0.2f, 0.86f + Accent.G * 0.12f, 0.92f + Accent.B * 0.08f, 0.92f);
		break;
	case ERouteNodeVisual::Current:
		GlyphColor = FLinearColor(1.0f, 0.90f, 0.52f, 1.0f);
		LabelColor = FLinearColor(1.0f, 0.86f, 0.46f, 0.98f);
		break;
	case ERouteNodeVisual::Visited:
		GlyphColor = FLinearColor(Gold.R, Gold.G, Gold.B, 0.62f);
		LabelColor = FLinearColor(0.80f, 0.72f, 0.52f, 0.48f);
		break;
	default:
		GlyphColor = FLinearColor(0.74f, 0.82f, 0.94f, bReachable ? 0.66f : 0.30f);
		LabelColor = FLinearColor(0.70f, 0.78f, 0.90f, bReachable ? 0.46f : 0.22f);
		break;
	}

	if (Sigil.IsValid()) { Sigil->SetState(Visual, bReachable); }
	if (GlyphText.IsValid()) { GlyphText->SetColorAndOpacity(FSlateColor(GlyphColor)); }
	if (LabelText.IsValid()) { LabelText->SetColorAndOpacity(FSlateColor(LabelColor)); }
}



