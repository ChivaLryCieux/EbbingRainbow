// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/SBattleUnitPortrait.h"

#include "Battle/Units/BattleUnit.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Rendering/SlateRenderTransform.h"

void SBattleUnitPortrait::Construct(const FArguments& InArgs)
{
	UnitIndex = InArgs._UnitIndex;
	EntranceDelay = InArgs._EntranceDelay;
	OnClickedDelegate = InArgs._OnClicked;
	AccentColor = BattleGlass::AccentPlayer;

	const float CardWidth = InArgs._CardWidth;

	// ---- 玻璃画刷 ----
	// 光晕层: 聚焦/当前回合时向外弥散的主题色
	BattleGlass::ConfigureGlassBrush(GlowBrush, BattleGlass::RadiusCard + 6.0f,
		FLinearColor(0, 0, 0, 0), 0.0f, FLinearColor::Transparent);

	// 主体: 烟熏玻璃 + 白色发丝描边
	BattleGlass::ConfigureGlassBrush(BodyBrush, BattleGlass::RadiusCard,
		BattleGlass::GlassFill, 1.1f, BattleGlass::Hairline);

	// 顶部高光发丝线 (苹果式内顶光, 让玻璃有厚度)
	BattleGlass::ConfigureGlassBrush(TopHighlightBrush, 2.0f,
		BattleGlass::TopHighlight, 0.0f, FLinearColor::Transparent);

	// 占位头像: 圆形玻璃徽章
	BattleGlass::ConfigureGlassBrush(AvatarBrush, BattleGlass::RadiusAvatar,
		FLinearColor(1, 1, 1, 0.07f), 1.4f, AccentColor.CopyWithNewOpacity(0.45f));

	// 血量条
	BattleGlass::ConfigureGlassBrush(HPTrackBrush, 4.0f,
		BattleGlass::HealthTrack, 0.0f, FLinearColor::Transparent);
	BattleGlass::ConfigureGlassBrush(HPFillBrush, 4.0f,
		BattleGlass::HealthHigh, 0.0f, FLinearColor::Transparent);

	// 聚焦指示轨: 左侧 3px 竖条
	BattleGlass::ConfigureGlassBrush(RailBrush, 1.5f,
		AccentColor.CopyWithNewOpacity(0.15f), 0.0f, FLinearColor::Transparent);

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(CardWidth)
		[
			SAssignNew(GlowBorder, SBorder)
			.BorderImage(&GlowBrush)
			.Padding(FMargin(0.0f))
			[
				SAssignNew(BodyBorder, SBorder)
				.BorderImage(&BodyBrush)
				.Padding(FMargin(1.0f))
				[
					SNew(SVerticalBox)

					// 顶部高光发丝线
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(TopHighlightBorder, SBorder)
						.BorderImage(&TopHighlightBrush)
						.Padding(FMargin(0.0f, 1.0f))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(RootButton, SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.Cursor(EMouseCursor::Hand)
						.OnClicked(InArgs._OnClicked)
						[
							SNew(SBox)
							.Padding(FMargin(12.0f, 10.0f, 14.0f, 11.0f))
							[
								SNew(SHorizontalBox)

								// ---- 左侧: 聚焦指示轨 + 占位头像 ----
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FMargin(0.0f, 0.0f, 11.0f, 0.0f))
								[
									SNew(SHorizontalBox)

									// 聚焦指示轨: 左侧 3px 竖条, 聚焦时点亮
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.FillWidth(0.0f)
									[
										SAssignNew(FocusRail, SBorder)
										.BorderImage(&RailBrush)
										.Padding(0.0f)
										[
											SNew(SBox)
											.WidthOverride(3.0f)
											.HeightOverride(34.0f)
										]
									]

									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
									[
										SNew(SBox)
										.WidthOverride(46.0f)
										.HeightOverride(46.0f)
										[
											SAssignNew(AvatarBorder, SBorder)
											.BorderImage(&AvatarBrush)
											.Padding(0.0f)
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											[
												SAssignNew(AvatarLetter, STextBlock)
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 19))
												.Justification(ETextJustify::Center)
												.ColorAndOpacity(FLinearColor::White)
											]
										]
									]
								]

								// ---- 右侧: 名称 / 血条 ----
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								[
									SNew(SVerticalBox)

									// 第一行: 单位名 + AP 行动点
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.VAlign(VAlign_Center)
										[
											SAssignNew(NameText, STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
											.ColorAndOpacity(BattleGlass::TextPrimary)
										]

										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SAssignNew(APText, STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
											.ColorAndOpacity(BattleGlass::AccentPlayer)
										]
									]

									// 第二行: 血量条 + 数值
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.VAlign(VAlign_Center)
										[
											SNew(SBox)
											.HeightOverride(6.0f)
											[
												SAssignNew(HPBar, SProgressBar)
												.Percent(1.0f)
												.BackgroundImage(&HPTrackBrush)
												.FillImage(&HPFillBrush)
											]
										]

										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(FMargin(9.0f, 0.0f, 0.0f, 0.0f))
										[
											SAssignNew(HPText, STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
											.ColorAndOpacity(BattleGlass::TextPrimary)
										]
									]
								]
							]
						]
					]
				]
			]
		]
	];

	SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
}

