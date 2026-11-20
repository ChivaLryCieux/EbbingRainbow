// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERRouteNode.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERRouteWidgets.h"
#include "UI/ERUIStatics.h"
#include "Run/RouteLayout.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

// ============================================================
// UERRouteLegendItem
// ============================================================

TSharedRef<SWidget> UERRouteLegendItem::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!LabelText && !TypeIcon) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERRouteLegendItem::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!LabelText && !TypeIcon) { Build(); }
	}
	Super::NativePreConstruct();

	SetupLegendItem(NodeType, LegendLabel, AccentColor, CustomIcon);
}

void UERRouteLegendItem::Build()
{
	const UERUITheme* T = ERUI::Theme();
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LegendRow"));
	WidgetTree->RootWidget = Row;
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TypeIcon = WidgetTree->ConstructWidget<UERRouteTypeIcon>(UERRouteTypeIcon::StaticClass(), TEXT("TypeIcon"));
	UHorizontalBoxSlot* IcSlot = Row->AddChildToHorizontalBox(TypeIcon);
	IcSlot->SetVerticalAlignment(VAlign_Center);
	IcSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

	CustomIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CustomIconImage"));
	CustomIconImage->SetBrush(ERUI::WhiteBrush(FVector2D(18.0f, 18.0f)));
	CustomIconImage->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* ImgSlot = Row->AddChildToHorizontalBox(CustomIconImage);
	ImgSlot->SetVerticalAlignment(VAlign_Center);
	ImgSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

	LabelText = ERUI::MakeText(this, LegendLabel, EERTypeScale::Micro, FLinearColor(0.74f, 0.82f, 0.94f, 0.70f));
	FSlateFontInfo FontInfo = T->GetFont(EERTypeScale::Micro);
	FontInfo.Size = 10;
	FontInfo.LetterSpacing = 40;
	LabelText->SetFont(FontInfo);
	Row->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);
}

void UERRouteLegendItem::SetupLegendItem(ERouteNodeType InType, const FText& InLabel, FLinearColor InAccent, UTexture2D* OptionalIcon)
{
	NodeType = InType;
	LegendLabel = InLabel;
	AccentColor = InAccent;
	CustomIcon = OptionalIcon;

	const bool bHasTex = IsValid(OptionalIcon);
	if (TypeIcon)
	{
		TypeIcon->SetIconParams(InType, InAccent);
		TypeIcon->SetVisibility(bHasTex ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (CustomIconImage)
	{
		CustomIconImage->SetVisibility(bHasTex ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasTex)
		{
			FSlateBrush B = CustomIconImage->GetBrush();
			B.SetResourceObject(OptionalIcon);
			CustomIconImage->SetBrush(B);
			CustomIconImage->SetColorAndOpacity(InAccent);
		}
	}
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
}

// ============================================================
// UERRouteNode
// ============================================================

TSharedRef<SWidget> UERRouteNode::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERRouteNode::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		bVisualInit = false;
		SetupNode(0, PreviewNodeType, PreviewGlyph, PreviewLabel, PreviewLabel, PreviewAccent, bPreviewFinal, PreviewIcon);
		SetNodeVisual(PreviewVisualState, bPreviewReachable);
	}
}

void UERRouteNode::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();
}

void UERRouteNode::BindDelegates()
{
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (HitButton)
	{
		HitButton->OnClicked.AddDynamic(this, &UERRouteNode::HandleClicked);
	}
}

void UERRouteNode::Build()
{
	const UERUITheme* T = ERUI::Theme();
	constexpr float BadgeBox = 88.0f;

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

	NodeSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NodeSizeBox"));
	NodeSizeBox->SetWidthOverride(ERRouteLayout::NodeW);
	NodeSizeBox->SetHeightOverride(ERRouteLayout::NodeH);
	HitButton->SetContent(NodeSizeBox);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NodeCol"));
	NodeSizeBox->SetContent(Col);

	BadgeSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BadgeSizeBox"));
	BadgeSizeBox->SetWidthOverride(BadgeBox);
	BadgeSizeBox->SetHeightOverride(BadgeBox);
	{
		UVerticalBoxSlot* BsSlot = Col->AddChildToVerticalBox(BadgeSizeBox);
		BsSlot->SetHorizontalAlignment(HAlign_Center);
		BsSlot->SetPadding(FMargin(0.0f, (ERRouteLayout::NodeH - BadgeBox) * 0.5f, 0.0f, 0.0f));
	}

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BadgeOverlay"));
	BadgeSizeBox->SetContent(Ov);

	Sigil = WidgetTree->ConstructWidget<UERRouteNodeSigil>(UERRouteNodeSigil::StaticClass(), TEXT("Sigil"));
	Sigil->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* SgSlot = Ov->AddChildToOverlay(Sigil);
		SgSlot->SetHorizontalAlignment(HAlign_Fill);
		SgSlot->SetVerticalAlignment(VAlign_Fill);
	}

	CustomIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CustomIconImage"));
	CustomIconImage->SetBrush(ERUI::WhiteBrush(FVector2D(36.0f, 36.0f)));
	CustomIconImage->SetVisibility(ESlateVisibility::Collapsed);
	{
		UOverlaySlot* IcSlot = Ov->AddChildToOverlay(CustomIconImage);
		IcSlot->SetHorizontalAlignment(HAlign_Center);
		IcSlot->SetVerticalAlignment(VAlign_Center);
	}

	GlyphText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, FLinearColor::White);
	{
		FSlateFontInfo GFont = T->GetFont(EERTypeScale::Title);
		GFont.Size = 23;
		GlyphText->SetFont(GFont);
		GlyphText->SetJustification(ETextJustify::Center);
		GlyphText->SetShadowOffset(FVector2D(0.0f, 1.0f));
		GlyphText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		UOverlaySlot* GlSlot = Ov->AddChildToOverlay(GlyphText);
		GlSlot->SetHorizontalAlignment(HAlign_Center);
		GlSlot->SetVerticalAlignment(VAlign_Center);
	}

	LabelText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, FLinearColor::White);
	{
		FSlateFontInfo LFont = T->GetFont(EERTypeScale::Micro);
		LFont.Size = 9;
		LFont.LetterSpacing = 40;
		LabelText->SetFont(LFont);
		LabelText->SetJustification(ETextJustify::Center);
		LabelText->SetShadowOffset(FVector2D(0.0f, 1.0f));
		LabelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		UVerticalBoxSlot* LbSlot = Col->AddChildToVerticalBox(LabelText);
		LbSlot->SetHorizontalAlignment(HAlign_Center);
		LbSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));
	}
}

