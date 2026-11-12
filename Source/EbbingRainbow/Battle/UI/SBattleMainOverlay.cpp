// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/SBattleMainOverlay.h"
#include "Core/ERLog.h"
#include "Battle/UI/SBattleUnitPortrait.h"
#include "Battle/UI/SBattleGlassKit.h"
#include "Battle/UI/SBattleResultPanel.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/Units/BattleUnit.h"

#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Brushes/SlateImageBrush.h"
#include "Rendering/SlateRenderTransform.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"

// =========================================================================================
// SBattleSkillCardWidget
// =========================================================================================

void SBattleSkillCardWidget::Construct(const FArguments& InArgs)
{
	CardIndex = InArgs._CardIndex;
	APCost = InArgs._APCost;
	bIsMoveCard = InArgs._bIsMoveCard;
	AccentColor = InArgs._AccentColor;
	OnClickedDelegate = InArgs._OnClicked;
	CooldownText = InArgs._CooldownBadge;

	// 卡牌玻璃底: 乳白磨砂 + 主题色发丝描边 (白底上主题色压深一档保证可读)
	const FLinearColor DeepAccent = BattleGlass::DeepenForWhite(AccentColor, 0.70f);
	BattleGlass::ConfigureGlassBrush(CardBgBrush, BattleGlass::RadiusCard,
		FLinearColor(0.97f, 0.98f, 1.00f, 0.62f), 1.2f, DeepAccent.CopyWithNewOpacity(0.65f));

	// AP 晶体徽章 (彩色药丸, 白字) + 冷却角标 (深色小药丸)
	BattleGlass::ConfigureGlassBrush(CostGemBrush, BattleGlass::RadiusPill,
		bIsMoveCard ? FLinearColor(0.10f, 0.55f, 0.35f, 0.92f) : FLinearColor(0.12f, 0.42f, 0.90f, 0.92f),
		1.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.45f));
	BattleGlass::ConfigureGlassBrush(CooldownBrush, BattleGlass::RadiusPill,
		FLinearColor(0.10f, 0.11f, 0.16f, 0.88f), 0.8f, FLinearColor(1.0f, 1.0f, 1.0f, 0.30f));

	// 技能图标画刷 (正方形渲染, 固定宽高杜绝拉伸); 纹理由 UBattleSkillData::Icon (UPROPERTY) 持有
	if (IsValid(InArgs._IconTexture))
	{
		IconBrush = MakeShared<FSlateImageBrush>(InArgs._IconTexture, FVector2D(64.0f, 64.0f));
	}

	// 详细说明只在悬停提示里出现
	if (!InArgs._ToolTip.IsEmpty())
	{
		SetToolTipText(InArgs._ToolTip);
	}

	const FText CostBadgeText = bIsMoveCard ? FText::FromString(TEXT("移")) : FText::FromString(FString::Printf(TEXT("%.0f"), APCost));

	ChildSlot
	[
		SAssignNew(BorderWidget, SBorder)
		.BorderImage(&CardBgBrush)
		.Padding(FMargin(6.0f, 6.0f))
		[
			SNew(SBox)
			.WidthOverride(72.0f)
			.HeightOverride(84.0f)
			[
				SNew(SVerticalBox)

				// 顶部行: 左侧 AP 费用徽章, 右侧快捷键提示
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.BorderImage(&CostGemBrush)
						.Padding(FMargin(6.0f, 1.5f))
						[
							SNew(STextBlock)
							.Text(CostBadgeText)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(InArgs._HotkeyText)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(BattleGlass::AccentGold)
					]
				]

				// 图标区: 正方形镶嵌; 无图标 (移动卡) 则以标题字符为视觉主体
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 3.0f)
				[
					SNew(SBox)
					.WidthOverride(52.0f)
					.HeightOverride(52.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image(IconBrush.Get())
							.ColorAndOpacity_Lambda([this]() {
								return bCanAfford ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);
							})
						]
						+ SOverlay::Slot()
						[
							SNew(STextBlock)
							.Visibility(IconBrush.IsValid() ? EVisibility::Collapsed : EVisibility::Visible)
							.Text(InArgs._Title)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
							.Justification(ETextJustify::Center)
							.ColorAndOpacity(DeepAccent)
						]
						// 冷却角标: 图标右下角深色小药丸
						+ SOverlay::Slot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Bottom)
						.Padding(0.0f, 0.0f, -3.0f, -3.0f)
						[
							SNew(SBorder)
							.Visibility(CooldownText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							.BorderImage(&CooldownBrush)
							.Padding(FMargin(5.0f, 1.0f))
							[
								SNew(STextBlock)
								.Text(CooldownText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]
				]

			]
		]
	];

	SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
}

void SBattleSkillCardWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 悬停标志以 Slate 权威命中为准, 防 Enter/Leave 不配对时卡死
	// (Leave 丢失 -> bMouseOverUI 恒真 -> 战场点击全被吞)
	bHovered = IsHovered();

	// 平滑悬停上浮与缩放插值
	const float TargetOffset = (bHovered || bIsSelected) ? -14.0f : 0.0f;
	const float TargetScale = (bHovered || bIsSelected) ? 1.06f : 1.0f;

	HoverOffset = FMath::FInterpTo(HoverOffset, TargetOffset, InDeltaTime, 14.0f);
	CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, InDeltaTime, 14.0f);

	SetRenderTransform(FSlateRenderTransform(CurrentScale, FVector2D(0.0f, HoverOffset)));

	// 玻璃描边随状态增强 (白底: 选中金边 / 悬停压深主题色 / 常态主题色)
	const FLinearColor DeepAccent = BattleGlass::DeepenForWhite(AccentColor, 0.70f);
	if (bIsSelected)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(BattleGlass::AccentGold);
		CardBgBrush.OutlineSettings.Width = 2.6f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.88f));
	}
	else if (bHovered)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(BattleGlass::AccentGold.CopyWithNewOpacity(0.90f));
		CardBgBrush.OutlineSettings.Width = 2.0f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.76f));
	}
	else if (!bCanAfford)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.45f, 0.48f, 0.54f, 0.40f));
		CardBgBrush.OutlineSettings.Width = 1.0f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(0.90f, 0.91f, 0.94f, 0.50f));
	}
	else
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(DeepAccent.CopyWithNewOpacity(0.65f));
		CardBgBrush.OutlineSettings.Width = 1.2f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(0.97f, 0.98f, 1.00f, 0.62f));
	}
}