void SBattleUnitPortrait::SetUnit(ABattleUnit* InUnit)
{
	BoundUnit = InUnit;
	if (NameText.IsValid())
	{
		NameText->SetText(FText::FromString(TEXT("— 空槽位 —")));
	}
	if (AvatarLetter.IsValid())
	{
		AvatarLetter->SetText(FText::FromString(TEXT("?")));
	}
	if (APText.IsValid())
	{
		APText->SetText(FText::GetEmpty());
	}
	if (HPText.IsValid())
	{
		HPText->SetText(FText::GetEmpty());
	}
	if (HPBar.IsValid())
	{
		HPBar->SetPercent(0.0f);
	}
	CachedName.Reset();
	CachedIntegrity = -1.0f;
	CachedBarrier = -1.0f;
	CachedAP = -1;
	CachedMaxAP = -1;
	bCachedDead = false;
	RefreshFromUnit();
}

void SBattleUnitPortrait::SetFocused(bool bInFocused)
{
	bFocused = bInFocused;
}

void SBattleUnitPortrait::RefreshFromUnit()
{
	ABattleUnit* Unit = BoundUnit.Get();
	if (!Unit)
	{
		return;
	}

	AccentColor = BattleGlass::TeamAccent((int32)Unit->Team);
	bDead = !Unit->IsAlive();
	bActiveTurn = (Unit->AP > 0) && !bDead;

	const FString& Name = Unit->Stats.DisplayName;

	// 只在数值真的变化时才重建文本 (每帧重建会造成大量字符串分配)
	const bool bNameChanged = (CachedName != Name);
	const bool bHpChanged = !FMath::IsNearlyEqual(CachedIntegrity, Unit->Integrity, 0.01f)
		|| !FMath::IsNearlyEqual(CachedBarrier, Unit->Barrier, 0.01f);
	const bool bAPChanged = (CachedAP != Unit->AP) || (CachedMaxAP != Unit->Stats.MaxAP);
	const bool bDeadChanged = (bCachedDead != bDead);

	if (!bNameChanged && !bHpChanged && !bAPChanged && !bDeadChanged)
	{
		return;
	}

	CachedName = Name;
	CachedIntegrity = Unit->Integrity;
	CachedBarrier = Unit->Barrier;
	CachedAP = Unit->AP;
	CachedMaxAP = Unit->Stats.MaxAP;
	bCachedDead = bDead;

	// 占位头像: 取首字母大写
	FString Initial = Name.Left(1).ToUpper();
	if (Initial.IsEmpty())
	{
		Initial = TEXT("?");
	}
	if (AvatarLetter.IsValid())
	{
		AvatarLetter->SetText(FText::FromString(Initial));
	}

	if (NameText.IsValid())
	{
		NameText->SetText(FText::FromString(bDead ? Name + TEXT("   [已损毁]") : Name));
	}

	if (HPText.IsValid())
	{
		if (Unit->Barrier > 0.0f)
		{
			HPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f   +%.0f"),
				Unit->Integrity, Unit->Stats.MaxIntegrity, Unit->Barrier)));
		}
		else
		{
			HPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),
				Unit->Integrity, Unit->Stats.MaxIntegrity)));
		}
	}

	// AP 行动点文本
	if (APText.IsValid())
	{
		if (bDead)
		{
			APText->SetText(FText::FromString(TEXT("— —")));
		}
		else
		{
			APText->SetText(FText::FromString(FString::Printf(TEXT("AP %d / %d"), Unit->AP, Unit->Stats.MaxAP)));
		}
	}
}

