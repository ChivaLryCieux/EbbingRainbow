// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/UI/HallGlassButton.h"

#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Rendering/SlateRenderTransform.h"

void SHallGlassButton::Construct(const FArguments& InArgs)
{
	AccentColor = InArgs._AccentColor;
	bPrimary = InArgs._bPrimary;

	// ---- 液态玻璃画刷 ----
	// 光晕层: 主按钮专属, 大圆角弥散色块, Alpha 每帧动画
	GlowBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	GlowBrush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	GlowBrush.OutlineSettings.CornerRadii = FVector4(34.0f, 34.0f, 34.0f, 34.0f);
	GlowBrush.OutlineSettings.Width = 0.0f;
	GlowBrush.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);

	// 主体: 烟熏玻璃胶囊 + 白色发丝描边 (颜色/透明度每帧动画)
	BodyBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	BodyBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.075f, 0.11f, 0.40f));
	BodyBrush.OutlineSettings.CornerRadii = FVector4(27.0f, 27.0f, 27.0f, 27.0f);
	BodyBrush.OutlineSettings.Width = 1.1f;
	BodyBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.90f, 0.95f, 1.0f, 0.20f));

	// 状态指示灯: 9x9 圆点
	LedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	LedBrush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	LedBrush.OutlineSettings.CornerRadii = FVector4(5.0f, 5.0f, 5.0f, 5.0f);
	LedBrush.OutlineSettings.Width = 1.2f;
	LedBrush.OutlineSettings.Color = FSlateColor(AccentColor.CopyWithNewOpacity(0.45f));

	ChildSlot
	[
		SAssignNew(RootButton, SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.OnClicked(InArgs._OnClicked)
		.Cursor(EMouseCursor::Hand)
		[
			SNew(SBox)
			.WidthOverride(InArgs._ButtonWidth)
			[
				// 光晕层 (向外扩张的柔和大圆角色块)
				SAssignNew(GlowBorder, SBorder)
				.BorderImage(&GlowBrush)
				.Padding(FMargin(11.0f, 10.0f))
				[
					// 玻璃主体
					SAssignNew(BodyBorder, SBorder)
					.BorderImage(&BodyBrush)
					.Padding(FMargin(20.0f, 12.0f))
					[
						SNew(SHorizontalBox)

						// 左侧弹性留白 (内容整体居中)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SBox)
						]

						// 状态指示灯
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(9.0f)
							.HeightOverride(9.0f)
							[
								SAssignNew(LedBorder, SBorder)
								.BorderImage(&LedBrush)
								.Padding(0.0f)
							]
						]

						// 主标签 + 字距拉开的英文小注
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(13.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(InArgs._Label)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
								.ColorAndOpacity(FLinearColor(0.96f, 0.98f, 1.0f, 0.96f))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(InArgs._Caption)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FLinearColor(0.78f, 0.85f, 0.95f, 0.58f))
							]
						]

						// 右侧弹性留白
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SBox)
						]
					]
				]
			]
		]
	];
}

void SHallGlassButton::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	RefreshVisuals(InCurrentTime, InDeltaTime);
}

void SHallGlassButton::RefreshVisuals(double CurrentTime, float DeltaTime)
{
	const bool bHovered = RootButton.IsValid() && RootButton->IsHovered();
	const bool bPressed = RootButton.IsValid() && RootButton->IsPressed();

	HoverAlpha = FMath::FInterpTo(HoverAlpha, bHovered ? 1.0f : 0.0f, DeltaTime, 14.0f);
	PressAlpha = FMath::FInterpTo(PressAlpha, bPressed ? 1.0f : 0.0f, DeltaTime, 22.0f);

	// 主按钮光晕缓慢呼吸
	const float Breath = bPrimary ? (0.5f + 0.5f * FMath::Sin(static_cast<float>(CurrentTime) * 1.6f)) : 0.0f;

	// ---- 光晕 ----
	if (bPrimary)
	{
		const float GlowAlpha = 0.05f + 0.035f * Breath + 0.13f * HoverAlpha;
		GlowBrush.TintColor = FSlateColor(AccentColor.CopyWithNewOpacity(GlowAlpha));
	}

	// ---- 玻璃主体: 烟熏底色 → 悬停提亮 (主按钮再向主题色偏移) ----
	const FLinearColor BaseGlass(0.05f, 0.075f, 0.11f, 0.40f);
	const FLinearColor HoverGlass(0.09f, 0.135f, 0.20f, 0.55f);
	FLinearColor Glass = FMath::Lerp(BaseGlass, HoverGlass, HoverAlpha);
	if (bPrimary)
	{
		const FLinearColor AccentGlass(
			FMath::Lerp(0.09f, 0.13f, Breath),
			FMath::Lerp(0.20f, 0.26f, Breath),
			0.34f,
			0.55f);
		Glass = FMath::Lerp(Glass, AccentGlass, 0.40f * HoverAlpha);
	}
	BodyBrush.TintColor = FSlateColor(Glass);

	// 发丝描边: 悬停时增强
	const float OutlineAlpha = 0.20f + 0.30f * HoverAlpha;
	BodyBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.90f, 0.95f, 1.0f, OutlineAlpha));

	// ---- 指示灯: 主按钮填充呼吸点亮, 次按钮仅描边 ----
	if (bPrimary)
	{
		const float LedAlpha = FMath::Clamp(0.45f + 0.55f * Breath + 0.20f * HoverAlpha, 0.0f, 1.0f);
		LedBrush.TintColor = FSlateColor(AccentColor.CopyWithNewOpacity(LedAlpha));
		LedBrush.OutlineSettings.Color = FSlateColor(AccentColor.CopyWithNewOpacity(0.85f));
	}
	else
	{
		LedBrush.OutlineSettings.Color = FSlateColor(AccentColor.CopyWithNewOpacity(0.45f + 0.35f * HoverAlpha));
	}

	// ---- 按压缩放 (液态回弹手感) ----
	if (GlowBorder.IsValid())
	{
		const float Scale = 1.0f - 0.03f * PressAlpha;
		GlowBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		GlowBorder->SetRenderTransform(FSlateRenderTransform(FScale2D(Scale)));
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}
