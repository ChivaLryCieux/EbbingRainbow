// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERActionButton.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UERActionButton::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERActionButton::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativePreConstruct();

	if (LabelText && !Label.IsEmpty())
	{
		LabelText->SetText(Label);
	}
	if (ArrowText)
	{
		ArrowText->SetText(ArrowSymbol);
		ArrowText->SetVisibility(bShowArrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsDesignTime())
	{
		bEnabledState = bPreviewEnabled;
		bUrgent = bPreviewUrgent;
	}
	ApplyStyle();
}

void UERActionButton::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();

	if (LabelText && !Label.IsEmpty())
	{
		LabelText->SetText(Label);
	}
	if (ArrowText)
	{
		ArrowText->SetText(ArrowSymbol);
		ArrowText->SetVisibility(bShowArrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplyStyle();
}

void UERActionButton::BindDelegates()
{
	// 委托绑定独立于 Build(): WBP 提供 HitButton 时同样生效
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (HitButton)
	{
		HitButton->OnClicked.AddDynamic(this, &UERActionButton::HandleClicked);
	}
}

void UERActionButton::Build()
{
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("HitButton"));
	WidgetTree->RootWidget = HitButton;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	HitButton->SetContent(Row);

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Micro, T->Bone);
	Row->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);

	ArrowText = ERUI::MakeText(this, ArrowSymbol, EERTypeScale::Micro, T->BoneSecondary);
	UHorizontalBoxSlot* ArSlot = Row->AddChildToHorizontalBox(ArrowText);
	ArSlot->SetVerticalAlignment(VAlign_Center);
	ArSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

	ApplyStyle();
}

void UERActionButton::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
}

void UERActionButton::SetButtonEnabled(bool bEnabled)
{
	bEnabledState = bEnabled;
	if (HitButton)
	{
		HitButton->SetIsEnabled(bEnabled);
	}
	ApplyStyle();
}

void UERActionButton::SetUrgent(bool bInUrgent)
{
	if (bUrgent != bInUrgent)
	{
		bUrgent = bInUrgent;
		if (Anim_Urgent && !IsDesignTime())
		{
			if (bUrgent)
			{
				PlayAnimation(Anim_Urgent, 0.0f, 0);
			}
			else
			{
				StopAnimation(Anim_Urgent);
			}
		}
	}
	ApplyStyle();
}

void UERActionButton::ApplyStyle()
{
	const bool bHovered = HitButton && HitButton->IsHovered();
	if (!IsDesignTime())
	{
		BP_OnStateUpdated(bEnabledState, bUrgent, bHovered);
	}

	if (!bDriveStyleFromCode)
	{
		return;
	}

	const UERUITheme* T = ERUI::Theme();
	const FLinearColor BoneCol = bUseThemeColors ? T->Bone : CustomNormalColor;
	const FLinearColor AccentCol = bUseThemeColors ? T->Accent : CustomUrgentColor;
	const FLinearColor GhostCol = bUseThemeColors ? T->BoneGhost : ERUI::Fade(CustomDisabledColor, 0.6f);
	const FLinearColor SecondaryCol = bUseThemeColors ? T->BoneSecondary : ERUI::Fade(BoneCol, 0.6f);
	const FLinearColor TertiaryCol = bUseThemeColors ? T->BoneTertiary : CustomDisabledColor;

	FLinearColor Stroke;
	if (!bEnabledState)
	{
		Stroke = GhostCol;
	}
	else if (bUrgent)
	{
		const float Pulse = IsDesignTime() ? 1.0f : (0.55f + 0.45f * FMath::Sin(UrgentPhase * UrgentPulseSpeed));
		Stroke = ERUI::Fade(AccentCol, Pulse);
	}
	else
	{
		Stroke = bHovered ? SecondaryCol : GhostCol;
	}

	if (HitButton)
	{
		if (BorderImage)
		{
			BorderImage->SetColorAndOpacity(Stroke);
		}
		else
		{
			FButtonStyle Style;
			Style.SetNormal(ERUI::HairlineBoxBrush(Stroke));
			Style.SetHovered(ERUI::HairlineBoxBrush(bEnabledState ? BoneCol : GhostCol));
			Style.SetPressed(ERUI::HairlineBoxBrush(bEnabledState ? AccentCol : GhostCol));
			Style.SetDisabled(ERUI::HairlineBoxBrush(GhostCol));
			Style.SetNormalPadding(ContentPadding);
			Style.SetPressedPadding(ContentPadding);
			HitButton->SetStyle(Style);
		}
	}

	const FLinearColor TextCol = !bEnabledState ? TertiaryCol
		: bUrgent ? AccentCol : BoneCol;
	if (LabelText) { LabelText->SetColorAndOpacity(FSlateColor(TextCol)); }
	if (ArrowText)
	{
		ArrowText->SetVisibility(bShowArrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ArrowText->SetColorAndOpacity(FSlateColor(bEnabledState ? TextCol : TertiaryCol));
	}
}

void UERActionButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bUrgent)
	{
		UrgentPhase += InDeltaTime;
	}
	ApplyStyle();
}

void UERActionButton::HandleClicked()
{
	OnActionClicked.Broadcast();
}

UWidget* UERActionButton::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
