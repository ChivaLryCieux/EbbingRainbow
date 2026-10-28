// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/SBattleMainOverlay.h"
#include "Battle/UI/SBattleUnitPortrait.h"
#include "Battle/UI/SBattleGlassKit.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/Units/BattleUnit.h"

#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Rendering/SlateRenderTransform.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

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

	// 卡牌玻璃底: 烟熏 + 主题色发丝描边
	BattleGlass::ConfigureGlassBrush(CardBgBrush, BattleGlass::RadiusCard,
		FLinearColor(0.045f, 0.065f, 0.100f, 0.82f), 1.2f, AccentColor.CopyWithNewOpacity(0.45f));

	// AP 晶体徽章
	BattleGlass::ConfigureGlassBrush(CostGemBrush, BattleGlass::RadiusPill,
		bIsMoveCard ? FLinearColor(0.10f, 0.55f, 0.35f, 0.90f) : FLinearColor(0.12f, 0.42f, 0.90f, 0.90f),
		1.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.45f));

	const FText CostBadgeText = bIsMoveCard ? FText::FromString(TEXT("移动")) : FText::FromString(FString::Printf(TEXT("%.0f AP"), APCost));

	ChildSlot
	[
		SAssignNew(BorderWidget, SBorder)
		.BorderImage(&CardBgBrush)
		.Padding(FMargin(11.0f, 9.0f))
		[
			SNew(SBox)
			.WidthOverride(150.0f)
			.HeightOverride(185.0f)
			[
				SNew(SVerticalBox)

				// 顶部行: 左侧 AP 费用徽章, 右侧快捷键提示
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(&CostGemBrush)
						.Padding(FMargin(8.0f, 3.0f))
						[
							SNew(STextBlock)
							.Text(CostBadgeText)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(FLinearColor(0.90f, 0.78f, 0.35f, 0.90f))
					]
				]

				// 卡牌标题
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 2.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._Title)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(BattleGlass::TextPrimary)
				]

				// 技能类型 / 范围标签
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 7.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._Category)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(AccentColor.CopyWithNewOpacity(0.95f))
				]

				// 卡牌效果描述 (多行自动折行)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(InArgs._Description)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FLinearColor(0.80f, 0.85f, 0.90f, 0.80f))
					.AutoWrapText(true)
				]

				// 底部状态提示
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 3.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() {
						if (!bCanAfford) return FText::FromString(TEXT("! AP 不足"));
						if (bIsSelected) return FText::FromString(TEXT(">> 瞄准中 <<"));
						return FText::FromString(TEXT("点击使用"));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity_Lambda([this]() {
						if (!bCanAfford) return FSlateColor(FLinearColor(0.85f, 0.35f, 0.35f));
						if (bIsSelected) return FSlateColor(FLinearColor(1.0f, 0.85f, 0.25f));
						return FSlateColor(FLinearColor(0.55f, 0.68f, 0.82f, 0.65f));
					})
				]
			]
		]
	];

	SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
}

void SBattleSkillCardWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 平滑悬停上浮与缩放插值
	const float TargetOffset = (bHovered || bIsSelected) ? -22.0f : 0.0f;
	const float TargetScale = (bHovered || bIsSelected) ? 1.07f : 1.0f;

	HoverOffset = FMath::FInterpTo(HoverOffset, TargetOffset, InDeltaTime, 14.0f);
	CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, InDeltaTime, 14.0f);

	SetRenderTransform(FSlateRenderTransform(CurrentScale, FVector2D(0.0f, HoverOffset)));

	// 玻璃描边随状态增强
	if (bIsSelected)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.85f, 0.25f, 1.0f));
		CardBgBrush.OutlineSettings.Width = 2.6f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(0.085f, 0.100f, 0.150f, 0.90f));
	}
	else if (bHovered)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 0.90f));
		CardBgBrush.OutlineSettings.Width = 2.0f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(0.075f, 0.105f, 0.160f, 0.87f));
	}
	else if (!bCanAfford)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.35f, 0.38f, 0.44f, 0.45f));
		CardBgBrush.OutlineSettings.Width = 1.0f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.72f));
	}
	else
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(AccentColor.CopyWithNewOpacity(0.45f));
		CardBgBrush.OutlineSettings.Width = 1.2f;
		CardBgBrush.TintColor = FSlateColor(FLinearColor(0.045f, 0.065f, 0.100f, 0.82f));
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
		FLinearColor(0.055f, 0.110f, 0.205f, 0.78f), 1.2f, BattleGlass::Hairline);

	// 战报通知条
	BattleGlass::ConfigureGlassBrush(NotificationBrush, BattleGlass::RadiusPill,
		FLinearColor(0.05f, 0.07f, 0.11f, 0.84f), 1.1f, BattleGlass::AccentGold.CopyWithNewOpacity(0.35f));

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
		// 5. 胜负结算横幅
		// =============================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ResultBannerBorder, SBorder)
			.BorderImage(&PanelBrush)
			.Padding(FMargin(52.0f, 26.0f))
			.Visibility(EVisibility::Hidden)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SAssignNew(ResultBannerText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(BattleGlass::AccentGold)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 20.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.OnClicked_Lambda([this]() {
						UWorld* World = nullptr;
						if (PlayerController.IsValid()) World = PlayerController->GetWorld();
						else if (BattleManager.IsValid()) World = BattleManager->GetWorld();
						if (!World && GEngine)
						{
							for (const FWorldContext& Context : GEngine->GetWorldContexts())
							{
								if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
								{
									if (UWorld* Cand = Context.World()) { World = Cand; break; }
								}
							}
						}
						if (World)
						{
							UE_LOG(LogTemp, Log, TEXT("[Battle] Returning to Hall level..."));
							UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")), true, TEXT("game=/Script/EbbingRainbow.HallGameMode"));
						}
						return FReply::Handled();
					})
					[
						SNew(SBorder)
						.BorderImage(&EndTurnNormalBrush)
						.Padding(FMargin(30.0f, 11.0f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("返回空间站大厅   ·   RETURN TO HALL")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity(FLinearColor(0.10f, 0.08f, 0.02f))
						]
					]
				]
			]
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

	// 1. 检查先攻队列是否有增减或单位阵亡, 触发重建
	int32 QueueHash = 0;
	const TArray<ABattleUnit*>& Queue = Mgr->GetInitiativeQueue();
	for (int32 i = 0; i < Queue.Num(); ++i)
	{
		if (ABattleUnit* U = Queue[i])
		{
			QueueHash = HashCombine(QueueHash, GetTypeHash(U));
			QueueHash = HashCombine(QueueHash, U->HP);
		}
	}
	if (QueueHash != CachedQueueHash || CurrentActive != CachedActiveUnit)
	{
		CachedQueueHash = QueueHash;
		RebuildInitiativeBar();
	}

	// 1b. 编队面板: 我方单位集合变化 (阵亡/增援) 时重建
	{
		TArray<TWeakObjectPtr<ABattleUnit>> PlayerUnitsNow;
		for (ABattleUnit* U : Mgr->GetAllUnits())
		{
			if (U && U->Team == EBattleTeam::Player)
			{
				PlayerUnitsNow.Add(U);
			}
		}

		bool bRosterChanged = PlayerUnitsNow.Num() != CachedPlayerUnits.Num();
		if (!bRosterChanged)
		{
			for (int32 i = 0; i < PlayerUnitsNow.Num(); ++i)
			{
				if (PlayerUnitsNow[i] != CachedPlayerUnits[i])
				{
					bRosterChanged = true;
					break;
				}
			}
		}

		if (bRosterChanged && RosterBox.IsValid())
		{
			CachedPlayerUnits = PlayerUnitsNow;
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
	const int32 CurrHP = CurrentActive ? CurrentActive->HP : -1;

	if (CurrentActive != CachedActiveUnit || FMath::Abs(CurrAP - CachedAP) > 0.05f || CurrHP != CachedHP || CurrentMode != CachedMode)
	{
		CachedActiveUnit = CurrentActive;
		CachedAP = CurrAP;
		CachedHP = CurrHP;
		CachedMode = CurrentMode;

		UpdateHeroPlate(CurrentActive);
		RebuildHandCards(CurrentActive);
	}

	// 3. 刷新结束回合按钮状态与胜负横幅
	if (CurrentState != CachedState)
	{
		CachedState = CurrentState;
		UpdateResultBanner(CurrentState);
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
			bIsAlive ? FLinearColor(0.055f, 0.105f, 0.195f, 0.78f) : FLinearColor(0.07f, 0.07f, 0.08f, 0.62f),
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
			ActiveHeroHPText->SetText(FText::FromString(FString::Printf(TEXT("完整度  %.0f / %.0f   +%.0f 护盾"), Unit->Integrity, Unit->Stats.MaxIntegrity, Unit->Barrier)));
		}
		else
		{
			ActiveHeroHPText->SetText(FText::FromString(FString::Printf(TEXT("完整度  %.0f / %.0f"), Unit->Integrity, Unit->Stats.MaxIntegrity)));
		}
	}

	if (ActiveHeroHPBar.IsValid())
	{
		const float Pct = (Unit->Stats.MaxIntegrity > 0.0f) ? (Unit->Integrity / Unit->Stats.MaxIntegrity) : 0.0f;
		ActiveHeroHPBar->SetPercent(Pct);
		HPFillBrush.TintColor = FSlateColor(BattleGlass::HealthColorFor(Pct));
	}

	if (ActiveHeroAPText.IsValid())
	{
		ActiveHeroAPText->SetText(FText::FromString(FString::Printf(TEXT("行动点  %d / %d   ·   1 AP = %.0f cm"), Unit->AP, Unit->Stats.MaxAP, Unit->GetCmPerAP())));
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

	if (!Unit || !Unit->IsAlive() || Unit->Team != EBattleTeam::Player)
	{
		return;
	}

	ABattlePlayerController* PC = PlayerController.Get();
	const EPlayerInteractionMode Mode = PC ? PC->GetInteractionMode() : EPlayerInteractionMode::Idle;
	const int32 SelectedSkill = PC ? PC->GetSelectedSkillIndex() : -1;

	const TArray<FBattleSkillDef>& Skills = Unit->GetSkills();

	// 1. 生成技能手牌
	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		const FBattleSkillDef& Skill = Skills[i];
		const bool bCanAfford = (Unit->AP >= Skill.APCost);
		const bool bIsSelected = (Mode == EPlayerInteractionMode::TargetingSkill && SelectedSkill == i);
		const FString Hotkey = FString::Printf(TEXT("[%d]"), i + 1);

		TSharedPtr<SBattleSkillCardWidget> CardWidget = SNew(SBattleSkillCardWidget)
			.CardIndex(i)
			.Title(FText::FromString(Skill.DisplayName))
			.Category(FText::FromString(FString::Printf(TEXT("射程 %.0f cm   |   %s %d"), Skill.Range, GetDamageTypeName(Skill.DamageType), Skill.BaseDamage)))
			.Description(FText::FromString(Skill.Description))
			.HotkeyText(FText::FromString(Hotkey))
			.APCost((float)Skill.APCost)
			.bIsMoveCard(false)
			.AccentColor(Skill.Color)
			.OnClicked(this, &SBattleMainOverlay::HandleCardClicked, i, false);

		CardWidget->SetAffordable(bCanAfford);
		CardWidget->SetSelectedMode(bIsSelected);
		CardWidgets.Add(CardWidget);

		HandCardsBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(8.0f, 0.0f))
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
			.Title(FText::FromString(TEXT("战术机动")))
			.Category(FText::FromString(TEXT("自由位移   ·   1 AP = 200 cm")))
			.Description(FText::FromString(TEXT("消耗 AP 沿地面寻路奔跑。\n点击地面可直接位移。")))
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

void SBattleMainOverlay::UpdateResultBanner(EBattleState State)
{
	if (!ResultBannerBorder.IsValid() || !ResultBannerText.IsValid())
	{
		return;
	}

	if (State == EBattleState::BattleOver)
	{
		ResultBannerBorder->SetVisibility(EVisibility::Visible);

		UBattleManager* Mgr = BattleManager.Get();
		bool bPlayerWon = true;
		if (Mgr)
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

		if (bPlayerWon)
		{
			ResultBannerText->SetText(FText::FromString(TEXT("VICTORY\n敌方全灭  ·  战术推演胜利")));
			ResultBannerText->SetColorAndOpacity(BattleGlass::AccentGold);
		}
		else
		{
			ResultBannerText->SetText(FText::FromString(TEXT("DEFEAT\n我方全灭  ·  战术推演失败")));
			ResultBannerText->SetColorAndOpacity(BattleGlass::AccentEnemy);
		}
	}
	else
	{
		ResultBannerBorder->SetVisibility(EVisibility::Hidden);
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
