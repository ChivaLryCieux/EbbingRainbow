// Copyright Epic Games, Inc. All rights reserved.

#include "UI/ERPersonnelMemberRow.h"

#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UERPersonnelMemberRow::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERPersonnelMemberRow::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		FHallHeroProfile Preview;
		Preview.HeroName = PreviewName;
		Preview.RoleTitle = FText::GetEmpty();
		Preview.School = EDamageType::Kinetic;
		Accent = ERHallRoster::GetSchoolColor(Preview.School);
		if (MemberName) { MemberName->SetText(PreviewName); }
		if (MemberSchool) { MemberSchool->SetText(PreviewSchool); }
		if (MemberIntegrity) { MemberIntegrity->SetText(PreviewIntegrity); }
		if (SchoolDot) { SchoolDot->SetBrush(ERUI::RoundedBrush(3.0f, Accent)); }
		bSelected = bPreviewSelected;
		RefreshVisuals(0.0f, true);
	}
}

void UERPersonnelMemberRow::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}
	if (!bDelegatesBound)
	{
		bDelegatesBound = true;
		if (HitButton)
		{
			HitButton->OnClicked.AddUniqueDynamic(this, &UERPersonnelMemberRow::HandleClicked);
		}
	}
	Super::NativeConstruct();
	RefreshVisuals(0.0f, false);
}

void UERPersonnelMemberRow::Build()
{
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("HitButton"));
	FButtonStyle Style;
	const FSlateBrush Transparent = ERUI::TransparentBrush();
	Style.SetNormal(Transparent);
	Style.SetHovered(Transparent);
	Style.SetPressed(Transparent);
	Style.SetDisabled(Transparent);
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f));
	HitButton->SetStyle(Style);
	HitButton->SetCursor(EMouseCursor::Hand);
	WidgetTree->RootWidget = HitButton;

	USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RowSize"));
	RowSize->SetHeightOverride(86.0f);
	HitButton->SetContent(RowSize);

	RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RowBorder"));
	RowBorder->SetPadding(FMargin(10.0f, 8.0f));
	RowBorder->SetHorizontalAlignment(HAlign_Fill);
	RowBorder->SetVerticalAlignment(VAlign_Fill);
	RowSize->SetContent(RowBorder);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RowContent"));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RowBorder->SetContent(Row);

	SelectRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SelectRule"));
	SelectRule->SetBrush(ERUI::WhiteBrush(FVector2D(2.0f, 40.0f)));
	UHorizontalBoxSlot* RuleSlot = Row->AddChildToHorizontalBox(SelectRule);
	RuleSlot->SetVerticalAlignment(VAlign_Center);
	RuleSlot->SetPadding(FMargin(0.0f, 0.0f, 9.0f, 0.0f));

	ThumbFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ThumbFrame"));
	ThumbFrame->SetPadding(FMargin(0.0f));
	ThumbFrame->SetHorizontalAlignment(HAlign_Fill);
	ThumbFrame->SetVerticalAlignment(VAlign_Fill);
	{
		USizeBox* ThumbSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ThumbSize"));
		ThumbSize->SetWidthOverride(52.0f);
		ThumbSize->SetHeightOverride(64.0f);
		ThumbSize->SetContent(ThumbFrame);
		UHorizontalBoxSlot* ThumbSlot = Row->AddChildToHorizontalBox(ThumbSize);
		ThumbSlot->SetVerticalAlignment(VAlign_Center);
	}
	PortraitThumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitThumb"));
	PortraitThumb->SetBrush(ERUI::WhiteBrush());
	PortraitThumb->SetColorAndOpacity(FLinearColor(0.10f, 0.12f, 0.15f, 1.0f));
	PortraitThumb->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ThumbFrame->SetContent(PortraitThumb);

	UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextCol"));
	TextCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextCol);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(12.0f, 0.0f, 6.0f, 0.0f));
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	MemberName = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->Bone);
	FSlateFontInfo NameFont = T->GetFont(EERTypeScale::Body);
	NameFont.Size = 16;
	MemberName->SetFont(NameFont);
	TextCol->AddChildToVerticalBox(MemberName);

	UHorizontalBox* SchoolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SchoolRow"));
	SchoolRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TextCol->AddChildToVerticalBox(SchoolRow)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	SchoolDot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SchoolDot"));
	SchoolDot->SetPadding(FMargin(0.0f));
	SchoolDot->SetBrush(ERUI::RoundedBrush(3.0f, Accent));
	{
		USizeBox* DotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SchoolDotSize"));
		DotSize->SetWidthOverride(6.0f);
		DotSize->SetHeightOverride(6.0f);
		DotSize->SetContent(SchoolDot);
		UHorizontalBoxSlot* DotSlot = SchoolRow->AddChildToHorizontalBox(DotSize);
		DotSlot->SetVerticalAlignment(VAlign_Center);
		DotSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}

	MemberSchool = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	FSlateFontInfo SchoolFont = T->GetFont(EERTypeScale::Micro);
	SchoolFont.Size = 9;
	SchoolFont.LetterSpacing = 80;
	MemberSchool->SetFont(SchoolFont);
	SchoolRow->AddChildToHorizontalBox(MemberSchool)->SetVerticalAlignment(VAlign_Center);

	MemberIntegrity = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary, true);
	MemberIntegrity->SetJustification(ETextJustify::Right);
	UHorizontalBoxSlot* IntegritySlot = Row->AddChildToHorizontalBox(MemberIntegrity);
	IntegritySlot->SetVerticalAlignment(VAlign_Center);
}

