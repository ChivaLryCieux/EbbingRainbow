// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERActionButton.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

void UERActionButton::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativeConstruct();
}

void UERActionButton::Build()
{
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	HitButton->OnClicked.AddDynamic(this, &UERActionButton::HandleClicked);
	WidgetTree->RootWidget = HitButton;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HitButton->SetContent(Row);

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Micro, T->Bone);
	Row->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);

	ArrowText = ERUI::MakeText(this, FText::FromString(TEXT("→")), EERTypeScale::Micro, T->BoneSecondary);
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
	bUrgent = bInUrgent;
	ApplyStyle();
}

void UERActionButton::ApplyStyle()
{
	if (!HitButton)
	{
		return;
	}
	const UERUITheme* T = ERUI::Theme();

	FLinearColor Stroke;
	if (!bEnabledState)
	{
		Stroke = T->BoneGhost;
	}
	else if (bUrgent)
	{
		const float Pulse = 0.55f + 0.45f * FMath::Sin(UrgentPhase * 4.0f);
		Stroke = ERUI::Fade(T->Accent, Pulse);
	}
	else
	{
		Stroke = HitButton->IsHovered() ? T->BoneSecondary : T->BoneGhost;
	}

	FButtonStyle Style;
	const FSlateBrush Normal = ERUI::HairlineBoxBrush(Stroke);
	Style.SetNormal(Normal);
	Style.SetHovered(ERUI::HairlineBoxBrush(bEnabledState ? T->Bone : T->BoneGhost));
	Style.SetPressed(ERUI::HairlineBoxBrush(bEnabledState ? T->Accent : T->BoneGhost));
	Style.SetDisabled(ERUI::HairlineBoxBrush(T->BoneGhost));
	const FMargin Pad(18.0f, 10.0f);
	Style.SetNormalPadding(Pad);
	Style.SetPressedPadding(Pad);
	HitButton->SetStyle(Style);

	const FLinearColor TextCol = !bEnabledState ? T->BoneTertiary
		: bUrgent ? T->Accent : T->Bone;
	if (LabelText) { LabelText->SetColorAndOpacity(FSlateColor(TextCol)); }
	if (ArrowText) { ArrowText->SetColorAndOpacity(FSlateColor(bEnabledState ? TextCol : T->BoneTertiary)); }
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
