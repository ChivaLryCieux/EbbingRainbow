// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERActionButton.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
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
	ApplyStyle(0.0f);
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

	HoverSpring.SnapTo(0.0f);
	PressSpring.SnapTo(0.0f);
	SweepProgress = 1.0f;

	if (LabelText && !Label.IsEmpty())
	{
		LabelText->SetText(Label);
	}
	if (ArrowText)
	{
		ArrowText->SetText(ArrowSymbol);
		ArrowText->SetVisibility(bShowArrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplyStyle(0.0f);
}

void UERActionButton::BindDelegates()
{
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
	{
		FButtonStyle TransparentStyle;
		const FSlateBrush Tr = ERUI::TransparentBrush();
		TransparentStyle.SetNormal(Tr);
		TransparentStyle.SetHovered(Tr);
		TransparentStyle.SetPressed(Tr);
		TransparentStyle.SetDisabled(Tr);
		TransparentStyle.SetNormalPadding(FMargin(0.0f));
		TransparentStyle.SetPressedPadding(FMargin(0.0f));
		HitButton->SetStyle(TransparentStyle);
		HitButton->SetCursor(EMouseCursor::Hand);
	}
	WidgetTree->RootWidget = HitButton;

	ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ButtonSizeBox"));
	if (MinButtonWidth > 0.0f) { ButtonSizeBox->SetMinDesiredWidth(MinButtonWidth); }
	if (MinButtonHeight > 0.0f) { ButtonSizeBox->SetMinDesiredHeight(MinButtonHeight); }
	HitButton->SetContent(ButtonSizeBox);

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ButtonOverlay"));
	Ov->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonSizeBox->SetContent(Ov);

	// 「白底墨字彩条」切角底板: 反白石膏 + 墨黑描边 + 四角刻度 + 左侧点睛条
	ButtonFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("ButtonFrame"));
	ButtonFrame->Chamfer = FVector4(0.0f, 12.0f, 0.0f, 12.0f);
	ButtonFrame->BorderThickness = 1.0f;
	ButtonFrame->bDrawCornerBrackets = true;
	ButtonFrame->BracketLength = 8.0f;
	ButtonFrame->BracketThickness = 1.4f;
	ButtonFrame->AccentEdge = EERAccentEdge::Left;
	ButtonFrame->AccentThickness = 4.0f;
	ButtonFrame->AccentColor = AccentColor;
	ButtonFrame->bUsePrismAccent = bPrismAccent;
	ButtonFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ERUI::StyleBoneFrame(ButtonFrame, 0.0f, true, false, AccentColor, bPrismAccent);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(ButtonFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	ContentRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(ContentRow);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(FontSize * 1.1f + 8.0f, FontSize * 0.5f + 4.0f));
	}

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Body, ERUI::InkText());
	LabelText->SetFont(ERUI::FontBold(FontSize, 80));
	ContentRow->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);

	ArrowText = ERUI::MakeText(this, ArrowSymbol, EERTypeScale::Body, ERUI::InkText(0.62f), true);
	ArrowText->SetFont(ERUI::FontBold(FMath::Max(14, FontSize - 2), 0));
	UHorizontalBoxSlot* ArSlot = ContentRow->AddChildToHorizontalBox(ArrowText);
	ArSlot->SetVerticalAlignment(VAlign_Center);
	ArSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));

	ApplyStyle(0.0f);
}

UERActionButton* UERActionButton::Make(UUserWidget* Owner, const FText& InLabel, int32 InFontSize,
	bool bInShowArrow, bool bInPrism)
{
	UERActionButton* B = Owner->WidgetTree->ConstructWidget<UERActionButton>(UERActionButton::StaticClass());
	B->Label = InLabel;
	B->FontSize = InFontSize;
	B->bShowArrow = bInShowArrow;
	B->bPrismAccent = bInPrism;
	return B;
}

void UERActionButton::SetAccentColor(FLinearColor InAccent, bool bPrism)
{
	AccentColor = InAccent;
	bPrismAccent = bPrism;
	ApplyStyle(0.0f);
}

void UERActionButton::SetFontSize(int32 InFontSize)
{
	FontSize = InFontSize;
	if (LabelText) { LabelText->SetFont(ERUI::FontBold(FontSize, 80)); }
	if (ArrowText) { ArrowText->SetFont(ERUI::FontBold(FMath::Max(14, FontSize - 2), 0)); }
	if (ContentRow)
	{
		if (UOverlaySlot* S = Cast<UOverlaySlot>(ContentRow->Slot))
		{
			S->SetPadding(FMargin(FontSize * 1.1f + 8.0f, FontSize * 0.5f + 4.0f));
		}
	}
}

