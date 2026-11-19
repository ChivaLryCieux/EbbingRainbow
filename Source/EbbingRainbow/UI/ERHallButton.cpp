// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHallButton.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

FSlateBrush UERHallButton::MakeRoundedBrush(float Radius, FLinearColor Fill, float OutlineWidth, FLinearColor Outline) const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	Brush.OutlineSettings.Width = OutlineWidth;
	Brush.OutlineSettings.Color = FSlateColor(Outline);
	return Brush;
}

TSharedRef<SWidget> UERHallButton::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERHallButton::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativePreConstruct();

	SyncStaticProperties();
	RefreshVisuals(0.0f, IsDesignTime());
}

void UERHallButton::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();

	SyncStaticProperties();
	RefreshVisuals(0.0f, false);
}

void UERHallButton::BindDelegates()
{
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (HitButton)
	{
		HitButton->OnClicked.AddDynamic(this, &UERHallButton::HandleClicked);
		HitButton->OnHovered.AddDynamic(this, &UERHallButton::HandleHovered);
		HitButton->OnUnhovered.AddDynamic(this, &UERHallButton::HandleUnhovered);
	}
}

void UERHallButton::Build()
{
	const UERUITheme* T = ERUI::Theme();

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

	ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ButtonSizeBox"));
	ButtonSizeBox->SetWidthOverride(ButtonWidth);
	HitButton->SetContent(ButtonSizeBox);

	GlowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GlowBorder"));
	GlowBorder->SetPadding(FMargin(11.0f, 10.0f));
	GlowBorder->SetHorizontalAlignment(HAlign_Fill);
	GlowBorder->SetVerticalAlignment(VAlign_Fill);
	ButtonSizeBox->SetContent(GlowBorder);

	BodyBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BodyBorder"));
	BodyBorder->SetPadding(FMargin(20.0f, 12.0f));
	BodyBorder->SetHorizontalAlignment(HAlign_Fill);
	BodyBorder->SetVerticalAlignment(VAlign_Fill);
	GlowBorder->SetContent(BodyBorder);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContentRow"));
	BodyBorder->SetContent(Row);

	USpacer* LeftSp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("LeftSpacer"));
	Row->AddChildToHorizontalBox(LeftSp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox* LedBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LedSizeBox"));
	LedBox->SetWidthOverride(9.0f);
	LedBox->SetHeightOverride(9.0f);
	LedBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LedBorder"));
	LedBorder->SetPadding(FMargin(0.0f));
	LedBox->SetContent(LedBorder);
	UHorizontalBoxSlot* LedSlot = Row->AddChildToHorizontalBox(LedBox);
	LedSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextCol"));
	UHorizontalBoxSlot* TcSlot = Row->AddChildToHorizontalBox(TextCol);
	TcSlot->SetVerticalAlignment(VAlign_Center);
	TcSlot->SetPadding(FMargin(13.0f, 0.0f, 0.0f, 0.0f));

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Body, FLinearColor(0.96f, 0.98f, 1.0f, 0.96f));
	FSlateFontInfo LblFont = T->GetFont(EERTypeScale::Body);
	LblFont.Size = 15;
	LabelText->SetFont(LblFont);
	TextCol->AddChildToVerticalBox(LabelText);

	CaptionText = ERUI::MakeText(this, Caption, EERTypeScale::Micro, FLinearColor(0.78f, 0.85f, 0.95f, 0.58f), true);
	FSlateFontInfo CapFont = T->GetFont(EERTypeScale::Micro, true);
	CapFont.Size = 8;
	CapFont.LetterSpacing = 120;
	CaptionText->SetFont(CapFont);
	TextCol->AddChildToVerticalBox(CaptionText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

	USpacer* RightSp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("RightSpacer"));
	Row->AddChildToHorizontalBox(RightSp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

void UERHallButton::SyncStaticProperties()
{
	if (LabelText)   { LabelText->SetText(Label); }
	if (CaptionText) { CaptionText->SetText(Caption); }
	if (ButtonSizeBox)
	{
		if (bApplyWidthOverride)
		{
			ButtonSizeBox->SetWidthOverride(ButtonWidth);
		}
		else
		{
			ButtonSizeBox->ClearWidthOverride();
		}
	}
}

void UERHallButton::SetButtonTexts(const FText& InLabel, const FText& InCaption)
{
	Label = InLabel;
	Caption = InCaption;
	SyncStaticProperties();
}

void UERHallButton::SetAccentAndPrimary(FLinearColor InAccent, bool bInPrimary)
{
	AccentColor = InAccent;
	bPrimary = bInPrimary;
	RefreshVisuals(0.0f, IsDesignTime());
}

void UERHallButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ElapsedTime += InDeltaTime;
	RefreshVisuals(InDeltaTime, false);
}