void SBattleSkillCardWidget::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	bHovered = true;
}

void SBattleSkillCardWidget::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bHovered = false;
}

FReply SBattleSkillCardWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (OnClickedDelegate.IsBound())
		{
			return OnClickedDelegate.Execute();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// =========================================================================================
// SBattleMainOverlay
// =========================================================================================

void SBattleMainOverlay::Construct(const FArguments& InArgs)
{
	BattleManager = InArgs._BattleManager;
	PlayerController = InArgs._PlayerController;

	// ---------------- 玻璃画刷 ----------------

	// 大面板: 先攻栏 / 英雄板 / 结算横幅
	BattleGlass::ConfigureGlassBrush(PanelBrush, BattleGlass::RadiusPanel,
		BattleGlass::GlassFill, 1.1f, BattleGlass::Hairline);

	// 中等卡片: 先攻芯片
	BattleGlass::ConfigureGlassBrush(CardBrush, BattleGlass::RadiusChip,
		FLinearColor(0.97f, 0.98f, 1.00f, 0.64f), 1.2f, BattleGlass::Hairline);

	// 战报通知条
	BattleGlass::ConfigureGlassBrush(NotificationBrush, BattleGlass::RadiusPill,
		FLinearColor(0.97f, 0.98f, 1.00f, 0.72f), 1.1f, BattleGlass::AccentGold.CopyWithNewOpacity(0.55f));

	// 金色"结束回合"玻璃主按钮 (带呼吸光晕)
	BattleGlass::ConfigureGlassBrush(EndTurnNormalBrush, BattleGlass::RadiusPill,
		FLinearColor(0.72f, 0.52f, 0.12f, 0.92f), 1.4f, FLinearColor(1.0f, 0.88f, 0.45f, 0.85f));
	BattleGlass::ConfigureGlassBrush(EndTurnHoverBrush, BattleGlass::RadiusPill,
		FLinearColor(0.88f, 0.66f, 0.16f, 0.97f), 2.0f, FLinearColor(1.0f, 0.97f, 0.66f, 1.0f));
	BattleGlass::ConfigureGlassBrush(EndTurnPressedBrush, BattleGlass::RadiusPill,
		FLinearColor(0.58f, 0.40f, 0.08f, 1.0f), 1.4f, FLinearColor(0.95f, 0.84f, 0.35f, 0.9f));
	BattleGlass::ConfigureGlassBrush(EndTurnDisabledBrush, BattleGlass::RadiusPill,
		FLinearColor(0.16f, 0.17f, 0.20f, 0.55f), 1.0f, FLinearColor(0.40f, 0.42f, 0.47f, 0.45f));
	BattleGlass::ConfigureGlassBrush(EndTurnGlowBrush, BattleGlass::RadiusPill + 5.0f,
		FLinearColor(0, 0, 0, 0), 0.0f, FLinearColor::Transparent);

	EndTurnBtnStyle.SetNormal(EndTurnNormalBrush);
	EndTurnBtnStyle.SetHovered(EndTurnHoverBrush);
	EndTurnBtnStyle.SetPressed(EndTurnPressedBrush);
	EndTurnBtnStyle.SetDisabled(EndTurnDisabledBrush);

	// 血条
	BattleGlass::ConfigureGlassBrush(HPTrackBrush, 4.0f,
		BattleGlass::HealthTrack, 0.0f, FLinearColor::Transparent);
	BattleGlass::ConfigureGlassBrush(HPFillBrush, 4.0f,
		BattleGlass::HealthHigh, 0.0f, FLinearColor::Transparent);

	// 作弊按钮: 淡红玻璃小胶囊, 刻意做得低调 (不抢战术 UI 的视觉重心)
	BattleGlass::ConfigureGlassBrush(CheatNormalBrush, BattleGlass::RadiusPill,
		FLinearColor(0.98f, 0.90f, 0.88f, 0.62f), 1.0f, FLinearColor(0.78f, 0.30f, 0.24f, 0.45f));
	BattleGlass::ConfigureGlassBrush(CheatHoverBrush, BattleGlass::RadiusPill,
		FLinearColor(1.00f, 0.87f, 0.84f, 0.80f), 1.4f, FLinearColor(0.82f, 0.28f, 0.22f, 0.75f));
	BattleGlass::ConfigureGlassBrush(CheatPressedBrush, BattleGlass::RadiusPill,
		FLinearColor(0.94f, 0.78f, 0.74f, 0.90f), 1.0f, FLinearColor(0.80f, 0.32f, 0.26f, 0.60f));
	CheatBtnStyle.SetNormal(CheatNormalBrush);
	CheatBtnStyle.SetHovered(CheatHoverBrush);
	CheatBtnStyle.SetPressed(CheatPressedBrush);

	// 面板柔影: 比面板大 5px 的深色圆角, 让玻璃从明亮场景里"浮"起来
	BattleGlass::ConfigureGlassBrush(PanelShadowBrush, BattleGlass::RadiusPanel + 5.0f,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.34f), 0.0f, FLinearColor::Transparent);

	ChildSlot
	[
		SNew(SOverlay)

		// =============================================================
		// 1. 顶部先攻队列栏
		// =============================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 16.0f, 0.0f, 0.0f)
		[
			SAssignNew(TopPanelBorder, SBorder)
			.BorderImage(&PanelShadowBrush)
			.Padding(FMargin(5.0f))
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(FMargin(18.0f, 8.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("先 攻 序 列   ·   I N I T I A T I V E")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(BattleGlass::TextSecondary)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(InitiativeHBox, SHorizontalBox)
					]
				]
			]
		]

		// =============================================================
		// 2. 动态战报提示条
		// =============================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 104.0f, 0.0f, 0.0f)
		[
			SAssignNew(NotificationBorder, SBorder)
			.BorderImage(&NotificationBrush)
			.Padding(FMargin(18.0f, 6.0f))
			.Visibility(EVisibility::Hidden)
			[
				SAssignNew(NotificationTextBlock, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(BattleGlass::AccentGold)
			]
		]

		// =============================================================
		// 3. 左侧我方编队名牌面板 (头像 / 名字 / 血量, 可点击聚焦)
		// =============================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(30.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)

			// 面板标题
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 10.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("我 方 编 队   ·   S Q U A D")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(BattleGlass::TextSecondary)
			]

			// 名牌列表
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(RosterBox, SVerticalBox)
			]
		]

		// =============================================================
		// 4. 底部主行动面板 (英雄板 + 手牌 + 结束回合)
		//    注意: 用 HAlign_Center + 全 AutoWidth, 让整条操作栏按内容宽度居中。
		//    PIE 内嵌视口的 Slate 布局宽度可能大于可见区域, 若用 HAlign_Fill +
		//    FillWidth 手牌槽, "结束回合"会被推到屏幕外沿之外而看不见。
		// =============================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 0.0f, 22.0f)
		[
			SAssignNew(BottomBarBox, SHorizontalBox)

			// 底部左侧: 活跃英雄状态板
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Bottom)
			[
				SNew(SBorder)
				.BorderImage(&PanelShadowBrush)
				.Padding(FMargin(5.0f))
				[
					SNew(SBorder)
					.BorderImage(&PanelBrush)
					.Padding(FMargin(16.0f, 12.0f, 18.0f, 13.0f))
					[
					SNew(SBox)
					.WidthOverride(232.0f)
					[
						SNew(SVerticalBox)

						// 英雄名称与阵营
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 8.0f)
						[
							SAssignNew(ActiveHeroNameText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
							.ColorAndOpacity(BattleGlass::TextPrimary)
						]

						// HP 血条与数值
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 5.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SAssignNew(ActiveHeroHPText, STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(BattleGlass::HealthHigh)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(SBox)
								.HeightOverride(7.0f)
								[
									SAssignNew(ActiveHeroHPBar, SProgressBar)
									.Percent(1.0f)
									.BackgroundImage(&HPTrackBrush)
									.FillImage(&HPFillBrush)
								]
							]
						]

						// AP 能量槽展示
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 7.0f, 0.0f, 0.0f)
						[
							SAssignNew(ActiveHeroAPText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(BattleGlass::AccentPlayer)
						]
					]
				]
			]
		]

			// 底部中央: 战术卡牌手牌区 (AutoWidth, 不抢占剩余空间)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(15.0f, 0.0f)
			[
				SAssignNew(HandCardsBox, SHorizontalBox)
			]

			// 底部右侧: 金色"结束回合"玻璃按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SBox)
				.WidthOverride(158.0f)
				.HeightOverride(72.0f)
				[
					SAssignNew(EndTurnGlowBorder, SBorder)
					.BorderImage(&EndTurnGlowBrush)
					.Padding(0.0f)
					[
						SAssignNew(EndTurnButton, SButton)
						.ButtonStyle(&EndTurnBtnStyle)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SBattleMainOverlay::HandleEndTurnClicked)
						[
							SAssignNew(EndTurnButtonText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.Justification(ETextJustify::Center)
							.ColorAndOpacity(FLinearColor(0.10f, 0.08f, 0.02f))
						]
					]
				]
			]
		]

		// =============================================================
		// 4b. 右上角 DEBUG 作弊按钮 (仅非 Shipping 构建可见)
		// =============================================================