void UERRouteNode::SetupNode(int32 InNodeId, ERouteNodeType InType, const FText& InGlyph, const FText& InLabel,
	const FText& InTooltip, FLinearColor InAccent, bool bInFinal, UTexture2D* OptionalIcon)
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}

	NodeId = InNodeId;
	Accent = InAccent;

	if (HitButton)
	{
		HitButton->SetToolTipText(InTooltip);
	}
	if (Sigil)
	{
		Sigil->SetNodeParams(InType, InAccent, bInFinal, InNodeId * 0.83f);
	}

	const bool bHasIcon = IsValid(OptionalIcon);
	if (CustomIconImage)
	{
		CustomIconImage->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasIcon)
		{
			FSlateBrush B = CustomIconImage->GetBrush();
			B.SetResourceObject(OptionalIcon);
			CustomIconImage->SetBrush(B);
		}
	}
	if (GlyphText)
	{
		GlyphText->SetText(InGlyph);
		GlyphText->SetVisibility((bHasIcon && bHideGlyphWhenIconValid) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}

	ApplyVisual();
}

void UERRouteNode::SetNodeVisual(ERouteNodeVisual InVisual, bool bInReachable)
{
	if (bVisualInit && Visual == InVisual && bReachable == bInReachable && !IsDesignTime())
	{
		return;
	}
	bVisualInit = true;
	Visual = InVisual;
	bReachable = bInReachable;
	ApplyVisual();

	if (!IsDesignTime())
	{
		BP_OnNodeVisualChanged(Visual, bReachable);
	}
}

void UERRouteNode::ApplyVisual()
{
	const FLinearColor Gold = ERRouteLayout::GoldColor();
	FLinearColor GlyphColor, LabelColor;

	switch (Visual)
	{
	case ERouteNodeVisual::Available:
		GlyphColor = FLinearColor(0.96f, 0.96f, 0.94f, 1.0f);
		LabelColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.92f);
		break;
	case ERouteNodeVisual::Current:
		GlyphColor = CurrentGlyphColor;
		LabelColor = CurrentLabelColor;
		break;
	case ERouteNodeVisual::Visited:
		GlyphColor = FLinearColor(Gold.R, Gold.G, Gold.B, 0.65f);
		LabelColor = FLinearColor(0.90f, 0.80f, 0.55f, 0.50f);
		break;
	default:
		GlyphColor = FLinearColor(0.92f, 0.92f, 0.90f, bReachable ? 0.56f : 0.24f);
		LabelColor = FLinearColor(0.92f, 0.92f, 0.90f, bReachable ? 0.42f : 0.18f);
		break;
	}

	if (Sigil)           { Sigil->SetVisualState(Visual, bReachable); }
	if (GlyphText)       { GlyphText->SetColorAndOpacity(FSlateColor(GlyphColor)); }
	if (CustomIconImage) { CustomIconImage->SetColorAndOpacity(GlyphColor); }
	if (LabelText)       { LabelText->SetColorAndOpacity(FSlateColor(LabelColor)); }
	if (HitButton)
	{
		HitButton->SetCursor(Visual == ERouteNodeVisual::Available ? EMouseCursor::Hand : EMouseCursor::Default);
	}
}

void UERRouteNode::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Sigil && HitButton)
	{
		Sigil->AdvanceInteraction(InDeltaTime, HitButton->IsHovered(), HitButton->IsPressed());
	}
}

void UERRouteNode::HandleClicked()
{
	OnRouteNodeClicked.Broadcast(NodeId);
}

UWidget* UERRouteNode::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
