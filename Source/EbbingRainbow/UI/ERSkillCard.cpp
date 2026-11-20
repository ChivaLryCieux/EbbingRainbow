// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERSkillCard.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UERSkillCard::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERSkillCard::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativePreConstruct();

	SetRenderTransformPivot(FVector2D(0.5f, 1.0f));

	if (IsDesignTime())
	{
		bAffordable = bPreviewAffordable;
		bSelected = bPreviewSelected;
		Setup(0, PreviewName, PreviewName, PreviewCooldown, PreviewHotkey, PreviewCost, false, PreviewAccent, PreviewIcon);
		SetRenderOpacity(bPreviewAffordable ? 1.0f : UnaffordableOpacity);
		if (bEnableProceduralHoverMotion)
		{
			const float PreviewLift = bPreviewSelected ? 1.0f : 0.0f;
			SetRenderTranslation(FVector2D(0.0f, HoverLiftY * PreviewLift));
			SetRenderScale(FVector2D(FMath::Lerp(1.0f, HoverScale, PreviewLift)));
		}
	}
}

void UERSkillCard::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();
	LiftSpring.SnapTo(0.0f);
	SweepProgress = 1.0f;
}

void UERSkillCard::BindDelegates()
{
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (HitButton)
	{
		HitButton->OnClicked.AddDynamic(this, &UERSkillCard::HandleClicked);
		HitButton->OnHovered.AddDynamic(this, &UERSkillCard::HandleHoverBegin);
		HitButton->OnUnhovered.AddDynamic(this, &UERSkillCard::HandleHoverEnd);
	}
}

void UERSkillCard::Build()
{
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("HitButton"));
	FButtonStyle BtnStyle;
	const FSlateBrush Transparent = ERUI::TransparentBrush();
	BtnStyle.SetNormal(Transparent);
	BtnStyle.SetHovered(Transparent);
	BtnStyle.SetPressed(Transparent);
	BtnStyle.SetDisabled(Transparent);
	BtnStyle.SetNormalPadding(FMargin(0.0f));
	BtnStyle.SetPressedPadding(FMargin(0.0f));
	HitButton->SetStyle(BtnStyle);
	HitButton->SetCursor(EMouseCursor::Hand);
	WidgetTree->RootWidget = HitButton;

	USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardBox"));
	CardBox->SetWidthOverride(98.0f);
	CardBox->SetHeightOverride(132.0f);
	HitButton->SetContent(CardBox);

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Ov"));
	Ov->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CardBox->SetContent(Ov);

	// 1. 哑光黑曜石 45° 切角战术卡牌底板
	CardFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("CardFrame"));
	CardFrame->Chamfer = FVector4(0.0f, 12.0f, 0.0f, 0.0f);
	CardFrame->FillColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.92f);
	CardFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.22f);
	CardFrame->bDrawCornerBrackets = true;
	CardFrame->BracketLength = 7.0f;
	CardFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.65f);
	CardFrame->AccentEdge = EERAccentEdge::Bottom;
	CardFrame->AccentThickness = 3.0f;
	CardFrame->AccentColor = T->Accent;
	CardFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(CardFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	// 2. 主列
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Col->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(Col);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(8.0f, 7.0f, 8.0f, 8.0f));
	}

	// 顶行: 反白费用块 + 极小快捷键
	{
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Top"));
		Top->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		CostBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CostBadge"));
		CostBadge->SetBrush(ERUI::RoundedBrush(0.0f, T->Bone));
		CostBadge->SetBrushColor(FLinearColor::White);
		CostBadge->SetPadding(FMargin(6.0f, 1.0f));
		CostText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->PureInk, true);
		CostText->SetFont(ERUI::FontBold(13, 0));
		CostBadge->SetContent(CostText);
		Top->AddChildToHorizontalBox(CostBadge)->SetVerticalAlignment(VAlign_Center);

		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("Sp"));
		Sp->SetSize(FVector2D(1.0f, 1.0f));
		Top->AddChildToHorizontalBox(Sp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		HotkeyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary, true);
		HotkeyText->SetFont(ERUI::FontBold(9, 120));
		UHorizontalBoxSlot* HkSlot = Top->AddChildToHorizontalBox(HotkeyText);
		HkSlot->SetHorizontalAlignment(HAlign_Right);
		HkSlot->SetVerticalAlignment(VAlign_Center);
		Col->AddChildToVerticalBox(Top);
	}

	// 中部: 克制的图标暗槽 / 字形
	{
		UOverlay* Mid = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Mid"));
		Mid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconBox"));
		IconBox->SetWidthOverride(48.0f);
		IconBox->SetHeightOverride(48.0f);
		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IconImage"));
		IconImage->SetBrushFromTexture(nullptr);
		IconImage->SetDesiredSizeOverride(FVector2D(48.0f, 48.0f));
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		IconBox->SetContent(IconImage);
		UOverlaySlot* IbSlot = Mid->AddChildToOverlay(IconBox);
		IbSlot->SetHorizontalAlignment(HAlign_Center);
		IbSlot->SetVerticalAlignment(VAlign_Center);

		GlyphText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone);
		GlyphText->SetFont(ERUI::FontBold(24, 0));
		UOverlaySlot* GlSlot = Mid->AddChildToOverlay(GlyphText);
		GlSlot->SetHorizontalAlignment(HAlign_Center);
		GlSlot->SetVerticalAlignment(VAlign_Center);

		UVerticalBoxSlot* MidSlot = Col->AddChildToVerticalBox(Mid);
		MidSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MidSlot->SetPadding(FMargin(0.0f, 4.0f));
	}

	// 底部技能名 (锐利粗体, 与底部点睛色条配合)
	{
		NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->Bone);
		NameText->SetFont(ERUI::FontBold(10, 40));
		NameText->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* NameSlot = Col->AddChildToVerticalBox(NameText);
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	// 冷却角标: 右下反黑高对比数字
	{
		CooldownBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CooldownBg"));
		CooldownBg->SetBrush(ERUI::WhiteBrush(FVector2D(20.0f, 18.0f)));
		CooldownBg->SetColorAndOpacity(T->DangerColor);
		CooldownBg->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* CbSlot = Ov->AddChildToOverlay(CooldownBg);
		CbSlot->SetHorizontalAlignment(HAlign_Right);
		CbSlot->SetVerticalAlignment(VAlign_Bottom);
		CbSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 6.0f));

		CooldownText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->Bone, true);
		CooldownText->SetFont(ERUI::FontBold(10, 0));
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* CtSlot = Ov->AddChildToOverlay(CooldownText);
		CtSlot->SetHorizontalAlignment(HAlign_Right);
		CtSlot->SetVerticalAlignment(VAlign_Bottom);
		CtSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 8.0f));
	}

	SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
}

