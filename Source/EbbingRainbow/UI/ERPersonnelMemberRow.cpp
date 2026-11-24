// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERPersonnelMemberRow.h"

#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
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
		Accent = ERHallRoster::GetSchoolColor(EDamageType::Kinetic);
		if (MemberIndexText) { MemberIndexText->SetText(INVTEXT("01")); }
		if (MemberName)      { MemberName->SetText(PreviewName); }
		if (MemberSchool)    { MemberSchool->SetText(PreviewSchool); }
		if (MemberIntegrity) { MemberIntegrity->SetText(PreviewIntegrity); }
		if (SchoolDot)       { SchoolDot->SetBrush(ERUI::RoundedBrush(0.0f, Accent)); }
		bSelected = bPreviewSelected;
		SelectSpring.SnapTo(bSelected ? 1.0f : 0.0f);
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
	HoverSpring.SnapTo(0.0f);
	SelectSpring.SnapTo(bSelected ? 1.0f : 0.0f);
	SweepProgress = 1.0f;
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
	RowSize->SetWidthOverride(198.0f);
	RowSize->SetHeightOverride(72.0f);
	HitButton->SetContent(RowSize);

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RowOverlay"));
	Ov->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RowSize->SetContent(Ov);

	// 切角机能底板 (右上 10px 切角 + 左侧 4px 学派点睛条)
	RowFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("RowFrame"));
	RowFrame->Chamfer = FVector4(0.0f, 10.0f, 0.0f, 0.0f);
	RowFrame->BorderThickness = 1.0f;
	RowFrame->bDrawCornerBrackets = false;
	RowFrame->AccentEdge = EERAccentEdge::Left;
	RowFrame->AccentThickness = 4.0f;
	RowFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(RowFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RowContent"));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(Row);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(14.0f, 8.0f, 14.0f, 8.0f));
	}

	ThumbFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ThumbFrame"));
	ThumbFrame->SetPadding(FMargin(1.0f));
	ThumbFrame->SetHorizontalAlignment(HAlign_Fill);
	ThumbFrame->SetVerticalAlignment(VAlign_Fill);
	{
		USizeBox* ThumbSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ThumbSize"));
		ThumbSize->SetWidthOverride(50.0f);
		ThumbSize->SetHeightOverride(64.0f);
		ThumbSize->SetContent(ThumbFrame);
		UHorizontalBoxSlot* ThumbSlot = Row->AddChildToHorizontalBox(ThumbSize);
		ThumbSlot->SetVerticalAlignment(VAlign_Center);
	}
	PortraitThumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitThumb"));
	PortraitThumb->SetBrush(ERUI::WhiteBrush());
	PortraitThumb->SetColorAndOpacity(FLinearColor(0.08f, 0.09f, 0.11f, 1.0f));
	PortraitThumb->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ThumbFrame->SetContent(PortraitThumb);

	UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextCol"));
	TextCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextCol);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(12.0f, 0.0f, 6.0f, 0.0f));
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// 编号 + 粗体名字行
	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NameRow"));
	NameRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TextCol->AddChildToVerticalBox(NameRow);

	MemberIndexText = ERUI::MakeText(this, INVTEXT("01"), EERTypeScale::Micro, T->BoneTertiary, true);
	MemberIndexText->SetFont(ERUI::FontBold(13, 100));
	{
		UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(MemberIndexText);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 2.0f));
	}

	MemberName = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone);
	MemberName->SetFont(ERUI::FontBold(24, 20));
	NameRow->AddChildToHorizontalBox(MemberName)->SetVerticalAlignment(VAlign_Bottom);

	UHorizontalBox* SchoolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SchoolRow"));
	SchoolRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TextCol->AddChildToVerticalBox(SchoolRow)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	SchoolDot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SchoolDot"));
	SchoolDot->SetPadding(FMargin(0.0f));
	SchoolDot->SetBrush(ERUI::RoundedBrush(0.0f, Accent));
	{
		USizeBox* DotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SchoolDotSize"));
		DotSize->SetWidthOverride(5.0f);
		DotSize->SetHeightOverride(5.0f);
		DotSize->SetContent(SchoolDot);
		UHorizontalBoxSlot* DotSlot = SchoolRow->AddChildToHorizontalBox(DotSize);
		DotSlot->SetVerticalAlignment(VAlign_Center);
		DotSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}

	MemberSchool = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	MemberSchool->SetFont(ERUI::FontBold(13, 60));
	SchoolRow->AddChildToHorizontalBox(MemberSchool)->SetVerticalAlignment(VAlign_Center);

	MemberIntegrity = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->BoneSecondary, true);
	MemberIntegrity->SetFont(ERUI::FontBold(21, 0));
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

	if (MemberIndexText)
	{
		MemberIndexText->SetText(FText::FromString(FString::Printf(TEXT("%02d"), InIndex + 1)));
	}
	if (MemberName) { MemberName->SetText(Profile.HeroName); }
	if (MemberSchool)
	{
		// 修复中点编码
		MemberSchool->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"),
			*Profile.GetSchoolName().ToString(), *Profile.RoleTitle.ToString())));
	}
	if (MemberIntegrity)
	{
		MemberIntegrity->SetText(FText::FromString(bHasRunState
			? FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(ShownIntegrity), FMath::RoundToInt(MaxIntegrity))
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(MaxIntegrity))));
	}
	if (SchoolDot)
	{
		SchoolDot->SetBrush(ERUI::RoundedBrush(0.0f, Accent));
	}

	UTexture2D* PortraitTexture = Profile.Portrait.IsNull() ? nullptr : Profile.Portrait.LoadSynchronous();
	SetPortrait(PortraitTexture ? PortraitTexture : DefaultPortrait.Get());
	SetSelected(bInSelected);
}

