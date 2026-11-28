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
#include "Components/Image.h"
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
	ButtonSizeBox->SetMinDesiredWidth(180.0f);
	if (bApplyWidthOverride)
	{
		ButtonSizeBox->SetWidthOverride(ButtonWidth);
	}
	ButtonSizeBox->SetHeightOverride(ButtonHeight);
	HitButton->SetContent(ButtonSizeBox);

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonSizeBox->SetContent(RootOverlay);

	// 纯白字 + 错位黑影《控制:共振》风格 (保留 TechFrame 作为折叠兼容位, 绝不绘制白底卡片)
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContentRow"));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = RootOverlay->AddChildToOverlay(Row);
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(14.0f, 4.0f, 16.0f, 4.0f));
	}

	LabelText = ERUI::MakeText(this, Label, EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 0.82f));
	ERUI::ApplyResonantText(LabelText, 21, true, 140, FLinearColor(0.99f, 0.99f, 0.98f, 0.82f), FVector2D(2.0f, 3.0f), 0.86f);
	{
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelText);
		S->SetVerticalAlignment(VAlign_Center);
	}
}

void UERHallButton::SyncStaticProperties()
{
	// 全面隐藏旧卡片框体、序号、英文副标与箭头, 仅留纯白汉字 + 错位阴影
	if (TechFrame)  { TechFrame->SetVisibility(ESlateVisibility::Collapsed); }
	if (BodyBorder) { BodyBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (GlowBorder) { GlowBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (LedBorder)  { LedBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (IndexText)  { IndexText->SetVisibility(ESlateVisibility::Collapsed); }
	if (CaptionText){ CaptionText->SetVisibility(ESlateVisibility::Collapsed); }
	if (ArrowText)  { ArrowText->SetVisibility(ESlateVisibility::Collapsed); }

	if (LabelText)  { LabelText->SetText(Label); }

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
		ButtonSizeBox->SetMinDesiredWidth(180.0f);
		ButtonSizeBox->SetHeightOverride(ButtonHeight);

		if (UOverlay* RootOverlay = Cast<UOverlay>(ButtonSizeBox->GetContent()))
		{
			if (!HoverBarContainer)
			{
				HoverBarContainer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HoverBarContainer"));
				HoverBarContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				HoverBarContainer->SetRenderOpacity(0.0f);

				HoverBarShadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HoverBarShadow"));
				HoverBarShadow->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 18.0f)));
				HoverBarShadow->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f));
				HoverBarShadow->SetRenderTranslation(FVector2D(1.5f, 2.0f));
				HoverBarContainer->AddChildToOverlay(HoverBarShadow);

				HoverBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HoverBar"));
				HoverBar->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 18.0f)));
				HoverBar->SetColorAndOpacity(FLinearColor(0.99f, 0.99f, 0.98f, 1.0f));
				HoverBarContainer->AddChildToOverlay(HoverBar);

				UOverlaySlot* BarSlot = RootOverlay->AddChildToOverlay(HoverBarContainer);
				BarSlot->SetHorizontalAlignment(HAlign_Left);
				BarSlot->SetVerticalAlignment(VAlign_Center);
				BarSlot->SetPadding(FMargin(0.0f));
			}

			// 兼容 WBP_HallButton 既有层级: 找到挂在 RootOverlay 下的内容行并统一左对齐紧凑边距
			for (int32 i = 0; i < RootOverlay->GetChildrenCount(); ++i)
			{
				UWidget* Child = RootOverlay->GetChildAt(i);
				if (Child && Child != TechFrame && Child != HoverBarContainer)
				{
					if (UOverlaySlot* RowSlot = Cast<UOverlaySlot>(Child->Slot))
					{
						RowSlot->SetHorizontalAlignment(HAlign_Left);
						RowSlot->SetVerticalAlignment(VAlign_Center);
						RowSlot->SetPadding(FMargin(14.0f, 4.0f, 16.0f, 4.0f));
					}
				}
			}
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
	SyncStaticProperties();
	RefreshVisuals(0.0f, IsDesignTime());
}

void UERHallButton::TriggerEntrancePulse(float DelaySeconds)
{
	if (!Caption.IsEmpty()) { CaptionCipher.Start(Caption.ToString(), 0.36f, DelaySeconds); }
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

	const bool bEnabled = GetIsEnabled() && (!HitButton || HitButton->GetIsEnabled());
	const bool bHovered = bEnabled && !bDesignPreview && HitButton && HitButton->IsHovered();
	const bool bPressed = bEnabled && !bDesignPreview && HitButton && HitButton->IsPressed();

	if (bHovered && !bWasHovered)
	{
		SweepProgress = 0.01f;
	}
	bWasHovered = bHovered;

	HoverSpring.SetTarget(bHovered ? 1.0f : 0.0f);
	PressSpring.SetTarget(bPressed ? 1.0f : 0.0f);

	const float HoverA = (DeltaTime > 0.0f) ? HoverSpring.Tick(DeltaTime) : (bHovered ? 1.0f : 0.0f);
	const float PressA = (DeltaTime > 0.0f) ? PressSpring.Tick(DeltaTime) : 0.0f;
	const float Hover01 = FMath::Clamp(HoverA, 0.0f, 1.0f);

	// 确保持久化 WBP 的旧卡片/刻度不会复活
	if (TechFrame)  { TechFrame->SetVisibility(ESlateVisibility::Collapsed); }
	if (BodyBorder) { BodyBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (IndexText)  { IndexText->SetVisibility(ESlateVisibility::Collapsed); }
	if (CaptionText){ CaptionText->SetVisibility(ESlateVisibility::Collapsed); }
	if (ArrowText)  { ArrowText->SetVisibility(ESlateVisibility::Collapsed); }

	const FLinearColor TextCol = bEnabled
		? FLinearColor(0.99f, 0.99f, 0.98f, FMath::Lerp(0.84f, 1.00f, Hover01))
		: FLinearColor(0.96f, 0.96f, 0.95f, 0.42f);
	const FVector2D ShadowOff = FMath::Lerp(FVector2D(2.0f, 3.0f), FVector2D(1.0f, 1.5f), FMath::Clamp(PressA, 0.0f, 1.0f));
	const float ShadowAlpha = bEnabled ? 0.86f : 0.45f;

	if (LabelText)
	{
		ERUI::ApplyResonantText(LabelText, 21, true, 140, TextCol, ShadowOff, ShadowAlpha);
		LabelText->SetRenderTranslation(FVector2D(8.0f * HoverA, 0.0f));
	}
	if (HoverBarContainer)
	{
		HoverBarContainer->SetRenderOpacity(bEnabled ? Hover01 : 0.0f);
	}
	if (HoverBar)
	{
		HoverBar->SetColorAndOpacity(FLinearColor(0.99f, 0.99f, 0.98f, 1.0f));
	}

	const float Scale = FMath::Lerp(1.0f, PressedScale, FMath::Clamp(PressA, 0.0f, 1.0f));
	SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	SetRenderScale(FVector2D(Scale, Scale));
	SetRenderTranslation(FVector2D::ZeroVector);
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