#if !UE_BUILD_SHIPPING
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 18.0f, 20.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(170.0f)
			.HeightOverride(34.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(CheatButton, SButton)
				.ButtonStyle(&CheatBtnStyle)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(18.0f, 0.0f))
				.OnClicked(this, &SBattleMainOverlay::HandleCheatKillClicked)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("DEBUG · 秒杀敌人")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FLinearColor(0.72f, 0.20f, 0.15f, 0.92f))
				]
			]
		]
#endif

		// =============================================================
		// 5. 战斗结算弹窗 (胜利/失败 + 返回按钮, 自带全屏遮罩与入场动画)
		// =============================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(ResultPanel, SBattleResultPanel)
			.OnPrimaryClicked(FSimpleDelegate::CreateSP(this, &SBattleMainOverlay::HandleResultPrimaryClicked))
		]
	];

	RebuildRosterPanel();
}

void SBattleMainOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 全局入场动画: 顶栏下沉 / 底栏上浮 / 名牌左滑 (三向汇入, ease-out cubic)
	if (EntranceProgress < 1.0f)
	{
		EntranceProgress = FMath::Min(EntranceProgress + InDeltaTime / BattleGlass::EntranceDuration, 1.0f);

		const float Eased = BattleGlass::EaseOutCubic(EntranceProgress);
		if (TopPanelBorder.IsValid())
		{
			TopPanelBorder->SetRenderOpacity(Eased);
			TopPanelBorder->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, (1.0f - Eased) * -16.0f)));
		}
		if (BottomBarBox.IsValid())
		{
			BottomBarBox->SetRenderOpacity(Eased);
			BottomBarBox->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, (1.0f - Eased) * 26.0f)));
		}
	}

	UBattleManager* Mgr = BattleManager.Get();
	ABattlePlayerController* PC = PlayerController.Get();
	if (!Mgr)
	{
		return;
	}

	ABattleUnit* CurrentActive = Mgr->GetActiveUnit();
	const EBattleState CurrentState = Mgr->GetBattleState();
	const EPlayerInteractionMode CurrentMode = PC ? PC->GetInteractionMode() : EPlayerInteractionMode::Idle;

	// 1. 先攻队列/单位存亡修订号比对 (修订号由管理器维护, 免每帧哈希+建数组)
	const int32 QueueRev = Mgr->GetInitiativeRevision();
	const int32 UnitsRev = Mgr->GetUnitsRevision();
	int32 IntegritySum = 0;
	for (ABattleUnit* U : Mgr->GetInitiativeQueue())
	{
		if (U) { IntegritySum += FMath::RoundToInt(U->Integrity); }
	}
	if (QueueRev != CachedQueueRevision || UnitsRev != CachedUnitsRevision
		|| IntegritySum != CachedIntegritySum || CurrentActive != CachedActiveUnit)
	{
		CachedQueueRevision = QueueRev;
		CachedIntegritySum = IntegritySum;
		RebuildInitiativeBar();
	}

	// 1b. 编队面板: 我方单位集合变化 (阵亡/增援) 时重建
	{
		if (UnitsRev != CachedUnitsRevision && RosterBox.IsValid())
		{
			CachedUnitsRevision = UnitsRev;
			CachedPlayerUnits.Reset();
			for (ABattleUnit* U : Mgr->GetAllUnits())
			{
				if (U && U->Team == EBattleTeam::Player)
				{
					CachedPlayerUnits.Add(U);
				}
			}
			RebuildRosterPanel();
		}

		// 同步每张名片的聚焦态 (摄像机可能被玩家手动操作解除)
		if (PC)
		{
			ABattleUnit* Focused = PC->GetFocusedUnit();
			for (const TSharedPtr<SBattleUnitPortrait>& Portrait : PortraitWidgets)
			{
				if (Portrait.IsValid())
				{
					Portrait->SetFocused(Portrait->GetUnit() == Focused);
				}
			}
		}
	}

	// 2. 检查活跃单位数值变更或切换, 触发手牌与属性刷新
	const float CurrAP = CurrentActive ? CurrentActive->AP : -1.0f;
	const int32 CurrHP = CurrentActive ? FMath::RoundToInt(CurrentActive->Integrity) : -1;

	if (CurrentActive != CachedActiveUnit || FMath::Abs(CurrAP - CachedAP) > 0.05f || CurrHP != CachedHP || CurrentMode != CachedMode)
	{
		CachedActiveUnit = CurrentActive;
		CachedAP = CurrAP;
		CachedHP = CurrHP;
		CachedMode = CurrentMode;

		UpdateHeroPlate(CurrentActive);
		RebuildHandCards(CurrentActive);
	}

	// 3. 刷新结束回合按钮状态与结算弹窗
	if (CurrentState != CachedState)
	{
		CachedState = CurrentState;
		UpdateResultPanel(CurrentState);
	}
	UpdateEndTurnButton(CurrentActive, CurrentState);

	// 4. 通知淡出计时器
	if (NotificationTimer > 0.0f)
	{
		NotificationTimer -= InDeltaTime;
		if (NotificationTimer <= 0.0f && NotificationBorder.IsValid())
		{
			NotificationBorder->SetVisibility(EVisibility::Hidden);
		}
	}

	// 5. 检查鼠标是否在任何名牌 / 手牌 / 结束按钮上
	bMouseOverUI = false;
	if (EndTurnButton.IsValid() && EndTurnButton->IsHovered())
	{
		bMouseOverUI = true;
	}
	if (CheatButton.IsValid() && CheatButton->IsHovered())
	{
		bMouseOverUI = true;
	}
	// 布局日志: 确认作弊按钮真的挂到了右上角 (只在首次有效布局时打一次)
	if (CheatButton.IsValid() && !bLoggedCheatGeometry)
	{
		const FVector2D AbsPos = CheatButton->GetCachedGeometry().GetAbsolutePosition();
		const FVector2D AbsSize = CheatButton->GetCachedGeometry().GetAbsoluteSize();
		if (AbsSize.X > 1.0f && AbsSize.Y > 1.0f)
		{
			bLoggedCheatGeometry = true;
			UE_LOG(LogERBattleUI, Log, TEXT("[BattleUI] 作弊按钮布局: 屏幕=(%.0f, %.0f) 尺寸=%.0fx%.0f"), AbsPos.X, AbsPos.Y, AbsSize.X, AbsSize.Y);
		}
	}
	// 结算弹窗盖住整个屏幕时, 任何点击都不该穿透到场景
	if (ResultPanel.IsValid() && ResultPanel->GetVisibility() == EVisibility::Visible)
	{
		bMouseOverUI = true;
	}
	for (const TSharedPtr<SBattleSkillCardWidget>& Card : CardWidgets)
	{
		if (Card.IsValid() && Card->IsHoveredState())
		{
			bMouseOverUI = true;
			break;
		}
	}
	if (!bMouseOverUI)
	{
		for (const TSharedPtr<SBattleUnitPortrait>& Portrait : PortraitWidgets)
		{
			if (Portrait.IsValid() && Portrait->IsHoveredState())
			{
				bMouseOverUI = true;
				break;
			}
		}
	}
}

