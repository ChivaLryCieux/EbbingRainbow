// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/UI/SRouteNodeWidget.h"

#include "Battle/UI/SBattleGlassKit.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SRouteNodeWidget::Construct(const FArguments& InArgs)
{
	NodeId = InArgs._NodeId;
	Accent = InArgs._Accent;

	const FSlateFontInfo GlyphFont = FCoreStyle::GetDefaultFontStyle("Bold", 30);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	ChildSlot
	[
		SAssignNew(Button, SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.ContentPadding(FMargin(0.0f))
		.OnClicked(InArgs._OnClicked)
		.ToolTipText(InArgs._Tooltip)
		[
			SNew(SBox)
			.WidthOverride(100.0f)
			.HeightOverride(112.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(88.0f)
					.HeightOverride(88.0f)
					[
						SAssignNew(Badge, SBorder)
						.BorderImage(&BadgeBrush)
						.Padding(FMargin(0.0f))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SAssignNew(GlyphText, STextBlock)
							.Font(GlyphFont)
							.Justification(ETextJustify::Center)
							.Text(InArgs._Glyph)
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
				[
					SAssignNew(LabelText, STextBlock)
					.Font(LabelFont)
					.Justification(ETextJustify::Center)
					.Text(InArgs._Label)
				]
			]
		]
	];

	ApplyVisual();
}

void SRouteNodeWidget::SetVisual(ERouteNodeVisual InVisual)
{
	if (Visual == InVisual)
	{
		return;
	}

	Visual = InVisual;
	ApplyVisual();
}

void SRouteNodeWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (Visual == ERouteNodeVisual::Available)
	{
		PulseTime += InDeltaTime;
		const float Wave = 0.5f + 0.5f * FMath::Sin(PulseTime * 3.0f);
		SetRenderOpacity(0.86f + 0.14f * Wave);
	}
}

void SRouteNodeWidget::ApplyVisual()
{
	FLinearColor Fill;
	FLinearColor Outline;
	FLinearColor GlyphColor;
	FLinearColor LabelColor;
	float OutlineWidth = 1.2f;
	float Opacity = 1.0f;

	switch (Visual)
	{
	case ERouteNodeVisual::Available:
		Fill = FLinearColor(0.080f, 0.200f, 0.300f, 0.88f);
		Outline = FLinearColor(Accent.R, Accent.G, Accent.B, 0.95f);
		GlyphColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		LabelColor = FLinearColor(0.85f, 0.93f, 1.0f, 0.85f);
		OutlineWidth = 2.0f;
		break;

	case ERouteNodeVisual::Current:
		Fill = FLinearColor(0.160f, 0.130f, 0.050f, 0.92f);
		Outline = FLinearColor(1.0f, 0.82f, 0.28f, 1.0f);
		GlyphColor = FLinearColor(1.0f, 0.88f, 0.45f, 1.0f);
		LabelColor = FLinearColor(1.0f, 0.85f, 0.40f, 0.95f);
		OutlineWidth = 2.4f;
		break;

	case ERouteNodeVisual::Visited:
		Fill = FLinearColor(0.050f, 0.090f, 0.120f, 0.72f);
		Outline = FLinearColor(1.0f, 0.82f, 0.28f, 0.40f);
		GlyphColor = FLinearColor(0.90f, 0.80f, 0.55f, 0.80f);
		LabelColor = FLinearColor(0.75f, 0.80f, 0.85f, 0.50f);
		break;

	case ERouteNodeVisual::Locked:
	default:
		// 未解锁也要看得见，否则玩家无法规划路线；只是压低对比度。
		Fill = FLinearColor(0.055f, 0.080f, 0.120f, 0.80f);
		Outline = FLinearColor(0.62f, 0.72f, 0.88f, 0.38f);
		GlyphColor = FLinearColor(0.72f, 0.80f, 0.92f, 0.62f);
		LabelColor = FLinearColor(0.70f, 0.78f, 0.90f, 0.42f);
		Opacity = 0.9f;
		break;
	}

	BattleGlass::ConfigureGlassBrush(BadgeBrush, 44.0f, Fill, OutlineWidth, Outline);
	if (Badge.IsValid())
	{
		Badge->SetBorderImage(&BadgeBrush);
	}
	if (GlyphText.IsValid())
	{
		GlyphText->SetColorAndOpacity(FSlateColor(GlyphColor));
	}
	if (LabelText.IsValid())
	{
		LabelText->SetColorAndOpacity(FSlateColor(LabelColor));
	}
	SetRenderOpacity(Opacity);
}
