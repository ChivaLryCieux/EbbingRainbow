// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/SBattleResultPanel.h"
#include "Battle/UI/SBattleGlassKit.h"

#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Rendering/SlateRenderTransform.h"

void SBattleResultPanel::Construct(const FArguments& InArgs)
{
	OnPrimaryClicked = InArgs._OnPrimaryClicked;

	// ---------------- 画刷 ----------------

	// 主面板: 比常规玻璃更实一点, 保证结算数字读得清
	BattleGlass::ConfigureGlassBrush(PanelBrush, BattleGlass::RadiusPanel,
		FLinearColor(0.030f, 0.045f, 0.072f, 0.94f), 1.2f, BattleGlass::HairlineHover);
	BattleGlass::ConfigureGlassBrush(PanelShadowBrush, BattleGlass::RadiusPanel + 8.0f,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.52f), 0.0f, FLinearColor::Transparent);
	BattleGlass::ConfigureGlassBrush(DividerBrush, 0.0f,
		FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), 0.0f, FLinearColor::Transparent);
	BattleGlass::ConfigureGlassBrush(BackdropBrush, 0.0f,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.42f), 0.0f, FLinearColor::Transparent);

	// 胜利: 金色主按钮
	BattleGlass::ConfigureGlassBrush(VictoryNormal, BattleGlass::RadiusPill,
		FLinearColor(0.72f, 0.52f, 0.12f, 0.94f), 1.4f, FLinearColor(1.0f, 0.88f, 0.45f, 0.85f));
	BattleGlass::ConfigureGlassBrush(VictoryHover, BattleGlass::RadiusPill,
		FLinearColor(0.88f, 0.66f, 0.16f, 0.97f), 2.0f, FLinearColor(1.0f, 0.97f, 0.66f, 1.0f));
	BattleGlass::ConfigureGlassBrush(VictoryPressed, BattleGlass::RadiusPill,
		FLinearColor(0.58f, 0.40f, 0.08f, 1.0f), 1.4f, FLinearColor(0.95f, 0.84f, 0.35f, 0.9f));
	VictoryBtnStyle.SetNormal(VictoryNormal);
	VictoryBtnStyle.SetHovered(VictoryHover);
	VictoryBtnStyle.SetPressed(VictoryPressed);

	// 失败: 暗红主按钮
	BattleGlass::ConfigureGlassBrush(DefeatNormal, BattleGlass::RadiusPill,
		FLinearColor(0.46f, 0.14f, 0.13f, 0.94f), 1.4f, FLinearColor(1.0f, 0.48f, 0.40f, 0.80f));
	BattleGlass::ConfigureGlassBrush(DefeatHover, BattleGlass::RadiusPill,
		FLinearColor(0.60f, 0.19f, 0.17f, 0.97f), 2.0f, FLinearColor(1.0f, 0.62f, 0.55f, 1.0f));
	BattleGlass::ConfigureGlassBrush(DefeatPressed, BattleGlass::RadiusPill,
		FLinearColor(0.34f, 0.10f, 0.10f, 1.0f), 1.4f, FLinearColor(0.92f, 0.42f, 0.36f, 0.9f));
	DefeatBtnStyle.SetNormal(DefeatNormal);
	DefeatBtnStyle.SetHovered(DefeatHover);
	DefeatBtnStyle.SetPressed(DefeatPressed);

	// ---------------- 布局 ----------------

	ChildSlot
	[
		SNew(SOverlay)

		// 全屏遮罩: 把注意力压到中间, 同时挡住场景的视觉干扰
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&BackdropBrush)
			.Padding(FMargin(0.0f))
		]

		// 中央面板
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(RootBorder, SBorder)
			.BorderImage(&PanelShadowBrush)
			.Padding(FMargin(7.0f))
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(FMargin(54.0f, 28.0f, 54.0f, 30.0f))
				[
					SNew(SBox)
					.WidthOverride(520.0f)
					[
						SNew(SVerticalBox)

						// 小标题
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SAssignNew(KickerText, STextBlock)
							.Text(FText::FromString(TEXT("战 斗 结 算   ·   B A T T L E   R E S U L T")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(BattleGlass::TextSecondary)
						]

						// 主标题: 胜利 / 失败
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 14.0f, 0.0f, 0.0f)
						[
							SAssignNew(TitleText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))
							.Justification(ETextJustify::Center)
							.ColorAndOpacity(BattleGlass::AccentGold)
						]

						// 副标题: 一句话说明
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SAssignNew(SubtitleText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
							.Justification(ETextJustify::Center)
							.ColorAndOpacity(BattleGlass::TextSecondary)
						]

						// 分隔线
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 20.0f, 0.0f, 10.0f)
						[
							SNew(SBox)
							.HeightOverride(1.0f)
							[
								SNew(SBorder)
								.BorderImage(&DividerBrush)
								.Padding(FMargin(0.0f))
							]
						]

						// 得失明细 (可扩展)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(RowsBox, SVerticalBox)
						]

						// 主按钮
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 26.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(300.0f)
							.HeightOverride(50.0f)
							[
								SAssignNew(PrimaryButton, SButton)
								.ButtonStyle(&VictoryBtnStyle)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.ContentPadding(FMargin(0.0f))
								.OnClicked_Lambda([this]()
								{
									OnPrimaryClicked.ExecuteIfBound();
									return FReply::Handled();
								})
								[
									SAssignNew(PrimaryButtonText, STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
									.Justification(ETextJustify::Center)
									.ColorAndOpacity(FLinearColor(0.10f, 0.08f, 0.02f))
								]
							]
						]
					]
				]
			]
		]
	];

	SetVisibility(EVisibility::Collapsed);
}