bool SBattleMainOverlay::IsMouseOverInteractiveUI() const
{
	return bMouseOverUI;
}

void SBattleMainOverlay::ShowNotification(const FString& InText)
{
	if (NotificationTextBlock.IsValid() && NotificationBorder.IsValid())
	{
		NotificationTextBlock->SetText(FText::FromString(InText));
		NotificationBorder->SetVisibility(EVisibility::Visible);
		NotificationTimer = 4.0f;
	}
}

void SBattleMainOverlay::RebuildRosterPanel()
{
	if (!RosterBox.IsValid())
	{
		return;
	}

	RosterBox->ClearChildren();
	PortraitWidgets.Reset();
	CachedPlayerUnits.Reset();

	UBattleManager* Mgr = BattleManager.Get();
	if (!Mgr)
	{
		return;
	}

	int32 SlotIndex = 0;
	for (ABattleUnit* Unit : Mgr->GetAllUnits())
	{
		if (!Unit || Unit->Team != EBattleTeam::Player)
		{
			continue;
		}

		CachedPlayerUnits.Add(Unit);

		TSharedPtr<SBattleUnitPortrait> Portrait = SNew(SBattleUnitPortrait)
			.UnitIndex(SlotIndex)
			.CardWidth(258.0f)
			.EntranceDelay(SlotIndex * 0.09f)
			.OnClicked(this, &SBattleMainOverlay::HandlePortraitClicked, SlotIndex);

		Portrait->SetUnit(Unit);
		PortraitWidgets.Add(Portrait);

		RosterBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
		[
			Portrait.ToSharedRef()
		];

		++SlotIndex;
	}
}

