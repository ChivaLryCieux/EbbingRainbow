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

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ButtonOverlay"));
	Ov->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HitButton->SetContent(Ov);

	// 机能风切角底板 (在明亮灰混凝土战场上绝对锐利高对比)
	ButtonFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("ButtonFrame"));
	ButtonFrame->Chamfer = FVector4(0.0f, 10.0f, 0.0f, 10.0f);
	ButtonFrame->FillColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.90f);
	ButtonFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.28f);
	ButtonFrame->bDrawCornerBrackets = true;
	ButtonFrame->BracketLength = 7.0f;
	ButtonFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.72f);
	ButtonFrame->AccentEdge = EERAccentEdge::Left;
	ButtonFrame->AccentThickness = 3.0f;
	ButtonFrame->AccentColor = T->Accent;
	ButtonFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(ButtonFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Ov->AddChildToOverlay(Row);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(22.0f, 12.0f));
	}

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Body, T->Bone);
	LabelText->SetFont(ERUI::FontBold(14, 120));
	Row->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);

	ArrowText = ERUI::MakeText(this, ArrowSymbol, EERTypeScale::Body, T->BoneSecondary, true);
	ArrowText->SetFont(ERUI::FontBold(15, 0));
	UHorizontalBoxSlot* ArSlot = Row->AddChildToHorizontalBox(ArrowText);
	ArSlot->SetVerticalAlignment(VAlign_Center);
	ArSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));

	ApplyStyle(0.0f);
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
		if (!bEnabledState)
		{
			ButtonFrame->SetFrameColors(
				FLinearColor(0.008f, 0.009f, 0.012f, 0.52f),
				ERUI::Fade(BoneCol, 0.10f),
				ERUI::Fade(BoneCol, 0.16f),
				ERUI::Fade(BoneCol, 0.12f));
		}
		else if (bUrgent)
		{
			// 紧急态(无剩余AP): 标志性高对比反白石膏底 + 极墨黑字 + 折射暖金点睛条
			ButtonFrame->SetFrameColors(
				FMath::Lerp(FLinearColor(0.93f, 0.93f, 0.91f, 0.95f), FLinearColor(1.0f, 1.0f, 0.98f, 1.0f), FMath::Max(H01, Pulse * 0.4f)),
				FLinearColor(0.01f, 0.01f, 0.02f, 0.45f),
				FLinearColor(0.01f, 0.01f, 0.02f, 0.85f),
				AccentCol);
		}
		else
		{
			ButtonFrame->SetFrameColors(
				FMath::Lerp(FLinearColor(0.008f, 0.009f, 0.012f, 0.90f), FLinearColor(0.024f, 0.026f, 0.032f, 0.96f), H01),
				ERUI::Fade(BoneCol, 0.24f + 0.58f * H01),
				ERUI::Fade(BoneCol, 0.65f + 0.35f * H01),
				FMath::Lerp(ERUI::Fade(BoneCol, 0.35f), AccentCol, H01));
		}
		ButtonFrame->SetMotionState(1.0f, H01, SweepProgress);
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

	const FLinearColor TextCol = !bEnabledState ? TertiaryCol
		: (bUrgent && ButtonFrame) ? T->PureInk
		: (bUrgent ? AccentCol : BoneCol);
	if (LabelText) { LabelText->SetColorAndOpacity(FSlateColor(TextCol)); }
	if (ArrowText)
	{
		ArrowText->SetVisibility(bShowArrow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ArrowText->SetColorAndOpacity(FSlateColor(bEnabledState ? TextCol : TertiaryCol));
		ArrowText->SetRenderTranslation(FVector2D(5.0f * HoverA, 0.0f));
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
}

UWidget* UERActionButton::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