void SBattleResultPanel::SetResult(bool bInVictory, const FText& InTitle, const FText& InSubtitle,
	const TArray<FBattleResultRow>& InRows, const FText& InButtonText)
{
	bVictory = bInVictory;
	Entrance = 0.0f;

	if (TitleText.IsValid())
	{
		TitleText->SetText(InTitle);
		TitleText->SetColorAndOpacity(bVictory ? BattleGlass::AccentGold : BattleGlass::AccentEnemy);
	}
	if (SubtitleText.IsValid())
	{
		SubtitleText->SetText(InSubtitle);
	}
	if (PrimaryButton.IsValid())
	{
		PrimaryButton->SetButtonStyle(bVictory ? &VictoryBtnStyle : &DefeatBtnStyle);
	}
	if (PrimaryButtonText.IsValid())
	{
		PrimaryButtonText->SetText(InButtonText);
	}

	if (RowsBox.IsValid())
	{
		RowsBox->ClearChildren();
		for (const FBattleResultRow& Row : InRows)
		{
			RowsBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Row.Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					.ColorAndOpacity(BattleGlass::TextSecondary)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Row.Value)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					.ColorAndOpacity(Row.ValueColor)
				]
			];
		}
	}

	SetVisibility(EVisibility::Visible);
	SetRenderOpacity(0.0f);
	bLoggedGeometry = false;
}

void SBattleResultPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (GetVisibility() != EVisibility::Visible || Entrance >= 1.0f)
	{
		return;
	}

	// 淡入 + 轻微上浮
	Entrance = FMath::Min(Entrance + InDeltaTime / 0.30f, 1.0f);
	const float Eased = BattleGlass::EaseOutCubic(Entrance);
	SetRenderOpacity(Eased);

	if (RootBorder.IsValid())
	{
		RootBorder->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, (1.0f - Eased) * 20.0f)));
	}

	// 入场结束打一次布局日志: 确认面板真的以正常尺寸落在屏幕中央
	if (Entrance >= 1.0f && !bLoggedGeometry)
	{
		bLoggedGeometry = true;
		const FGeometry& Geo = GetCachedGeometry();
		const FVector2D AbsPos = Geo.GetAbsolutePosition();
		const FVector2D AbsSize = Geo.GetAbsoluteSize();
		UE_LOG(LogTemp, Log, TEXT("[BattleUI] 结算面板布局: 屏幕=(%.0f, %.0f) 尺寸=%.0fx%.0f"),
			AbsPos.X, AbsPos.Y, AbsSize.X, AbsSize.Y);
	}
}