void SBattleMainOverlay::RebuildInitiativeBar()
{
	if (!InitiativeHBox.IsValid())
	{
		return;
	}

	InitiativeHBox->ClearChildren();
	ChipBrushes.Reset();

	UBattleManager* Mgr = BattleManager.Get();
	if (!Mgr)
	{
		return;
	}

	const TArray<ABattleUnit*>& Queue = Mgr->GetInitiativeQueue();
	ABattleUnit* ActiveUnit = Mgr->GetActiveUnit();

	for (ABattleUnit* Unit : Queue)
	{
		if (!Unit)
		{
			continue;
		}

		const bool bIsActive = (Unit == ActiveUnit);
		const bool bIsPlayer = (Unit->Team == EBattleTeam::Player);
		const bool bIsAlive = Unit->IsAlive();

		// 单位外框颜色 (我方蓝, 敌方红, 活跃金, 阵亡灰)
		FLinearColor FrameColor;
		if (!bIsAlive)
		{
			FrameColor = FLinearColor(0.30f, 0.32f, 0.36f, 0.45f);
		}
		else if (bIsActive)
		{
			FrameColor = BattleGlass::AccentGold;
		}
		else if (bIsPlayer)
		{
			FrameColor = BattleGlass::AccentPlayer.CopyWithNewOpacity(0.80f);
		}
		else
		{
			FrameColor = BattleGlass::AccentEnemy.CopyWithNewOpacity(0.80f);
		}

		TSharedPtr<FSlateBrush> ChipBrush = MakeShared<FSlateBrush>();
		BattleGlass::ConfigureGlassBrush(*ChipBrush, BattleGlass::RadiusChip,
			bIsAlive ? FLinearColor(0.97f, 0.98f, 1.00f, 0.62f) : FLinearColor(0.82f, 0.84f, 0.88f, 0.50f),
			bIsActive ? 2.4f : 1.1f, FrameColor);
		ChipBrushes.Add(ChipBrush);

		const float HPPercent = (Unit->Stats.MaxIntegrity > 0.0f) ? (Unit->Integrity / Unit->Stats.MaxIntegrity) : 0.0f;
		const FString UnitLabel = bIsAlive ? FString::Printf(TEXT("%s\n先攻 %d"), *Unit->Stats.DisplayName, Unit->Stats.Initiative)
		                                   : FString::Printf(TEXT("%s\n已损毁"), *Unit->Stats.DisplayName);

		InitiativeHBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(5.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(ChipBrush.Get())
			.Padding(FMargin(10.0f, 5.0f))
			[
				SNew(SBox)
				.WidthOverride(84.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(UnitLabel))
						.Font(FCoreStyle::GetDefaultFontStyle(bIsActive ? "Bold" : "Regular", 10))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(bIsAlive ? BattleGlass::TextPrimary : BattleGlass::TextDead)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.HeightOverride(5.0f)
						[
							SNew(SProgressBar)
							.Percent(HPPercent)
							.BackgroundImage(&HPTrackBrush)
							.FillImage(&HPFillBrush)
							.FillColorAndOpacity(BattleGlass::HealthColorFor(HPPercent))
						]
					]
				]
			]
		];
	}
}

