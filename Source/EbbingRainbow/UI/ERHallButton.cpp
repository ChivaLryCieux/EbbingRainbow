// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHallButton.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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

	HoverSpring.SnapTo(0.0f);
	PressSpring.SnapTo(0.0f);
	SweepProgress = 1.0f;
	bWasHovered = false;

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
	ButtonSizeBox->SetHeightOverride(ButtonHeight);
	HitButton->SetContent(ButtonSizeBox);

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonSizeBox->SetContent(RootOverlay);

	// 1. 机能风 45° 切角底板 (右上/左下切角 + 四角对准刻度 + 左侧点睛条)
	TechFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("TechFrame"));
	TechFrame->Chamfer = FVector4(0.0f, 14.0f, 0.0f, 14.0f);
	TechFrame->BorderThickness = 1.0f;
	TechFrame->bDrawCornerBrackets = true;
	TechFrame->BracketLength = 10.0f;
	TechFrame->BracketThickness = 1.5f;
	TechFrame->AccentEdge = EERAccentEdge::Left;
	TechFrame->AccentThickness = 3.5f;
	TechFrame->bUsePrismAccent = bPrimary;
	TechFrame->DesiredFrameSize = FVector2D(ButtonWidth, ButtonHeight);
	TechFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = RootOverlay->AddChildToOverlay(TechFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	// 2. 内容排版: [大号 Linux Biolinum 索引号] | [粗体主标题 / 极小英文遥测副标] ... [→]
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContentRow"));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = RootOverlay->AddChildToOverlay(Row);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(22.0f, 10.0f, 20.0f, 10.0f));
	}

	IndexText = ERUI::MakeText(this, IndexCode, EERTypeScale::Title, T->BoneSecondary, true);
	IndexText->SetFont(ERUI::FontBold(24, 40));
	{
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IndexText);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	}

	UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextCol"));
	TextCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(TextCol);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Title, T->Bone);
	LabelText->SetFont(ERUI::FontBold(20, 60));
	TextCol->AddChildToVerticalBox(LabelText);

	CaptionText = ERUI::MakeText(this, Caption, EERTypeScale::Micro, T->BoneTertiary, true);
	CaptionText->SetFont(ERUI::FontRegular(8, 240));
	TextCol->AddChildToVerticalBox(CaptionText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	ArrowText = ERUI::MakeText(this, INVTEXT("→"), EERTypeScale::Title, T->BoneSecondary, true);
	ArrowText->SetFont(ERUI::FontBold(18, 0));
	{
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ArrowText);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
	}
}

void UERHallButton::SyncStaticProperties()
{
	if (IndexText)   { IndexText->SetText(IndexCode); }
	if (LabelText)   { LabelText->SetText(Label); }
	if (CaptionText && !CaptionCipher.IsRunning()) { CaptionText->SetText(Caption); }
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
		ButtonSizeBox->SetHeightOverride(ButtonHeight);
	}
	if (TechFrame)
	{
		TechFrame->bUsePrismAccent = bPrimary;
		TechFrame->AccentEdge = EERAccentEdge::Left;
		TechFrame->AccentThickness = bPrimary ? 4.0f : 3.0f;
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
	SyncStaticProperties();
	RefreshVisuals(0.0f, IsDesignTime());
}

void UERHallButton::TriggerEntrancePulse(float DelaySeconds)
{
	CaptionCipher.Start(Caption.ToString(), 0.36f, DelaySeconds);
	SweepProgress = 0.01f;
}

void UERHallButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ElapsedTime += InDeltaTime;

	FString Decoded;
	if (CaptionCipher.Tick(InDeltaTime, Decoded) && CaptionText)
	{
		CaptionText->SetText(FText::FromString(Decoded));
	}

	if (SweepProgress < 1.0f)
	{
		SweepProgress = FMath::Min(1.0f, SweepProgress + InDeltaTime / 0.42f);
	}

	RefreshVisuals(InDeltaTime, false);
}

