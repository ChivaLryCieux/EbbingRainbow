// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERStatCell.h"

#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Image.h"
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

	// 极小追踪微标 (Control: 共振式大小字极度对比)
	LabelText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	LabelText->SetFont(ERUI::FontRegular(8, 220));
	Col->AddChildToVerticalBox(LabelText);

	// 巨幅 Linux Biolinum 粗体数值
	ValueText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, true);
	ValueText->SetFont(ERUI::FontBold(26, 10));
	Col->AddChildToVerticalBox(ValueText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 4.0f));

	UImage* FootTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FootTick"));
	FootTick->SetBrush(ERUI::WhiteBrush(FVector2D(16.0f, 1.5f)));
	FootTick->SetColorAndOpacity(T->BoneGhost);
	FootTick->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBoxSlot* TS = Col->AddChildToVerticalBox(FootTick);
	TS->SetHorizontalAlignment(HAlign_Left);
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
