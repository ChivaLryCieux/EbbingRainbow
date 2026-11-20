// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERFloatingText.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UERFloatingText::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!FloatingTextBlock) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERFloatingText::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!FloatingTextBlock) { Build(); }
	}
	Super::NativePreConstruct();

	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (IsDesignTime())
	{
		SetFloatingData(PreviewText, PreviewColor, PreviewWorldSize);
	}
}

void UERFloatingText::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FloatingTextBlock = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, true);
	FSlateFontInfo FontInfo = ERUI::FontBold(FMath::RoundToInt(FMath::Max(26.0f, BaseFontSize)), 20);
	FontInfo.OutlineSettings.OutlineSize = 2;
	FontInfo.OutlineSettings.OutlineColor = FLinearColor(0.005f, 0.006f, 0.008f, 0.92f);
	FloatingTextBlock->SetFont(FontInfo);
	FloatingTextBlock->SetShadowOffset(FVector2D(1.5f, 2.0f));
	FloatingTextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	FloatingTextBlock->SetJustification(ETextJustify::Center);

	UOverlaySlot* S = Root->AddChildToOverlay(FloatingTextBlock);
	S->SetHorizontalAlignment(HAlign_Center);
	S->SetVerticalAlignment(VAlign_Center);
}

void UERFloatingText::SetFloatingData(const FText& InText, FLinearColor InColor, float InWorldSize)
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!FloatingTextBlock) { Build(); }
	}

	const float Ref = FMath::Max(8.0f, ReferenceWorldSize);
	SizeRatio = FMath::Clamp(InWorldSize / Ref, 0.5f, 2.5f);

	if (FloatingTextBlock)
	{
		FloatingTextBlock->SetText(InText);
		FloatingTextBlock->SetColorAndOpacity(FSlateColor(InColor));
	}
	SetPopupTransform(1.0f, 1.0f);

	if (Anim_Pop && !IsDesignTime())
	{
		PlayAnimationForward(Anim_Pop);
	}
	if (!IsDesignTime())
	{
		BP_OnAcquired(InText, InColor, InWorldSize);
	}
}

void UERFloatingText::SetPopupTransform(float InScale, float InAlpha)
{
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderScale(FVector2D(InScale * SizeRatio));
	SetRenderOpacity(InAlpha);
}