void UERPersonnelMemberRow::Setup(int32 InIndex, const FHallHeroProfile& Profile, float CurrentIntegrity, bool bHasRunState, bool bInSelected)
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(); }
	}

	MemberIndex = InIndex;
	Accent = Profile.GetSchoolColor();

	const float MaxIntegrity = FMath::Max(1.0f, Profile.Stats.MaxIntegrity);
	const float ShownIntegrity = bHasRunState ? FMath::Clamp(CurrentIntegrity, 0.0f, MaxIntegrity) : MaxIntegrity;
	IntegrityAlpha = FMath::Clamp(ShownIntegrity / MaxIntegrity, 0.0f, 1.0f);
	bLowIntegrity = IntegrityAlpha < 0.35f;

	if (MemberName) { MemberName->SetText(Profile.HeroName); }
	if (MemberSchool)
	{
		MemberSchool->SetText(FText::FromString(FString::Printf(TEXT("%s 路 %s"),
			*Profile.GetSchoolName().ToString(), *Profile.RoleTitle.ToString())));
	}
	if (MemberIntegrity)
	{
		MemberIntegrity->SetText(FText::FromString(bHasRunState
			? FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(ShownIntegrity), FMath::RoundToInt(MaxIntegrity))
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(MaxIntegrity))));
	}
	if (SchoolDot)
	{
		SchoolDot->SetBrush(ERUI::RoundedBrush(3.0f, Accent));
	}

	UTexture2D* PortraitTexture = Profile.Portrait.IsNull() ? nullptr : Profile.Portrait.LoadSynchronous();
	SetPortrait(PortraitTexture ? PortraitTexture : DefaultPortrait.Get());
	SetSelected(bInSelected);
}

void UERPersonnelMemberRow::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	RefreshVisuals(0.0f, IsDesignTime());
}

void UERPersonnelMemberRow::SetPortrait(UTexture2D* InTexture)
{
	if (!PortraitThumb)
	{
		return;
	}
	if (InTexture)
	{
		PortraitThumb->SetBrushFromTexture(InTexture, false);
		PortraitThumb->SetColorAndOpacity(FLinearColor::White);
	}
	else
	{
		PortraitThumb->SetBrush(ERUI::WhiteBrush());
		PortraitThumb->SetColorAndOpacity(FLinearColor(0.10f, 0.12f, 0.15f, 1.0f));
	}
}

void UERPersonnelMemberRow::RefreshVisuals(float DeltaTime, bool bDesignPreview)
{
	const UERUITheme* T = ERUI::Theme();
	const bool bIsHovered = !bDesignPreview && bDriveHoverFromCode && HitButton && HitButton->IsHovered();

	if (DeltaTime > 0.0f)
	{
		HoverAlpha = FMath::FInterpTo(HoverAlpha, bIsHovered ? 1.0f : 0.0f, DeltaTime, HoverInterpSpeed);
	}
	else
	{
		HoverAlpha = bIsHovered ? 1.0f : 0.0f;
	}

	if (RowBorder)
	{
		FLinearColor Fill = FLinearColor(0.055f, 0.075f, 0.105f, 0.30f + 0.16f * HoverAlpha);
		if (bSelected)
		{
			Fill = FLinearColor(
				FMath::Lerp(0.075f, Accent.R * 0.30f, 0.45f),
				FMath::Lerp(0.105f, Accent.G * 0.30f, 0.45f),
				FMath::Lerp(0.150f, Accent.B * 0.30f, 0.45f),
				0.52f);
		}
		const FLinearColor Outline = bSelected
			? Accent.CopyWithNewOpacity(0.52f)
			: FLinearColor(0.90f, 0.95f, 1.0f, 0.08f + 0.10f * HoverAlpha);
		RowBorder->SetBrush(ERUI::RoundedBrush(10.0f, Fill, 1.0f, Outline));
		RowBorder->SetBrushColor(FLinearColor::White);
	}

	if (SelectRule)
	{
		SelectRule->SetColorAndOpacity(Accent.CopyWithNewOpacity(bSelected ? 1.0f : 0.0f));
	}

	if (ThumbFrame)
	{
		const FLinearColor ThumbOutline = bSelected
			? Accent.CopyWithNewOpacity(0.42f)
			: FLinearColor(1.0f, 1.0f, 1.0f, 0.10f + 0.08f * HoverAlpha);
		ThumbFrame->SetBrush(ERUI::RoundedBrush(2.0f, FLinearColor(0.02f, 0.03f, 0.04f, 0.85f), 1.0f, ThumbOutline));
		ThumbFrame->SetBrushColor(FLinearColor::White);
	}

	if (MemberName)
	{
		MemberName->SetColorAndOpacity(FSlateColor(bSelected
			? T->Bone
			: ERUI::Fade(T->Bone, 0.58f + 0.22f * HoverAlpha)));
	}
	if (MemberSchool)
	{
		MemberSchool->SetColorAndOpacity(FSlateColor(ERUI::Fade(T->BoneTertiary, bSelected ? 1.25f : 1.0f)));
	}
	if (MemberIntegrity)
	{
		const FLinearColor IntegrityTint = bLowIntegrity
			? T->DangerColor
			: (bSelected ? T->BoneSecondary : ERUI::Fade(T->BoneTertiary, 1.1f));
		MemberIntegrity->SetColorAndOpacity(FSlateColor(IntegrityTint));
	}
}

void UERPersonnelMemberRow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshVisuals(InDeltaTime, false);
}

void UERPersonnelMemberRow::HandleClicked()
{
	OnMemberSelected.Broadcast(MemberIndex);
}

UWidget* UERPersonnelMemberRow::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