void UERActionButton::SetMinSize(float InMinWidth, float InMinHeight)
{
	MinButtonWidth = InMinWidth;
	MinButtonHeight = InMinHeight;
	if (!ButtonSizeBox) { return; }
	if (InMinWidth > 0.0f) { ButtonSizeBox->SetMinDesiredWidth(InMinWidth); } else { ButtonSizeBox->ClearMinDesiredWidth(); }
	if (InMinHeight > 0.0f) { ButtonSizeBox->SetMinDesiredHeight(InMinHeight); } else { ButtonSizeBox->ClearMinDesiredHeight(); }
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
	ApplyStyle(0.0f);
}

void UERActionButton::SetUrgent(bool bInUrgent)
{
	if (bUrgent != bInUrgent)
	{
		bUrgent = bInUrgent;
		if (bUrgent)
		{
			SweepProgress = 0.01f;
		}
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
	ApplyStyle(0.0f);
}

void UERActionButton::ApplyStyle(float DeltaTime)
{
	const bool bHovered = bEnabledState && HitButton && HitButton->IsHovered();
	const bool bPressed = bEnabledState && HitButton && HitButton->IsPressed();

	if (bHovered && !bWasHovered)
	{
		SweepProgress = 0.01f;
	}
	bWasHovered = bHovered;

	HoverSpring.SetTarget(bHovered ? 1.0f : 0.0f);
	PressSpring.SetTarget(bPressed ? 1.0f : 0.0f);
	const float HoverA = (DeltaTime > 0.0f) ? HoverSpring.Tick(DeltaTime) : (bHovered ? 1.0f : 0.0f);
	const float PressA = (DeltaTime > 0.0f) ? PressSpring.Tick(DeltaTime) : 0.0f;
	const float H01 = FMath::Clamp(HoverA, 0.0f, 1.0f);

	if (DeltaTime > 0.0f && SweepProgress < 1.0f)
	{
		SweepProgress = FMath::Min(1.0f, SweepProgress + DeltaTime / 0.38f);
	}

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
	const FLinearColor TertiaryCol = bUseThemeColors ? T->BoneTertiary : CustomDisabledColor;

	const float Pulse = IsDesignTime() ? 1.0f : ERMotion::SolemnPulse(UrgentPhase, UrgentPulseSpeed * 0.65f);

	if (ButtonFrame)
	{
		// 统一「白底墨字彩条」: 紧急态改用折射暖金单色条并随呼吸提亮, 其余用本按钮的点睛色/光谱条
		const float Hover = bUrgent ? FMath::Max(H01, Pulse * 0.4f) : H01;
		ERUI::StyleBoneFrame(ButtonFrame, Hover, bEnabledState, bUrgent,
			bUrgent ? T->Accent : AccentColor, bUrgent ? false : bPrismAccent);
		ButtonFrame->SetMotionState(1.0f, Hover, bEnabledState ? SweepProgress : 1.0f);
	}
	else if (HitButton)
	{
		const FLinearColor Stroke = !bEnabledState ? GhostCol : (bUrgent ? ERUI::Fade(AccentCol, 0.65f + 0.35f * Pulse) : ERUI::Fade(BoneCol, 0.30f + 0.50f * H01));
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

	const FLinearColor TextCol = ButtonFrame
		? (bEnabledState ? ERUI::InkText() : ERUI::InkText(0.6f))
		: (!bEnabledState ? TertiaryCol : (bUrgent ? AccentCol : BoneCol));
	if (LabelText) { LabelText->SetColorAndOpacity(FSlateColor(TextCol)); }
	if (ArrowText)
	{
		ArrowText->SetVisibility(bShowArrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ArrowText->SetColorAndOpacity(FSlateColor(ButtonFrame ? ERUI::Fade(TextCol, 0.72f) : TextCol));
		ArrowText->SetRenderTranslation(FVector2D(6.0f * HoverA, 0.0f));
	}

	const float Scale = FMath::Lerp(1.0f, 0.965f, FMath::Clamp(PressA, 0.0f, 1.0f));
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderScale(FVector2D(Scale, Scale));
}

void UERActionButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bUrgent)
	{
		UrgentPhase += InDeltaTime;
	}
	ApplyStyle(InDeltaTime);
}

void UERActionButton::HandleClicked()
{
	PressSpring.Impulse(6.0f);
	OnActionClicked.Broadcast();
	if (NativeCallback) { NativeCallback(); }
}

UWidget* UERActionButton::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