void SBattleMainOverlay::UpdateHeroPlate(ABattleUnit* Unit)
{
	if (!Unit || !Unit->IsAlive())
	{
		if (ActiveHeroNameText.IsValid()) ActiveHeroNameText->SetText(FText::FromString(TEXT("等待回合...")));
		if (ActiveHeroHPText.IsValid()) ActiveHeroHPText->SetText(FText::GetEmpty());
		if (ActiveHeroAPText.IsValid()) ActiveHeroAPText->SetText(FText::GetEmpty());
		if (ActiveHeroHPBar.IsValid()) ActiveHeroHPBar->SetPercent(0.0f);
		return;
	}

	const bool bIsPlayer = (Unit->Team == EBattleTeam::Player);
	const FString TeamStr = bIsPlayer ? TEXT("我方英雄") : TEXT("敌方目标");
	const FString NameFormatted = FString::Printf(TEXT("%s  ·  %s"), *TeamStr, *Unit->Stats.DisplayName);

	if (ActiveHeroNameText.IsValid())
	{
		ActiveHeroNameText->SetText(FText::FromString(NameFormatted));
		ActiveHeroNameText->SetColorAndOpacity(bIsPlayer ? BattleGlass::AccentPlayer : BattleGlass::AccentEnemy);
	}

	if (ActiveHeroHPText.IsValid())
	{
		if (Unit->Barrier > 0.0f)
		{
			const float MaxIntegrity = Unit->GetSkillComponent() ? Unit->GetSkillComponent()->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
			ActiveHeroHPText->SetText(FText::FromString(FString::Printf(TEXT("完整度  %.0f / %.0f   +%.0f 护盾"), Unit->Integrity, MaxIntegrity, Unit->Barrier)));
		}
		else
		{
			const float MaxIntegrity = Unit->GetSkillComponent() ? Unit->GetSkillComponent()->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
			ActiveHeroHPText->SetText(FText::FromString(FString::Printf(TEXT("完整度  %.0f / %.0f"), Unit->Integrity, MaxIntegrity)));
		}
	}

	if (ActiveHeroHPBar.IsValid())
	{
		const float MaxIntegrity = Unit->GetSkillComponent() ? Unit->GetSkillComponent()->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
		const float Pct = (MaxIntegrity > 0.0f) ? (Unit->Integrity / MaxIntegrity) : 0.0f;
		ActiveHeroHPBar->SetPercent(Pct);
		HPFillBrush.TintColor = FSlateColor(BattleGlass::HealthColorFor(Pct));
	}

	if (ActiveHeroAPText.IsValid())
	{
		// 行动点 + 状态摘要 (燃烧/导电/酸蚀...)
		const int32 MaxAP = Unit->GetSkillComponent() ? Unit->GetSkillComponent()->GetEffectiveMaxAP() : Unit->Stats.MaxAP;
		FString APLine = FString::Printf(TEXT("行动点  %d / %d   ·   1 AP = %.0f cm"), Unit->AP, MaxAP, Unit->GetCmPerAP());
		if (Unit->GetSkillComponent())
		{
			const FString StatusSummary = Unit->GetSkillComponent()->GetStatusSummary();
			if (!StatusSummary.IsEmpty())
			{
				APLine += FString::Printf(TEXT("   ·   %s"), *StatusSummary);
			}
		}
		ActiveHeroAPText->SetText(FText::FromString(APLine));
	}
}

