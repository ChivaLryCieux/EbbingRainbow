// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERPipBar.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UERPipBar::EnsureContainer()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PipBox && WidgetTree)
		{
			PipBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PipBox"));
			WidgetTree->RootWidget = PipBox;
		}
		if (PipBox)
		{
			PipBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

TSharedRef<SWidget> UERPipBar::RebuildWidget()
{
	EnsureContainer();
	return Super::RebuildWidget();
}

void UERPipBar::NativePreConstruct()
{
	EnsureContainer();
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		CurTotal = -1;
		CurFilled = -1;
		SetPips(FMath::Clamp(PreviewFilled, 0, PreviewTotal), FMath::Max(0, PreviewTotal));
	}
}

void UERPipBar::NativeConstruct()
{
	EnsureContainer();
	Super::NativeConstruct();
}

void UERPipBar::SetPipColor(FLinearColor Color)
{
	PipColor = Color;
	bHasExplicitColor = true;
	RefreshColors();
}

void UERPipBar::SetPips(int32 Filled, int32 Total)
{
	EnsureContainer();
	if (Filled == CurFilled && Total == CurTotal && !IsDesignTime())
	{
		return;
	}
	const bool bStructureChanged = (Total != CurTotal) || IsDesignTime();
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
	if (!PipBox || !WidgetTree)
	{
		return;
	}
	PipBox->ClearChildren();
	const float Size = (PipSizeOverride > 0.0f) ? PipSizeOverride : ERUI::Theme()->PipSize;
	const float Gap = (PipGapOverride >= 0.0f) ? PipGapOverride : ERUI::Theme()->PipGap;
	UTexture2D* Tex = PipTexture ? PipTexture.Get() : ERUI::GetWhiteTexture();
	for (int32 i = 0; i < CurTotal; ++i)
	{
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.ImageSize = FVector2D(Size, Size);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Pip->SetBrush(Brush);
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
	const UERUITheme* T = ERUI::Theme();
	const FLinearColor Filled = bUseThemeColors ? (bHasExplicitColor ? PipColor : T->Bone) : CustomFilledColor;
	const FLinearColor Empty = bUseThemeColors ? T->BoneGhost : CustomEmptyColor;
	for (int32 i = 0; i < PipBox->GetChildrenCount(); ++i)
	{
		if (UImage* Pip = Cast<UImage>(PipBox->GetChildAt(i)))
		{
			Pip->SetColorAndOpacity(i < CurFilled ? Filled : Empty);
		}
	}
}