void UERHallButton::RefreshVisuals(float DeltaTime, bool bDesignPreview)
{
	if (!bDriveGlassAnimationFromCode)
	{
		return;
	}

	const UERUITheme* T = ERUI::Theme();
	const bool bHovered = !bDesignPreview && HitButton && HitButton->IsHovered();
	const bool bPressed = !bDesignPreview && HitButton && HitButton->IsPressed();

	if (bHovered && !bWasHovered)
	{
		SweepProgress = 0.01f;
	}
	bWasHovered = bHovered;

	HoverSpring.SetTarget(bHovered ? 1.0f : 0.0f);
	PressSpring.SetTarget(bPressed ? 1.0f : 0.0f);

	const float HoverA = (DeltaTime > 0.0f) ? HoverSpring.Tick(DeltaTime) : 0.0f;
	const float PressA = (DeltaTime > 0.0f) ? PressSpring.Tick(DeltaTime) : 0.0f;
	const float Hover01 = FMath::Clamp(HoverA, 0.0f, 1.15f);
	const float Pulse = bPrimary ? ERMotion::SolemnPulse(ElapsedTime, BreathSpeed) : 0.0f;

	// 黑-白-点睛彩对比: 主按钮反白极墨字 + 虹彩边条; 次按钮哑光黑曜石 + 纯白字
	if (TechFrame)
	{
		if (bPrimary)
		{
			const FLinearColor Fill = FMath::Lerp(
				FLinearColor(0.93f, 0.93f, 0.91f, 0.95f),
				FLinearColor(1.00f, 1.00f, 0.98f, 1.00f),
				FMath::Clamp(Hover01, 0.0f, 1.0f));
			const FLinearColor Border = FLinearColor(0.02f, 0.02f, 0.03f, 0.35f + 0.45f * FMath::Clamp(Hover01, 0.0f, 1.0f));
			const FLinearColor Bracket = FLinearColor(0.01f, 0.01f, 0.02f, 0.72f + 0.25f * Pulse);
			TechFrame->SetFrameColors(Fill, Border, Bracket, AccentColor);
			TechFrame->bUsePrismAccent = true;
		}
		else
		{
			const FLinearColor Fill = FMath::Lerp(
				FLinearColor(0.008f, 0.009f, 0.012f, 0.90f),
				FLinearColor(0.024f, 0.026f, 0.032f, 0.96f),
				FMath::Clamp(Hover01, 0.0f, 1.0f));
			const FLinearColor Border = FLinearColor(0.92f, 0.92f, 0.90f, 0.22f + 0.56f * FMath::Clamp(Hover01, 0.0f, 1.0f));
			const FLinearColor Bracket = FLinearColor(0.92f, 0.92f, 0.90f, 0.68f + 0.32f * FMath::Clamp(Hover01, 0.0f, 1.0f));
			const FLinearColor EdgeCol = FMath::Lerp(FLinearColor(0.92f, 0.92f, 0.90f, 0.28f), AccentColor, FMath::Clamp(Hover01, 0.0f, 1.0f));
			TechFrame->SetFrameColors(Fill, Border, Bracket, EdgeCol);
			TechFrame->bUsePrismAccent = false;
		}
		TechFrame->SetMotionState(1.0f, FMath::Clamp(Hover01, 0.0f, 1.0f), SweepProgress);
	}

	// 文字色彩反转
	const FLinearColor MainTextCol = bPrimary ? T->PureInk : T->Bone;
	const FLinearColor SubTextCol  = bPrimary ? ERUI::Fade(T->PureInk, 0.62f) : ERUI::Fade(T->Bone, 0.48f + 0.28f * FMath::Clamp(Hover01, 0.0f, 1.0f));
	const FLinearColor IdxCol      = bPrimary ? ERUI::Fade(T->PureInk, 0.42f + 0.30f * FMath::Clamp(Hover01, 0.0f, 1.0f)) : FMath::Lerp(ERUI::Fade(T->Bone, 0.32f), AccentColor, FMath::Clamp(Hover01, 0.0f, 1.0f));

	if (IndexText)   { IndexText->SetColorAndOpacity(FSlateColor(IdxCol)); }
	if (LabelText)   { LabelText->SetColorAndOpacity(FSlateColor(MainTextCol)); }
	if (CaptionText) { CaptionText->SetColorAndOpacity(FSlateColor(SubTextCol)); }
	if (ArrowText)
	{
		ArrowText->SetColorAndOpacity(FSlateColor(bPrimary ? T->PureInk : FMath::Lerp(T->BoneSecondary, T->Bone, FMath::Clamp(Hover01, 0.0f, 1.0f))));
		ArrowText->SetRenderTranslation(FVector2D(6.0f * HoverA, 0.0f));
	}

	// 弹簧物理位姿 (极克制 3px 轻抬 + 按压缩放)
	const float Scale = FMath::Lerp(1.0f, PressedScale, FMath::Clamp(PressA, 0.0f, 1.0f));
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderScale(FVector2D(Scale, Scale));
	SetRenderTranslation(FVector2D(0.0f, -3.0f * HoverA + 1.5f * PressA));

	// 兼容旧 WBP 如果仍保留 BodyBorder (不破坏老蓝图)
	if (!TechFrame && BodyBorder)
	{
		const FLinearColor Fill = bPrimary ? FLinearColor(0.94f, 0.94f, 0.92f, 0.96f) : T->InkPanel;
		BodyBorder->SetBrush(ERUI::RoundedBrush(0.0f, Fill, 1.0f, T->Bone));
		BodyBorder->SetBrushColor(FLinearColor::White);
	}
}

void UERHallButton::HandleClicked()
{
	PressSpring.Impulse(6.0f);
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