void SBattleMainOverlay::RebuildHandCards(ABattleUnit* Unit)
{
	if (!HandCardsBox.IsValid())
	{
		return;
	}

	HandCardsBox->ClearChildren();
	CardWidgets.Reset();

	if (!IsValid(Unit) || !Unit->IsAlive() || Unit->Team != EBattleTeam::Player)
	{
		return;
	}

	ABattlePlayerController* PC = PlayerController.Get();
	const EPlayerInteractionMode Mode = PC ? PC->GetInteractionMode() : EPlayerInteractionMode::Idle;
	const int32 SelectedSkill = PC ? PC->GetSelectedSkillIndex() : -1;

	const TArray<FBattleSkillSpec>& Skills = Unit->GetSkillSpecs();

	// 1. 生成技能手牌 (技能系统 2.0: 统一技能实例, 冷却中置灰)
	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		const FBattleSkillSpec& Spec = Skills[i];
		const UBattleSkillData* Skill = Spec.SkillData;
		if (!IsValid(Skill))
		{
			continue;
		}

		const bool bOnCooldown = Spec.IsOnCooldown();
		const bool bCanAfford = (Unit->AP >= Skill->APCost) && !bOnCooldown;
		const bool bIsSelected = (Mode == EPlayerInteractionMode::TargetingSkill && SelectedSkill == i);
		const FString Hotkey = FString::Printf(TEXT("[%d]"), i + 1);

		// 悬停提示: 名称 + 描述 + 射程/伤害/费用/冷却一行
		FString RangeStr;
		if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly && Skill->AoE.IsArea())
		{
			RangeStr = FString::Printf(TEXT("自身周围 %.0f cm"), Skill->AoE.Radius);
		}
		else if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
		{
			RangeStr = TEXT("自身");
		}
		else if (Skill->AoE.IsArea())
		{
			RangeStr = FString::Printf(TEXT("射程 %.0f · 范围 %.0f"), Skill->Range, Skill->AoE.Radius);
		}
		else
		{
			RangeStr = FString::Printf(TEXT("射程 %.0f cm"), Skill->Range);
		}
		if (Skill->Chain.IsChain())
		{
			RangeStr += FString::Printf(TEXT(" · 连锁%d"), Skill->Chain.MaxJumps);
		}

		FString TipStr = FString::Printf(TEXT("%s\n%s\n%s  ·  %s %.0f  ·  %d AP"),
			*Skill->DisplayName.ToString(), *Skill->Description.ToString(), *RangeStr,
			GetDamageTypeName(Skill->Damage.DamageType), Skill->GetEstimatedTotalDamage(), Skill->APCost);
		if (Skill->HitCount > 1)
		{
			TipStr += FString::Printf(TEXT(" (x%d)"), Skill->HitCount);
		}
		if (bOnCooldown)
		{
			TipStr += TEXT("  ·  ") + Spec.GetCooldownLabel();
		}

		const FText CooldownBadge = Spec.CooldownRemaining > 0 || Spec.bBlockedThisTurn
			? FText::FromString(FString::Printf(TEXT("%d"), FMath::Max(1, Spec.CooldownRemaining)))
			: FText::GetEmpty();

		TSharedPtr<SBattleSkillCardWidget> CardWidget = SNew(SBattleSkillCardWidget)
			.CardIndex(i)
			.Title(Skill->DisplayName)
			.ToolTip(FText::FromString(TipStr))
			.CooldownBadge(CooldownBadge)
			.HotkeyText(FText::FromString(Hotkey))
			.APCost((float)Skill->APCost)
			.bIsMoveCard(false)
			.AccentColor(Skill->ThemeColor)
			.IconTexture(Skill->Icon)
			.OnClicked(this, &SBattleMainOverlay::HandleCardClicked, i, false);

		CardWidget->SetAffordable(bCanAfford);
		CardWidget->SetSelectedMode(bIsSelected);
		CardWidgets.Add(CardWidget);

		HandCardsBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(4.0f, 0.0f))
		[
			CardWidget.ToSharedRef()
		];
	}

	// 2. 生成战术移动卡牌
	{
		const bool bCanAffordMove = (Unit->AP >= 1);
		const bool bIsMoveSelected = (Mode == EPlayerInteractionMode::Moving);

		TSharedPtr<SBattleSkillCardWidget> MoveCardWidget = SNew(SBattleSkillCardWidget)
			.CardIndex(-1)
			.Title(FText::FromString(TEXT("移")))
			.ToolTip(FText::FromString(TEXT("战术机动\n消耗 AP 沿地面寻路奔跑，点击地面位移。\n1 AP = 200 cm")))
			.HotkeyText(FText::FromString(TEXT("[M]")))
			.APCost(0.0f)
			.bIsMoveCard(true)
			.AccentColor(FLinearColor(0.20f, 0.85f, 0.50f))
			.OnClicked(this, &SBattleMainOverlay::HandleCardClicked, -1, true);

		MoveCardWidget->SetAffordable(bCanAffordMove);
		MoveCardWidget->SetSelectedMode(bIsMoveSelected);
		CardWidgets.Add(MoveCardWidget);

		HandCardsBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(8.0f, 0.0f))
		[
			MoveCardWidget.ToSharedRef()
		];
	}
}

void SBattleMainOverlay::UpdateEndTurnButton(ABattleUnit* Unit, EBattleState State)
{
	if (!EndTurnButton.IsValid() || !EndTurnButtonText.IsValid())
	{
		return;
	}

	// 主按钮呼吸光晕 (与 Hall 主按钮同频)
	if (EndTurnGlowBorder.IsValid())
	{
		const bool bEnabled = EndTurnButton->IsEnabled();
		const float Breath = 0.5f + 0.5f * FMath::Sin(static_cast<float>(FSlateApplication::Get().GetCurrentTime()) * 1.6f);
		const float GlowAlpha = bEnabled ? (0.06f + 0.05f * Breath) : 0.0f;
		EndTurnGlowBrush.TintColor = FSlateColor(BattleGlass::AccentGold.CopyWithNewOpacity(GlowAlpha));
		EndTurnGlowBorder->Invalidate(EInvalidateWidgetReason::Paint);
	}

	if (State == EBattleState::BattleOver)
	{
		EndTurnButton->SetEnabled(false);
		EndTurnButtonText->SetText(FText::FromString(TEXT("战斗结束")));
		return;
	}

	if (Unit && Unit->IsAlive() && Unit->Team == EBattleTeam::Player)
	{
		EndTurnButton->SetEnabled(true);
		EndTurnButtonText->SetText(FText::FromString(TEXT("结束回合\n[SPACE]")));
	}
	else
	{
		EndTurnButton->SetEnabled(false);
		EndTurnButtonText->SetText(FText::FromString(TEXT("敌方行动中\n请稍候")));
	}
}

