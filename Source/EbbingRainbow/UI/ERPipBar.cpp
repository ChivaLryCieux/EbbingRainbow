// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERPipBar.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"

TSharedRef<SWidget> UERPipBar::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		PipBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		WidgetTree->RootWidget = PipBox;
		PipBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UERPipBar::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		PipBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		WidgetTree->RootWidget = PipBox;
		PipBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UERPipBar::SetPips(int32 Filled, int32 Total)
{
	if (Filled == CurFilled && Total == CurTotal)
	{
		return;
	}
	const bool bStructureChanged = (Total != CurTotal);
	CurFilled = Filled;
	CurTotal = Total;
	if (bStructureChanged)
	{
		Rebuild();
	}
	RefreshColors();
}

void UERPipBar::Rebuild()
{
	if (!PipBox)
	{
		return;
	}
	PipBox->ClearChildren();
	const float Size = ERUI::Theme()->PipSize;
	const float Gap = ERUI::Theme()->PipGap;
	for (int32 i = 0; i < CurTotal; ++i)
	{
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Pip->SetBrush(ERUI::WhiteBrush(FVector2D(Size, Size)));
		Pip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* S = PipBox->AddChildToHorizontalBox(Pip);
		S->SetPadding(FMargin(i == 0 ? 0.0f : Gap, 0.0f, 0.0f, 0.0f));
	}
}

void UERPipBar::RefreshColors()
{
	if (!PipBox)
	{
		return;
	}
	const FLinearColor Empty = ERUI::Theme()->BoneGhost;
	for (int32 i = 0; i < PipBox->GetChildrenCount(); ++i)
	{
		if (UImage* Pip = Cast<UImage>(PipBox->GetChildAt(i)))
		{
			Pip->SetColorAndOpacity(i < CurFilled ? PipColor : Empty);
		}
	}
}
