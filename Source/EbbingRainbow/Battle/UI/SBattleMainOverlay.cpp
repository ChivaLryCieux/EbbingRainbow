// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/SBattleMainOverlay.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Units/BattleUnit.h"

#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Rendering/SlateRenderTransform.h"
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

	// 卡牌背景圆角矩形画刷
	CardBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	CardBgBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.07f, 0.11f, 0.94f));
	CardBgBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	CardBgBrush.OutlineSettings.Width = 2.0f;
	CardBgBrush.OutlineSettings.Color = FSlateColor(AccentColor * 0.75f);

	// 左上角 AP 晶体标记画刷
	CostGemBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	CostGemBrush.TintColor = FSlateColor(bIsMoveCard ? FLinearColor(0.1f, 0.6f, 0.35f, 0.95f) : FLinearColor(0.12f, 0.45f, 0.95f, 0.95f));
	CostGemBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
	CostGemBrush.OutlineSettings.Width = 1.0f;
	CostGemBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));

	const FText CostBadgeText = bIsMoveCard ? FText::FromString(TEXT("移动")) : FText::FromString(FString::Printf(TEXT("%.0f AP"), APCost));

	ChildSlot
	[
		SAssignNew(BorderWidget, SBorder)
		.BorderImage(&CardBgBrush)
		.Padding(FMargin(10.0f, 8.0f, 10.0f, 8.0f))
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
						.Padding(FMargin(7.0f, 3.0f))
						[
							SNew(STextBlock)
							.Text(CostBadgeText)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
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
						.ColorAndOpacity(FLinearColor(0.85f, 0.75f, 0.3f, 0.9f))
					]
				]

				// 卡牌标题
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 2.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._Title)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					.ColorAndOpacity(FLinearColor::White)
				]

				// 技能类型 / 范围标签
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._Category)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(AccentColor)
				]

				// 卡牌效果描述 (多行自动折行)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(InArgs._Description)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f, 0.85f))
					.AutoWrapText(true)
				]

				// 底部状态提示
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() {
						if (!bCanAfford) return FText::FromString(TEXT("! AP 不足"));
						if (bIsSelected) return FText::FromString(TEXT(">> 瞄准中 <<"));
						return FText::FromString(TEXT("点击使用"));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity_Lambda([this]() {
						if (!bCanAfford) return FSlateColor(FLinearColor(0.85f, 0.3f, 0.3f));
						if (bIsSelected) return FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f));
						return FSlateColor(FLinearColor(0.5f, 0.65f, 0.8f, 0.7f));
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

	// 动态外边框发光
	if (bIsSelected)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
		CardBgBrush.OutlineSettings.Width = 3.0f;
	}
	else if (bHovered)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.78f, 0.25f, 0.95f));
		CardBgBrush.OutlineSettings.Width = 2.5f;
	}
	else if (!bCanAfford)
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.3f, 0.3f, 0.35f, 0.5f));
		CardBgBrush.OutlineSettings.Width = 1.0f;
	}
	else
	{
		CardBgBrush.OutlineSettings.Color = FSlateColor(AccentColor * 0.75f);
		CardBgBrush.OutlineSettings.Width = 1.8f;
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

	// 顶部先攻栏底座画刷
	TopBarBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TopBarBgBrush.TintColor = FSlateColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.88f));
	TopBarBgBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
	TopBarBgBrush.OutlineSettings.Width = 1.5f;
	TopBarBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.2f, 0.28f, 0.45f, 0.7f));

	// 英雄面板底座画刷
	HeroPlateBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	HeroPlateBgBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.92f));
	HeroPlateBgBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
	HeroPlateBgBrush.OutlineSettings.Width = 1.8f;
	HeroPlateBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.25f, 0.45f, 0.8f, 0.8f));

	// 金色“结束回合”按钮样式 (Slay the Spire 风格)
	EndTurnNormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	EndTurnNormalBrush.TintColor = FSlateColor(FLinearColor(0.72f, 0.52f, 0.12f, 0.95f));
	EndTurnNormalBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	EndTurnNormalBrush.OutlineSettings.Width = 2.0f;
	EndTurnNormalBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.88f, 0.45f, 1.0f));

	EndTurnHoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	EndTurnHoverBrush.TintColor = FSlateColor(FLinearColor(0.92f, 0.7f, 0.18f, 1.0f));
	EndTurnHoverBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	EndTurnHoverBrush.OutlineSettings.Width = 2.5f;
	EndTurnHoverBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.98f, 0.65f, 1.0f));

	EndTurnPressedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	EndTurnPressedBrush.TintColor = FSlateColor(FLinearColor(0.55f, 0.38f, 0.08f, 1.0f));
	EndTurnPressedBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	EndTurnPressedBrush.OutlineSettings.Width = 2.0f;
	EndTurnPressedBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.9f, 0.8f, 0.3f, 1.0f));

	EndTurnDisabledBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	EndTurnDisabledBrush.TintColor = FSlateColor(FLinearColor(0.18f, 0.18f, 0.22f, 0.85f));
	EndTurnDisabledBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	EndTurnDisabledBrush.OutlineSettings.Width = 1.0f;
	EndTurnDisabledBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.35f, 0.35f, 0.4f, 0.6f));

	EndTurnBtnStyle.SetNormal(EndTurnNormalBrush);
	EndTurnBtnStyle.SetHovered(EndTurnHoverBrush);
	EndTurnBtnStyle.SetPressed(EndTurnPressedBrush);
	EndTurnBtnStyle.SetDisabled(EndTurnDisabledBrush);

	ChildSlot
	[
		SNew(SOverlay)

		// -------------------------------------------------------------
		// 1. 顶部先攻队列栏
		// -------------------------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 14.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(&TopBarBgBrush)
			.Padding(FMargin(18.0f, 6.0f, 18.0f, 6.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("— 先 攻 序 列 (INITIATIVE) —")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(FLinearColor(0.65f, 0.72f, 0.85f, 0.85f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(InitiativeHBox, SHorizontalBox)
				]
			]
		]

		// -------------------------------------------------------------
		// 2. 动态战报提示条
		// -------------------------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 85.0f, 0.0f, 0.0f)
		[
			SAssignNew(NotificationBorder, SBorder)
			.BorderImage(&TopBarBgBrush)
			.Padding(FMargin(16.0f, 5.0f))
			.Visibility(EVisibility::Hidden)
			[
				SAssignNew(NotificationTextBlock, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.35f))
			]
		]

		// -------------------------------------------------------------
		// 3. 底部主行动面板 (左侧英雄属性 + 中间战术手牌 + 右侧结束回合)
		// -------------------------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(30.0f, 0.0f, 30.0f, 18.0f)
		[
			SNew(SHorizontalBox)

			// 底部左侧: 活跃英雄状态板
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Bottom)
			[
				SNew(SBorder)
				.BorderImage(&HeroPlateBgBrush)
				.Padding(FMargin(16.0f, 12.0f, 18.0f, 12.0f))
				[
					SNew(SBox)
					.WidthOverride(230.0f)
					[
						SNew(SVerticalBox)

						// 英雄名称与阵营
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 6.0f)
						[
							SAssignNew(ActiveHeroNameText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
							.ColorAndOpacity(FLinearColor::White)
						]

						// HP 血条与数值
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SAssignNew(ActiveHeroHPText, STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor(0.3f, 0.95f, 0.45f))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 3.0f, 0.0f, 0.0f)
							[
								SAssignNew(ActiveHeroHPBar, SProgressBar)
								.Percent(1.0f)
								.FillColorAndOpacity(FLinearColor(0.2f, 0.85f, 0.35f))
							]
						]

						// AP 能量槽展示
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SAssignNew(ActiveHeroAPText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor(0.35f, 0.75f, 1.0f))
						]
					]
				]
			]

			// 底部中央: 战术卡牌手牌区
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(15.0f, 0.0f)
			[
				SAssignNew(HandCardsBox, SHorizontalBox)
			]

			// 底部右侧: 金色“结束回合”按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SBox)
				.WidthOverride(150.0f)
				.HeightOverride(68.0f)
				[
					SAssignNew(EndTurnButton, SButton)
					.ButtonStyle(&EndTurnBtnStyle)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SBattleMainOverlay::HandleEndTurnClicked)
					[
						SAssignNew(EndTurnButtonText, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity(FLinearColor(0.1f, 0.08f, 0.02f))
					]
				]
			]
		]

		// -------------------------------------------------------------
		// 4. 胜负结算横幅 (全灭敌方或我方全灭时居中展示)
		// -------------------------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ResultBannerBorder, SBorder)
			.BorderImage(&TopBarBgBrush)
			.Padding(FMargin(50.0f, 25.0f))
			.Visibility(EVisibility::Hidden)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SAssignNew(ResultBannerText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
					.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.15f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 18.0f, 0.0f, 0.0f)
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
						.Padding(FMargin(28.0f, 10.0f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("返回空间站大厅 (RETURN TO HALL) >>")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity(FLinearColor(0.1f, 0.08f, 0.02f))
						]
					]
				]
			]
		]
	];
}

void SBattleMainOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

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

	// 5. 检查鼠标是否在任何手牌或结束按钮上
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
			FrameColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
		}
		else if (bIsActive)
		{
			FrameColor = FLinearColor(1.0f, 0.85f, 0.15f, 1.0f);
		}
		else if (bIsPlayer)
		{
			FrameColor = FLinearColor(0.2f, 0.6f, 1.0f, 0.85f);
		}
		else
		{
			FrameColor = FLinearColor(0.95f, 0.25f, 0.2f, 0.85f);
		}

		TSharedPtr<FSlateBrush> ChipBrush = MakeShared<FSlateBrush>();
		ChipBrush->DrawAs = ESlateBrushDrawType::RoundedBox;
		ChipBrush->TintColor = FSlateColor(bIsAlive ? (bIsPlayer ? FLinearColor(0.06f, 0.12f, 0.22f, 0.9f) : FLinearColor(0.22f, 0.06f, 0.06f, 0.9f)) : FLinearColor(0.08f, 0.08f, 0.08f, 0.8f));
		ChipBrush->OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
		ChipBrush->OutlineSettings.Width = bIsActive ? 3.0f : 1.5f;
		ChipBrush->OutlineSettings.Color = FSlateColor(FrameColor);
		ChipBrushes.Add(ChipBrush);

		const float HPPercent = (Unit->Stats.MaxIntegrity > 0.0f) ? (Unit->Integrity / Unit->Stats.MaxIntegrity) : 0.0f;
		const FString UnitLabel = bIsAlive ? FString::Printf(TEXT("%s\n先攻 %d"), *Unit->Stats.DisplayName, Unit->Stats.Initiative)
		                                   : FString::Printf(TEXT("%s\n[已损毁]"), *Unit->Stats.DisplayName);

		InitiativeHBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(5.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(ChipBrush.Get())
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SBox)
				.WidthOverride(82.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(UnitLabel))
						.Font(FCoreStyle::GetDefaultFontStyle(bIsActive ? "Bold" : "Regular", 10))
						.ColorAndOpacity(bIsAlive ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(SProgressBar)
						.Percent(HPPercent)
						.FillColorAndOpacity(bIsPlayer ? FLinearColor(0.2f, 0.85f, 0.35f) : FLinearColor(0.9f, 0.2f, 0.2f))
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
	const FString NameFormatted = FString::Printf(TEXT("[%s] %s"), *TeamStr, *Unit->Stats.DisplayName);

	if (ActiveHeroNameText.IsValid())
	{
		ActiveHeroNameText->SetText(FText::FromString(NameFormatted));
		ActiveHeroNameText->SetColorAndOpacity(bIsPlayer ? FLinearColor(0.4f, 0.8f, 1.0f) : FLinearColor(1.0f, 0.4f, 0.4f));
	}

	if (ActiveHeroHPText.IsValid())
	{
		if (Unit->Barrier > 0.0f)
		{
			ActiveHeroHPText->SetText(FText::FromString(FString::Printf(TEXT("完整度: %.0f / %.0f (+%.0f护盾)"), Unit->Integrity, Unit->Stats.MaxIntegrity, Unit->Barrier)));
		}
		else
		{
			ActiveHeroHPText->SetText(FText::FromString(FString::Printf(TEXT("完整度: %.0f / %.0f"), Unit->Integrity, Unit->Stats.MaxIntegrity)));
		}
	}

	if (ActiveHeroHPBar.IsValid())
	{
		const float Pct = (Unit->Stats.MaxIntegrity > 0.0f) ? (Unit->Integrity / Unit->Stats.MaxIntegrity) : 0.0f;
		ActiveHeroHPBar->SetPercent(Pct);
		ActiveHeroHPBar->SetFillColorAndOpacity(Pct > 0.3f ? FLinearColor(0.2f, 0.85f, 0.35f) : FLinearColor(0.9f, 0.25f, 0.15f));
	}

	if (ActiveHeroAPText.IsValid())
	{
		ActiveHeroAPText->SetText(FText::FromString(FString::Printf(TEXT("行动点 AP: %d / %d\n(先攻 %d -> 1 AP = %.0fcm 移动)"), Unit->AP, Unit->Stats.MaxAP, Unit->Stats.Initiative, Unit->GetCmPerAP())));
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
			.Category(FText::FromString(FString::Printf(TEXT("射程 %.0fcm | 伤害 %d (%s)"), Skill.Range, Skill.BaseDamage, GetDamageTypeName(Skill.DamageType))))
			.Description(FText::FromString(Skill.Description))
			.HotkeyText(FText::FromString(Hotkey))
			.APCost((float)Skill.APCost)
			.bIsMoveCard(false)
			.AccentColor(Skill.Color)
			.OnClicked_Lambda([this, i]() {
				return HandleCardClicked(i, false);
			});

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
			.Category(FText::FromString(TEXT("自由位移 [1 AP=200cm]")))
			.Description(FText::FromString(TEXT("消耗 AP 沿地面寻路奔跑。\n点击地面可直接位移。")))
			.HotkeyText(FText::FromString(TEXT("[M]")))
			.APCost(0.0f)
			.bIsMoveCard(true)
			.AccentColor(FLinearColor(0.2f, 0.85f, 0.5f))
			.OnClicked_Lambda([this]() {
				return HandleCardClicked(-1, true);
			});

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
		EndTurnButtonText->SetText(FText::FromString(TEXT("敌方行动中...\n请等待")));
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
			ResultBannerText->SetText(FText::FromString(TEXT("★ VICTORY! 敌方全灭 ★\n原型战斗胜利")));
			ResultBannerText->SetColorAndOpacity(FLinearColor(1.0f, 0.88f, 0.2f));
		}
		else
		{
			ResultBannerText->SetText(FText::FromString(TEXT("☠ DEFEAT! 我方全灭 ☠\n战术推演失败")));
			ResultBannerText->SetColorAndOpacity(FLinearColor(1.0f, 0.25f, 0.25f));
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
