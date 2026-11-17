// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERSkillCard.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

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

void UERSkillCard::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativeConstruct();
}

void UERSkillCard::Build()
{
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle BtnStyle;
	const FSlateBrush Transparent = ERUI::TransparentBrush();
	BtnStyle.SetNormal(Transparent);
	BtnStyle.SetHovered(Transparent);
	BtnStyle.SetPressed(Transparent);
	BtnStyle.SetDisabled(Transparent);
	HitButton->SetStyle(BtnStyle);
	HitButton->OnClicked.AddDynamic(this, &UERSkillCard::HandleClicked);
	HitButton->OnHovered.AddDynamic(this, &UERSkillCard::HandleHoverBegin);
	HitButton->OnUnhovered.AddDynamic(this, &UERSkillCard::HandleHoverEnd);
	WidgetTree->RootWidget = HitButton;

	USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardBox->SetWidthOverride(T->CardSize.X);
	CardBox->SetHeightOverride(T->CardSize.Y);
	HitButton->SetContent(CardBox);

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	CardBox->SetContent(Ov);

	// 主列
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Ov->AddChildToOverlay(Col);

	// 顶行: 费用 + 快捷键
	{
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		CostText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, true);
		Top->AddChildToHorizontalBox(CostText);

		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Sp->SetSize(FVector2D(1.0f, 1.0f));
		UHorizontalBoxSlot* SpSlot = Top->AddChildToHorizontalBox(Sp);
		SpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		HotkeyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary, true);
		UHorizontalBoxSlot* HkSlot = Top->AddChildToHorizontalBox(HotkeyText);
		HkSlot->SetHorizontalAlignment(HAlign_Right);
		HkSlot->SetVerticalAlignment(VAlign_Top);
		Col->AddChildToVerticalBox(Top);
	}

	// 中部: 图标 / 字形
	{
		UOverlay* Mid = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(30.0f);
		IconBox->SetHeightOverride(30.0f);
		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(nullptr);
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		IconBox->SetContent(IconImage);
		UOverlaySlot* IbSlot = Mid->AddChildToOverlay(IconBox);
		IbSlot->SetHorizontalAlignment(HAlign_Center);
		IbSlot->SetVerticalAlignment(VAlign_Center);

		GlyphText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone);
		UOverlaySlot* GlSlot = Mid->AddChildToOverlay(GlyphText);
		GlSlot->SetHorizontalAlignment(HAlign_Center);
		GlSlot->SetVerticalAlignment(VAlign_Center);

		UVerticalBoxSlot* MidSlot = Col->AddChildToVerticalBox(Mid);
		MidSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MidSlot->SetPadding(FMargin(0.0f, 2.0f));
	}

	// 主题色底线
	{
		AccentLine = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		AccentLine->SetBrush(ERUI::WhiteBrush(FVector2D(T->CardSize.X - 16.0f, 2.0f)));
		UVerticalBoxSlot* LineSlot = Col->AddChildToVerticalBox(AccentLine);
		LineSlot->SetHorizontalAlignment(HAlign_Center);
		LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 1.0f));
	}

	// 冷却角标: 右下墨底数字
	{
		CooldownBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		CooldownBg->SetBrush(ERUI::WhiteBrush(FVector2D(16.0f, 14.0f)));
		CooldownBg->SetColorAndOpacity(FLinearColor(0.03f, 0.035f, 0.045f, 0.9f));
		CooldownBg->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* CbSlot = Ov->AddChildToOverlay(CooldownBg);
		CbSlot->SetHorizontalAlignment(HAlign_Right);
		CbSlot->SetVerticalAlignment(VAlign_Bottom);
		CbSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

		CooldownText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->Bone, true);
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* CtSlot = Ov->AddChildToOverlay(CooldownText);
		CtSlot->SetHorizontalAlignment(HAlign_Right);
		CtSlot->SetVerticalAlignment(VAlign_Bottom);
		CtSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
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
		CostText->SetText(bIsMove ? FText::FromString(TEXT("移")) : FText::AsNumber(APCost));
	}
	if (HotkeyText)
	{
		HotkeyText->SetText(Hotkey);
	}
	if (IconImage)
	{
		const bool bHasIcon = IsValid(Icon);
		IconImage->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasIcon)
		{
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetDesiredSizeOverride(FVector2D(30.0f, 30.0f));
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
	if (CooldownText)
	{
		const bool bHasCd = !InCooldownText.IsEmpty();
		CooldownText->SetVisibility(bHasCd ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		CooldownBg->SetVisibility(bHasCd ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		CooldownText->SetText(InCooldownText);
	}

	ApplyVisualState();
}

void UERSkillCard::SetAffordable(bool bInAffordable)
{
	bAffordable = bInAffordable;
}

void UERSkillCard::SetSelectedMode(bool bInSelected)
{
	bSelected = bInSelected;
}

void UERSkillCard::ApplyVisualState()
{
	const UERUITheme* T = ERUI::Theme();
	if (AccentLine)
	{
		AccentLine->SetColorAndOpacity(bSelected ? T->Accent : (bAffordable ? Accent : T->BoneGhost));
	}
}

void UERSkillCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Speed = ERUI::Theme()->HoverInterpSpeed;
	const float TargetLift = (bHovered || bSelected) ? 1.0f : 0.0f;
	const float TargetFade = bAffordable ? 1.0f : 0.35f;

	LiftT = FMath::FInterpTo(LiftT, TargetLift, InDeltaTime, Speed);
	FadeT = FMath::FInterpTo(FadeT, TargetFade, InDeltaTime, Speed);

	SetRenderTranslation(FVector2D(0.0f, -10.0f * LiftT));
	SetRenderScale(FVector2D(1.0f + 0.05f * LiftT));
	SetRenderOpacity(FadeT);

	ApplyVisualState();
}

void UERSkillCard::HandleClicked()
{
	OnCardClicked.Broadcast(CardIndex, bIsMove);
}

void UERSkillCard::HandleHoverBegin()
{
	bHovered = true;
	OnCardHover.Broadcast(this, true);
}

void UERSkillCard::HandleHoverEnd()
{
	bHovered = false;
	OnCardHover.Broadcast(this, false);
}

UWidget* UERSkillCard::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
