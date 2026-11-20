// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERSkillTooltip.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

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
		if (!TitleText && !TooltipBg && !TooltipFrame) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERSkillTooltip::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!TitleText && !TooltipBg && !TooltipFrame) { Build(); }
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

	TooltipFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("TooltipFrame"));
	TooltipFrame->Chamfer = FVector4(0.0f, 12.0f, 0.0f, 0.0f);
	TooltipFrame->FillColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.94f);
	TooltipFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.24f);
	TooltipFrame->bDrawCornerBrackets = true;
	TooltipFrame->BracketLength = 8.0f;
	TooltipFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.72f);
	TooltipFrame->AccentEdge = EERAccentEdge::Left;
	TooltipFrame->AccentThickness = 3.0f;
	TooltipFrame->AccentColor = T->Accent;
	TooltipFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(TooltipFrame);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TipCol"));
	UOverlaySlot* ColSlot = Root->AddChildToOverlay(Col);
	ColSlot->SetPadding(FMargin(18.0f, 12.0f, 20.0f, 12.0f));

	TitleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone);
	TitleText->SetFont(ERUI::FontBold(18, 40));
	Col->AddChildToVerticalBox(TitleText);

	DescriptionText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary);
	DescriptionText->SetFont(ERUI::FontRegular(12, 20));
	Col->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));

	TooltipDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("TooltipDivider"));
	TooltipDivider->AnchorLength = 20.0f;
	TooltipDivider->AnchorColor = T->Accent;
	TooltipDivider->LineColor = T->BoneGhost;
	TooltipDivider->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Col->AddChildToVerticalBox(TooltipDivider)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 6.0f));

	MetaText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->Bone, true);
	MetaText->SetFont(ERUI::FontBold(10, 160));
	Col->AddChildToVerticalBox(MetaText);
}

void UERSkillTooltip::SetTooltipContent(const FText& InTitle, const FText& InDescription, const FText& InMeta, FLinearColor InAccent)
{
	const UERUITheme* T = ERUI::Theme();
	if (TooltipFrame)
	{
		TooltipFrame->SetFrameColors(
			FLinearColor(0.008f, 0.009f, 0.012f, 0.94f),
			FLinearColor(0.92f, 0.92f, 0.90f, 0.24f),
			FLinearColor(0.92f, 0.92f, 0.90f, 0.72f),
			InAccent);
	}
	if (TooltipDivider)
	{
		TooltipDivider->SetDividerColors(T->BoneGhost, InAccent);
	}
	if (AccentRule)
	{
		AccentRule->SetColorAndOpacity(InAccent);
	}
	if (TitleText)
	{
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
