// Copyright Epic Games, Inc. All rights reserved.

#include "UI/ERStatCell.h"

#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> UERStatCell::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!LabelText && !ValueText) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERStatCell::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!LabelText && !ValueText) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		SetStat(PreviewLabel, PreviewValue, PreviewTint);
	}
}

void UERStatCell::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CellColumn"));
	Col->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Col;

	LabelText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	FSlateFontInfo LabelFont = T->GetFont(EERTypeScale::Micro);
	LabelFont.Size = 9;
	LabelFont.LetterSpacing = 120;
	LabelText->SetFont(LabelFont);
	Col->AddChildToVerticalBox(LabelText);

	ValueText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, true);
	FSlateFontInfo ValueFont = T->GetFont(EERTypeScale::Title, true);
	ValueFont.Size = 21;
	ValueText->SetFont(ValueFont);
	Col->AddChildToVerticalBox(ValueText)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));
}

void UERStatCell::SetStat(const FText& InLabel, const FText& InValue, FLinearColor InTint)
{
	PreviewLabel = InLabel;
	PreviewValue = InValue;
	PreviewTint = InTint;
	SetLabel(InLabel);
	SetValue(InValue, InTint);
}

void UERStatCell::SetValue(const FText& InValue, FLinearColor InTint)
{
	PreviewValue = InValue;
	PreviewTint = InTint;
	if (ValueText)
	{
		ValueText->SetText(InValue);
		ValueText->SetColorAndOpacity(FSlateColor(InTint));
	}
}

void UERStatCell::SetLabel(const FText& InLabel)
{
	PreviewLabel = InLabel;
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
}