void UERHallButton::RefreshVisuals(float DeltaTime, bool bDesignPreview)
{
	if (!bDriveGlassAnimationFromCode)
	{
		return;
	}

	const bool bHovered = !bDesignPreview && HitButton && HitButton->IsHovered();
	const bool bPressed = !bDesignPreview && HitButton && HitButton->IsPressed();

	if (DeltaTime > 0.0f)
	{
		HoverAlpha = FMath::FInterpTo(HoverAlpha, bHovered ? 1.0f : 0.0f, DeltaTime, HoverInterpSpeed);
		PressAlpha = FMath::FInterpTo(PressAlpha, bPressed ? 1.0f : 0.0f, DeltaTime, PressInterpSpeed);
	}
	else
	{
		HoverAlpha = 0.0f;
		PressAlpha = 0.0f;
	}

	const float Breath = bPrimary
		? (bDesignPreview ? 0.65f : (0.5f + 0.5f * FMath::Sin(ElapsedTime * BreathSpeed)))
		: 0.0f;

	if (GlowBorder)
	{
		const float GlowAlpha = bPrimary ? (0.06f + 0.04f * Breath + 0.13f * HoverAlpha) : 0.0f;
		GlowBorder->SetBrush(MakeRoundedBrush(GlowCornerRadius, AccentColor.CopyWithNewOpacity(GlowAlpha), 0.0f, FLinearColor::Transparent));
		GlowBorder->SetBrushColor(FLinearColor::White);

		const float Scale = FMath::Lerp(1.0f, PressedScale, PressAlpha);
		GlowBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		GlowBorder->SetRenderScale(FVector2D(Scale, Scale));
	}

	if (BodyBorder)
	{
		FLinearColor Glass = FMath::Lerp(BaseGlassColor, HoverGlassColor, HoverAlpha);
		if (bPrimary)
		{
			const FLinearColor AccentGlass(
				FMath::Lerp(0.09f, 0.13f, Breath),
				FMath::Lerp(0.20f, 0.26f, Breath),
				0.34f,
				0.55f);
			Glass = FMath::Lerp(Glass, AccentGlass, 0.40f * HoverAlpha);
		}
		const float OutAlpha = 0.20f + 0.30f * HoverAlpha;
		BodyBorder->SetBrush(MakeRoundedBrush(BodyCornerRadius, Glass, 1.1f, OutlineColor.CopyWithNewOpacity(OutAlpha)));
		BodyBorder->SetBrushColor(FLinearColor::White);
	}

	if (LedBorder)
	{
		if (bPrimary)
		{
			const float LedAlpha = FMath::Clamp(0.45f + 0.55f * Breath + 0.20f * HoverAlpha, 0.0f, 1.0f);
			LedBorder->SetBrush(MakeRoundedBrush(5.0f, AccentColor.CopyWithNewOpacity(LedAlpha), 1.2f, AccentColor.CopyWithNewOpacity(0.85f)));
		}
		else
		{
			LedBorder->SetBrush(MakeRoundedBrush(5.0f, FLinearColor::Transparent, 1.2f, AccentColor.CopyWithNewOpacity(0.45f + 0.35f * HoverAlpha)));
		}
		LedBorder->SetBrushColor(FLinearColor::White);
	}
}

void UERHallButton::HandleClicked()
{
	OnButtonClicked.Broadcast();
}

void UERHallButton::HandleHovered()
{
	if (Anim_Hover && !IsDesignTime())
	{
		PlayAnimationForward(Anim_Hover);
	}
	BP_OnHoverChanged(true);
}

void UERHallButton::HandleUnhovered()
{
	if (Anim_Hover && !IsDesignTime())
	{
		PlayAnimationReverse(Anim_Hover);
	}
	BP_OnHoverChanged(false);
}

UWidget* UERHallButton::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