void SBattleMainOverlay::UpdateResultPanel(EBattleState State)
{
	if (!ResultPanel.IsValid())
	{
		return;
	}

	if (State != EBattleState::BattleOver)
	{
		ResultPanel->SetVisibility(EVisibility::Collapsed);
		return;
	}

	UBattleManager* Mgr = BattleManager.Get();
	ABattleGameMode* GM = GetBattleGameMode();

	// 胜负以 GameMode 的结算结果为准; 极端情况下 (结算尚未到达) 用场上存活数兜底
	bool bPlayerWon = true;
	if (GM && GM->IsBattleResolved())
	{
		bPlayerWon = GM->DidPlayerWin();
	}
	else if (Mgr)
	{
		for (ABattleUnit* U : Mgr->GetAllUnits())
		{
			if (U && U->IsAlive() && U->Team == EBattleTeam::Enemy)
			{
				bPlayerWon = false;
				break;
			}
		}
	}

	// ---------- 结算明细 (后续掉落 / 经验 / 好感度直接往 Rows 里加) ----------
	int32 AlivePlayers = 0;
	int32 TotalPlayers = 0;
	float CurrentIntegrity = 0.0f;
	float MaxIntegrity = 0.0f;
	if (Mgr)
	{
		for (ABattleUnit* U : Mgr->GetAllUnits())
		{
			if (U && U->Team == EBattleTeam::Player)
			{
				++TotalPlayers;
				if (U->IsAlive())
				{
					++AlivePlayers;
				}
				CurrentIntegrity += FMath::Max(0.0f, U->Integrity);
				MaxIntegrity += U->Stats.MaxIntegrity;
			}
		}
	}
	const float IntegrityPercent = (MaxIntegrity > KINDA_SMALL_NUMBER) ? (CurrentIntegrity / MaxIntegrity) : 0.0f;

	TArray<FBattleResultRow> Rows;
	if (Mgr)
	{
		Rows.Emplace(FText::FromString(TEXT("战斗回合")), FText::AsNumber(Mgr->GetRoundIndex()), BattleGlass::TextPrimary);
	}
	Rows.Emplace(FText::FromString(TEXT("存活队员")),
		FText::FromString(FString::Printf(TEXT("%d / %d"), AlivePlayers, TotalPlayers)),
		(AlivePlayers == TotalPlayers) ? BattleGlass::HealthHigh : BattleGlass::HealthMid);
	Rows.Emplace(FText::FromString(TEXT("队伍完整度")),
		FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(CurrentIntegrity), FMath::RoundToInt(MaxIntegrity))),
		BattleGlass::HealthColorFor(IntegrityPercent));
	// 预留位: 目前没有掉落系统, 先占一行, 免得以后加内容时面板结构又要动
	Rows.Emplace(FText::FromString(TEXT("战利品")), FText::FromString(TEXT("—")), BattleGlass::TextTertiary);

	FText Subtitle;
	if (bPlayerWon)
	{
		Subtitle = (GM && GM->IsRunBattle())
			? FText::FromString(FString::Printf(TEXT("敌方全灭  ·  航线图节点 %d"), GM->GetPendingNodeId()))
			: FText::FromString(TEXT("敌方全灭  ·  战术推演完成"));
	}
	else
	{
		Subtitle = FText::FromString(TEXT("我方全灭  ·  本次远征结束"));
	}

	const FText ButtonText = (GM && GM->ShouldReturnToRoute())
		? FText::FromString(TEXT("返 回 航 线 图"))
		: FText::FromString(TEXT("返 回 空 间 站 大 厅"));

	ResultPanel->SetResult(bPlayerWon,
		bPlayerWon ? FText::FromString(TEXT("战 斗 胜 利")) : FText::FromString(TEXT("战 斗 失 败")),
		Subtitle, Rows, ButtonText);

	UE_LOG(LogERBattleUI, Log, TEXT("[BattleUI] 结算面板弹出: %s | %s | 返回按钮=%s | 回合=%d 存活=%d/%d 完整度=%.0f/%.0f"),
		bPlayerWon ? TEXT("胜利") : TEXT("失败"), *Subtitle.ToString(), *ButtonText.ToString(),
		Mgr ? Mgr->GetRoundIndex() : 0, AlivePlayers, TotalPlayers, CurrentIntegrity, MaxIntegrity);
}

ABattleGameMode* SBattleMainOverlay::GetBattleGameMode() const
{
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		if (UWorld* World = PC->GetWorld())
		{
			return Cast<ABattleGameMode>(World->GetAuthGameMode());
		}
	}
	if (UBattleManager* Mgr = BattleManager.Get())
	{
		if (UWorld* World = Mgr->GetWorld())
		{
			return Cast<ABattleGameMode>(World->GetAuthGameMode());
		}
	}
	return nullptr;
}

FReply SBattleMainOverlay::HandleCheatKillClicked()
{
	if (ABattleGameMode* GM = GetBattleGameMode())
	{
		UE_LOG(LogERBattleUI, Log, TEXT("[Battle] DEBUG 按钮: 秒杀全部敌人"));
		GM->DebugKillAllEnemies();
	}
	return FReply::Handled();
}

void SBattleMainOverlay::HandleResultPrimaryClicked()
{
	if (ABattleGameMode* GM = GetBattleGameMode())
	{
		GM->ReturnFromBattle();
		return;
	}

	// 兜底: 拿不到 GameMode 时直接回大厅
	UWorld* World = nullptr;
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		World = PC->GetWorld();
	}
	if (World)
	{
		UE_LOG(LogERBattleUI, Log, TEXT("[Battle] 结算返回 (兜底路径) -> Lvl_Hall"));
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")), true,
			TEXT("game=/Script/EbbingRainbow.HallGameMode"));
	}
}

FReply SBattleMainOverlay::HandleEndTurnClicked()
{
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		PC->RequestEndTurn();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SBattleMainOverlay::HandleCardClicked(int32 Index, bool bIsMove)
{
	UE_LOG(LogERBattleUI, Log, TEXT("[BattleInput] 卡牌点击: Index=%d bIsMove=%d PC有效=%d"), Index, bIsMove, PlayerController.IsValid());
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		if (bIsMove)
		{
			PC->ToggleMoveMode();
		}
		else
		{
			PC->StartSkillTargeting(Index);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SBattleMainOverlay::HandlePortraitClicked(int32 UnitIndex)
{
	ABattlePlayerController* PC = PlayerController.Get();
	if (!PC)
	{
		return FReply::Unhandled();
	}

	if (!PortraitWidgets.IsValidIndex(UnitIndex))
	{
		return FReply::Handled();
	}

	ABattleUnit* Unit = PortraitWidgets[UnitIndex]->GetUnit();
	if (!Unit)
	{
		return FReply::Handled();
	}

	// 阵亡单位不给聚焦, 但保留点击反馈
	if (!Unit->IsAlive())
	{
		return FReply::Handled();
	}

	PC->FocusOnUnit(Unit);
	return FReply::Handled();
}