void SBattleUnitPortrait::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 入场动画 (带错峰延迟, 首个 Tick 记录起始时刻)
	if (EntranceProgress < 1.0f)
	{
		if (EntranceStartTime < 0.0)
		{
			EntranceStartTime = InCurrentTime;
		}
		const float Local = static_cast<float>(InCurrentTime - EntranceStartTime) - EntranceDelay;
		EntranceProgress = FMath::Clamp(Local / BattleGlass::EntranceDuration, 0.0f, 1.0f);
	}

	ABattleUnit* Unit = BoundUnit.Get();
	if (Unit)
	{
		RefreshFromUnit();

		// 血量条平滑追随真实血量
		const float TargetPct = (Unit->Stats.MaxIntegrity > 0.0f)
			? FMath::Clamp(Unit->Integrity / Unit->Stats.MaxIntegrity, 0.0f, 1.0f)
			: 0.0f;
		DisplayedHpPercent = FMath::FInterpTo(DisplayedHpPercent, TargetPct, InDeltaTime, 9.0f);
	}

	RefreshVisuals(InCurrentTime, InDeltaTime);
}

void SBattleUnitPortrait::RefreshVisuals(double CurrentTime, float DeltaTime)
{
	const bool bHoveredNow = RootButton.IsValid() && RootButton->IsHovered();
	const bool bPressedNow = RootButton.IsValid() && RootButton->IsPressed();

	HoverAlpha = FMath::FInterpTo(HoverAlpha, bHoveredNow ? 1.0f : 0.0f, DeltaTime, BattleGlass::HoverInterpSpeed);
	PressAlpha = FMath::FInterpTo(PressAlpha, bPressedNow ? 1.0f : 0.0f, DeltaTime, BattleGlass::PressInterpSpeed);
	FocusAlpha = FMath::FInterpTo(FocusAlpha, bFocused ? 1.0f : 0.0f, DeltaTime, 10.0f);
	ActiveAlpha = FMath::FInterpTo(ActiveAlpha, bActiveTurn ? 1.0f : 0.0f, DeltaTime, 8.0f);
	DeathAlpha = FMath::FInterpTo(DeathAlpha, bDead ? 1.0f : 0.0f, DeltaTime, 7.0f);

	// 当前回合的缓慢呼吸 (与 Hall 主按钮同频)
	const float Breath = 0.5f + 0.5f * FMath::Sin(static_cast<float>(CurrentTime) * 1.6f);

	// ---- 玻璃底色: 常规 → 悬停提亮 → 聚焦再亮; 阵亡去饱和 ----
	FLinearColor Glass = FMath::Lerp(BattleGlass::GlassFill, BattleGlass::GlassFillHover, HoverAlpha);
	Glass = FMath::Lerp(Glass, BattleGlass::GlassFillActive, 0.55f * FocusAlpha);
	Glass = FMath::Lerp(Glass, BattleGlass::GlassFillDead, DeathAlpha);
	BodyBrush.TintColor = FSlateColor(Glass);

	// ---- 描边: 发丝线, 聚焦时转为主题色并加粗 ----
	FLinearColor Stroke = FMath::Lerp(BattleGlass::Hairline, BattleGlass::HairlineHover, HoverAlpha);
	Stroke = FMath::Lerp(Stroke, AccentColor.CopyWithNewOpacity(0.55f), FocusAlpha);
	Stroke = FMath::Lerp(Stroke, BattleGlass::AccentGold.CopyWithNewOpacity(0.45f + 0.30f * Breath), 0.55f * ActiveAlpha);
	BodyBrush.OutlineSettings.Color = FSlateColor(Stroke);
	BodyBrush.OutlineSettings.Width = 1.1f + 0.9f * FocusAlpha + 0.5f * ActiveAlpha;

	// ---- 聚焦/当前回合外圈弥散光 + 常驻柔影 ----
	// GlowBrush 比 BodyBorder 大 6px, 用一圈深色柔光等效"投影"让名牌从场景里浮起来;
	// 聚焦/当前回合时再叠上主题色辉光。
	const float GlowAlpha = FMath::Max(0.16f * FocusAlpha, 0.10f * ActiveAlpha * (0.4f + 0.6f * Breath));
	const float ShadowAlpha = 0.30f * (1.0f - 0.35f * DeathAlpha);
	GlowBrush.TintColor = FSlateColor(FMath::Lerp(
		FLinearColor(0.0f, 0.0f, 0.0f, ShadowAlpha),
		FMath::Lerp(AccentColor, BattleGlass::AccentGold, 0.5f * ActiveAlpha),
		FMath::Clamp(GlowAlpha / 0.30f, 0.0f, 1.0f)));
	GlowBrush.OutlineSettings.Color = FSlateColor(AccentColor.CopyWithNewOpacity(GlowAlpha));
	GlowBrush.OutlineSettings.Width = 6.0f;

	// ---- 左侧聚焦指示轨 ----
	if (FocusRail.IsValid())
	{
		const float RailAlpha = 0.12f + 0.88f * FocusAlpha;
		RailBrush.TintColor = FSlateColor(AccentColor.CopyWithNewOpacity(RailAlpha));
		FocusRail->Invalidate(EInvalidateWidgetReason::Paint);
	}

	// ---- 占位头像: 聚焦时填充主题色, 阵亡时灰化 ----
	FLinearColor AvatarFill = FLinearColor(1, 1, 1, 0.07f);
	AvatarFill = FMath::Lerp(AvatarFill, AccentColor.CopyWithNewOpacity(0.22f + 0.10f * Breath * ActiveAlpha), 0.5f + 0.5f * FocusAlpha);
	AvatarFill = FMath::Lerp(AvatarFill, FLinearColor(1, 1, 1, 0.04f), DeathAlpha);
	AvatarBrush.TintColor = FSlateColor(AvatarFill);
	AvatarBrush.OutlineSettings.Color = FSlateColor(FMath::Lerp(
		AccentColor.CopyWithNewOpacity(0.45f),
		AccentColor.CopyWithNewOpacity(0.95f),
		FMath::Max(FocusAlpha, ActiveAlpha * (0.5f + 0.5f * Breath))));

	if (AvatarLetter.IsValid())
	{
		FLinearColor LetterColor = FMath::Lerp(FLinearColor(1, 1, 1, 0.92f), AccentColor * 1.15f, FMath::Max(FocusAlpha, ActiveAlpha));
		LetterColor = FMath::Lerp(LetterColor, BattleGlass::TextDead, DeathAlpha);
		AvatarLetter->SetColorAndOpacity(LetterColor);
	}

	// ---- 文字状态 ----
	if (NameText.IsValid())
	{
		FLinearColor NameColor = FMath::Lerp(BattleGlass::TextPrimary, BattleGlass::AccentGold, 0.65f * ActiveAlpha);
		NameColor = FMath::Lerp(NameColor, BattleGlass::TextDead, DeathAlpha);
		NameText->SetColorAndOpacity(NameColor);
	}
	if (HPText.IsValid())
	{
		FLinearColor HPColor = FMath::Lerp(BattleGlass::TextPrimary, BattleGlass::HealthColorFor(DisplayedHpPercent), 0.35f);
		HPColor = FMath::Lerp(HPColor, BattleGlass::TextDead, DeathAlpha);
		HPText->SetColorAndOpacity(HPColor);
	}
	if (APText.IsValid())
	{
		FLinearColor APColor = FMath::Lerp(BattleGlass::AccentPlayer, BattleGlass::TextTertiary, 0.55f);
		APColor = FMath::Lerp(APColor, BattleGlass::AccentGold, ActiveAlpha * (0.5f + 0.5f * Breath));
		APColor = FMath::Lerp(APColor, BattleGlass::TextDead, DeathAlpha);
		APText->SetColorAndOpacity(APColor);
	}

	// ---- 血量条 ----
	if (HPBar.IsValid())
	{
		HPBar->SetPercent(DisplayedHpPercent);
		HPFillBrush.TintColor = FSlateColor(FMath::Lerp(
			BattleGlass::HealthColorFor(DisplayedHpPercent),
			FLinearColor(0.35f, 0.38f, 0.42f, 1.0f),
			DeathAlpha));
	}

	// ---- 入场动画: 左滑入 + 淡入 ----
	const float Eased = BattleGlass::EaseOutCubic(EntranceProgress);
	SetRenderOpacity(Eased);
	SetRenderTransform(FSlateRenderTransform(FVector2D((1.0f - Eased) * -22.0f, 0.0f)));

	// ---- 按压缩放 (液态回弹) ----
	if (BodyBorder.IsValid())
	{
		const float Scale = 1.0f - 0.018f * PressAlpha - 0.010f * HoverAlpha;
		BodyBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		BodyBorder->SetRenderTransform(FSlateRenderTransform(FScale2D(Scale)));
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBattleUnitPortrait::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	bHovered = true;
}

void SBattleUnitPortrait::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bHovered = false;
}
