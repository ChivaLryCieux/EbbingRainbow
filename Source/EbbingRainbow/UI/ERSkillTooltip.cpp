// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERSkillTooltip.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> UERSkillTooltip::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!TitleText && !TooltipBg) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERSkillTooltip::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!TitleText && !TooltipBg) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		SetTooltipContent(PreviewTitle, PreviewDescription, PreviewMeta, PreviewAccent);
	}
}

void UERSkillTooltip::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TooltipOverlay"));
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TooltipBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TooltipBg"));
	TooltipBg->SetBrush(ERUI::WhiteBrush());
	TooltipBg->SetColorAndOpacity(T->InkPanel);
	TooltipBg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlaySlot* BgSlot = Root->AddChildToOverlay(TooltipBg);
	BgSlot->SetHorizontalAlignment(HAlign_Fill);
	BgSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TipCol"));
	UOverlaySlot* ColSlot = Root->AddChildToOverlay(Col);
	ColSlot->SetPadding(FMargin(16.0f, 10.0f));

	AccentRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AccentRule"));
	AccentRule->SetBrush(ERUI::WhiteBrush(FVector2D(28.0f, 2.0f)));
	AccentRule->SetColorAndOpacity(T->Accent);
	AccentRule->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBoxSlot* RuleSlot = Col->AddChildToVerticalBox(AccentRule);
	RuleSlot->SetHorizontalAlignment(HAlign_Left);
	RuleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	TitleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->Bone);
	Col->AddChildToVerticalBox(TitleText);

	DescriptionText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary);
	Col->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	MetaText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary, true);
	Col->AddChildToVerticalBox(MetaText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
}

void UERSkillTooltip::SetTooltipContent(const FText& InTitle, const FText& InDescription, const FText& InMeta, FLinearColor InAccent)
{
	if (AccentRule)
	{
		AccentRule->SetColorAndOpacity(InAccent);
	}
	if (TitleText)
	{
		// 若 WBP 删掉了独立的 DescriptionText, 则把描述拼进 TitleText 兜底
		if (!DescriptionText && !InDescription.IsEmpty())
		{
			TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *InTitle.ToString(), *InDescription.ToString())));
		}
		else
		{
			TitleText->SetText(InTitle);
		}
	}
	if (DescriptionText)
	{
		DescriptionText->SetVisibility(InDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		DescriptionText->SetText(InDescription);
	}
	if (MetaText)
	{
		MetaText->SetVisibility(InMeta.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		MetaText->SetText(InMeta);
	}
}
