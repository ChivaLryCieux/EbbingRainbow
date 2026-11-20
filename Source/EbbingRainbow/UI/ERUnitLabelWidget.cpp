// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERUnitLabelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UERUnitLabelWidget::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	return Super::RebuildWidget();
}

void UERUnitLabelWidget::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	Super::NativeConstruct();

	if (LabelText)
	{
		LabelText->SetText(CachedText);
		LabelText->SetColorAndOpacity(FSlateColor(ERUI::Theme()->Bone));
	}
	if (AccentBar)
	{
		AccentBar->SetColorAndOpacity(CachedColor);
	}
}

void UERUnitLabelWidget::Build()
{
	const UERUITheme* T = ERUI::Theme();

	BackdropBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BackdropBorder"));
	BackdropBorder->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.009f, 0.012f, 0.86f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.22f)));
	BackdropBorder->SetBrushColor(FLinearColor::White);
	BackdropBorder->SetPadding(FMargin(8.0f, 4.0f, 10.0f, 4.0f));
	BackdropBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = BackdropBorder;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LabelRow"));
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	BackdropBorder->SetContent(Row);

	AccentBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AccentBar"));
	AccentBar->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 22.0f)));
	AccentBar->SetColorAndOpacity(CachedColor);
	AccentBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(AccentBar);
	BarSlot->SetVerticalAlignment(VAlign_Fill);
	BarSlot->SetPadding(FMargin(0.0f, 1.0f, 8.0f, 1.0f));

	LabelText = ERUI::MakeText(this, CachedText, EERTypeScale::Micro, T->Bone);
	FSlateFontInfo FontInfo = ERUI::FontBold(10, 60);
	LabelText->SetFont(FontInfo);
	LabelText->SetJustification(ETextJustify::Left);
	LabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
	Row->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);
}

void UERUnitLabelWidget::SetLabelText(const FText& InText)
{
	CachedText = InText;
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	if (LabelText)
	{
		LabelText->SetText(CachedText);
	}
}

void UERUnitLabelWidget::SetLabelColor(const FLinearColor& InColor)
{
	CachedColor = InColor;
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	if (AccentBar)
	{
		AccentBar->SetColorAndOpacity(CachedColor);
	}
	if (LabelText)
	{
		// 黑底纯白字保证绝对清晰, 阵营色彩仅在左侧 3px 点睛竖条体现
		LabelText->SetColorAndOpacity(FSlateColor(ERUI::Theme()->Bone));
	}
}