void UERSkillCard::Setup(int32 InIndex, const FText& Name, const FText& Tooltip, const FText& InCooldownText,
	const FText& Hotkey, int32 APCost, bool bInIsMove, FLinearColor InAccent, UTexture2D* Icon)
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}

	CardIndex = InIndex;
	bIsMove = bInIsMove;
	Accent = InAccent;
	CardName = Name;
	CardTooltip = Tooltip;

	if (CostText)
	{
		CostText->SetText(bIsMove ? MoveCostLabel : FText::AsNumber(APCost));
	}
	if (HotkeyText)
	{
		HotkeyText->SetText(Hotkey.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Printf(TEXT("[%s]"), *Hotkey.ToString())));
	}
	if (NameText)
	{
		NameText->SetText(Name);
	}
	if (IconImage)
	{
		const bool bHasIcon = IsValid(Icon);
		IconImage->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasIcon)
		{
			FSlateBrush Brush = IconImage->GetBrush();
			Brush.SetResourceObject(Icon);
			IconImage->SetBrush(Brush);
		}
	}
	if (GlyphText)
	{
		const bool bHasIcon = IsValid(Icon);
		GlyphText->SetVisibility(bHasIcon ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		GlyphText->SetText(Name.IsEmpty() ? FText::GetEmpty() : FText::FromString(Name.ToString().Left(1)));
		GlyphText->SetColorAndOpacity(FSlateColor(ERUI::Fade(Accent, 1.0f)));
	}
	if (AccentLine)
	{
		AccentLine->SetColorAndOpacity(InAccent);
	}
	const bool bHasCd = !InCooldownText.IsEmpty();
	if (CooldownText)
	{
		CooldownText->SetVisibility(bHasCd ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		CooldownText->SetText(InCooldownText);
	}
	if (CooldownBg)
	{
		CooldownBg->SetVisibility(bHasCd ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyVisualState();
	if (!IsDesignTime())
	{
		BP_OnCardSetup(CardIndex, bIsMove, APCost, Accent, Icon);
	}
}

void UERSkillCard::SetAffordable(bool bInAffordable)
{
	bAffordable = bInAffordable;
	ApplyVisualState();
}

void UERSkillCard::SetSelectedMode(bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		if (bSelected)
		{
			SweepProgress = 0.01f;
		}
		if (Anim_Select && !IsDesignTime())
		{
			PlayAnimation(Anim_Select, 0.0f, 1, bSelected ? EUMGSequencePlayMode::Forward : EUMGSequencePlayMode::Reverse);
		}
	}
	ApplyVisualState();
}

void UERSkillCard::ApplyVisualState()
{
	const UERUITheme* T = ERUI::Theme();
	const float Active01 = FMath::Clamp(LiftT, 0.0f, 1.0f);

	if (CardFrame)
	{
		const FLinearColor Fill = bSelected
			? FLinearColor(0.024f, 0.026f, 0.034f, 0.97f)
			: FMath::Lerp(FLinearColor(0.008f, 0.009f, 0.012f, 0.90f), FLinearColor(0.020f, 0.022f, 0.028f, 0.95f), Active01);
		const FLinearColor Border = bSelected
			? T->Bone
			: FLinearColor(0.92f, 0.92f, 0.90f, 0.22f + 0.62f * Active01);
		const FLinearColor Bracket = bSelected ? T->Accent : FLinearColor(0.92f, 0.92f, 0.90f, 0.65f + 0.35f * Active01);
		const FLinearColor EdgeCol = bSelected ? T->Accent : (bAffordable ? Accent : T->BoneGhost);
		CardFrame->SetFrameColors(Fill, Border, Bracket, EdgeCol);
		CardFrame->SetMotionState(1.0f, Active01, SweepProgress);
	}
	if (CostBadge)
	{
		CostBadge->SetBrush(ERUI::RoundedBrush(0.0f, bSelected ? T->Accent : (bAffordable ? T->Bone : T->BoneGhost)));
		CostBadge->SetBrushColor(FLinearColor::White);
	}
	if (CostText)
	{
		CostText->SetColorAndOpacity(FSlateColor(bAffordable ? T->PureInk : T->BoneTertiary));
	}
	if (AccentLine)
	{
		AccentLine->SetColorAndOpacity(bSelected ? T->Accent : (bAffordable ? Accent : T->BoneGhost));
	}
	if (SelectedFrame)
	{
		SelectedFrame->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!IsDesignTime())
	{
		BP_OnVisualStateChanged(bHovered, bSelected, bAffordable);
	}
}

void UERSkillCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float TargetLift = (bHovered || bSelected) ? 1.0f : 0.0f;
	const float TargetFade = bAffordable ? 1.0f : UnaffordableOpacity;
	const float Speed = (HoverInterpSpeedOverride > 0.0f) ? HoverInterpSpeedOverride : ERUI::Theme()->HoverInterpSpeed;

	LiftSpring.SetTarget(TargetLift);
	LiftT = LiftSpring.Tick(InDeltaTime);
	FadeT = FMath::FInterpTo(FadeT, TargetFade, InDeltaTime, Speed);

	if (SweepProgress < 1.0f)
	{
		SweepProgress = FMath::Min(1.0f, SweepProgress + InDeltaTime / 0.36f);
	}

	if (bEnableProceduralHoverMotion)
	{
		SetRenderTranslation(FVector2D(0.0f, HoverLiftY * LiftT));
		SetRenderScale(FVector2D(FMath::Lerp(1.0f, HoverScale, LiftT)));
	}
	SetRenderOpacity(FadeT);

	ApplyVisualState();
}

void UERSkillCard::HandleClicked()
{
	LiftSpring.Impulse(-4.5f);
	OnCardClicked.Broadcast(CardIndex, bIsMove);
}

void UERSkillCard::HandleHoverBegin()
{
	bHovered = true;
	SweepProgress = 0.01f;
	if (Anim_Hover && !IsDesignTime())
	{
		PlayAnimationForward(Anim_Hover);
	}
	OnCardHover.Broadcast(this, true);
}

void UERSkillCard::HandleHoverEnd()
{
	bHovered = false;
	if (Anim_Hover && !IsDesignTime())
	{
		PlayAnimationReverse(Anim_Hover);
	}
	OnCardHover.Broadcast(this, false);
}

UWidget* UERSkillCard::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