void UERPersonnelMemberRow::SetSelected(bool bInSelected)
{
	if (bInSelected && !bSelected)
	{
		SweepProgress = 0.01f;
	}
	bSelected = bInSelected;
	SelectSpring.SetTarget(bSelected ? 1.0f : 0.0f);
	if (IsDesignTime())
	{
		SelectSpring.SnapTo(bSelected ? 1.0f : 0.0f);
	}
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
		PortraitThumb->SetColorAndOpacity(FLinearColor(0.08f, 0.09f, 0.11f, 1.0f));
	}
}

void UERPersonnelMemberRow::RefreshVisuals(float DeltaTime, bool bDesignPreview)
{
	const UERUITheme* T = ERUI::Theme();
	const bool bIsHovered = !bDesignPreview && bDriveHoverFromCode && HitButton && HitButton->IsHovered();
	if (bIsHovered && !bWasHovered)
	{
		SweepProgress = 0.01f;
	}
	bWasHovered = bIsHovered;

	HoverSpring.SetTarget(bIsHovered ? 1.0f : 0.0f);
	SelectSpring.SetTarget(bSelected ? 1.0f : 0.0f);

	const float HoverA = (DeltaTime > 0.0f) ? HoverSpring.Tick(DeltaTime) : (bIsHovered ? 1.0f : 0.0f);
	const float SelA   = (DeltaTime > 0.0f) ? SelectSpring.Tick(DeltaTime) : (bSelected ? 1.0f : 0.0f);
	const float H01    = FMath::Clamp(HoverA, 0.0f, 1.0f);
	const float S01    = FMath::Clamp(SelA, 0.0f, 1.0f);

	if (DeltaTime > 0.0f && SweepProgress < 1.0f)
	{
		SweepProgress = FMath::Min(1.0f, SweepProgress + DeltaTime / 0.36f);
	}

	// 统一「白底墨字彩条」: 骨白底 + 学派点睛条, 选中 = 加粗墨框 + 微抬
	if (RowFrame)
	{
		ERUI::StyleBoneFrame(RowFrame, FMath::Max(H01, S01), true, S01 > 0.5f, Accent, false);
		RowFrame->SetMotionState(1.0f, FMath::Max(H01, S01 * 0.5f), SweepProgress);
	}
	if (RowBorder)
	{
		RowBorder->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.93f, 0.93f, 0.91f, 0.96f), 1.0f, FLinearColor(0.02f, 0.02f, 0.03f, 0.4f)));
		RowBorder->SetBrushColor(FLinearColor::White);
	}
	if (SelectRule)
	{
		SelectRule->SetColorAndOpacity(Accent);
	}

	if (ThumbFrame)
	{
		ThumbFrame->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.009f, 0.012f, 0.95f), 1.0f, FLinearColor(0.02f, 0.02f, 0.03f, 0.55f + 0.3f * S01)));
		ThumbFrame->SetBrushColor(FLinearColor::White);
	}

	const FLinearColor NameCol = ERUI::InkText();
	const FLinearColor SubCol  = ERUI::InkText(0.66f);
	const FLinearColor IdxCol  = ERUI::InkText(0.5f);
	const FLinearColor IntCol  = bLowIntegrity ? FLinearColor(0.62f, 0.04f, 0.02f, 1.0f) : ERUI::InkText(0.8f);
	if (MemberIndexText) { MemberIndexText->SetColorAndOpacity(FSlateColor(IdxCol)); }
	if (MemberName)      { MemberName->SetColorAndOpacity(FSlateColor(NameCol)); }
	if (MemberSchool)    { MemberSchool->SetColorAndOpacity(FSlateColor(SubCol)); }
	if (MemberIntegrity) { MemberIntegrity->SetColorAndOpacity(FSlateColor(IntCol)); }

	// 弹簧微抬 (-6px 选中 / -3px 悬停, 适配全屏左下横向浮动编队切换条)
	SetRenderTranslation(FVector2D(0.0f, -6.0f * SelA - 3.0f * HoverA * (1.0f - S01)));
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
