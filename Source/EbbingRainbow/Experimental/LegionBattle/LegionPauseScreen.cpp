// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionPauseScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "UI/ERUIStatics.h"

namespace
{
	constexpr int32 PausePlanetFontSize = 190;
	constexpr float PauseLogoHeight = 100.0f;
	constexpr int32 PauseButtonFontSize = 22;
	const TCHAR* PauseIrisWhiteLogoPath = TEXT("/Game/UI/Textures/T_UI_Faction_Iris_White.T_UI_Faction_Iris_White");
}

TSharedRef<SWidget> ULegionPauseScreen::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	return Super::RebuildWidget();
}

void ULegionPauseScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
}

void ULegionPauseScreen::Build()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// ---- 全屏深墨半透明底幕 (Visible: 拦截视口落点, 保留冻结战场的微弱轮廓) ----
	{
		UImage* Scrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Scrim"));
		Scrim->SetBrush(ERUI::WhiteBrush());
		Scrim->SetColorAndOpacity(FLinearColor(0.004f, 0.005f, 0.008f, 0.68f));
		Scrim->SetVisibility(ESlateVisibility::Visible);
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Scrim);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// ---- 中央纵列: 星球名 / 虹膜徽标 / 三枚居中纯白错位阴影按钮 ----
	UVerticalBox* CenterCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterCol"));
	CenterCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(CenterCol);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D::ZeroVector);
		S->SetAutoSize(true);
	}

	// 1. 星球名 (190pt 粗体纯白 + 黑色错位立体阴影)
	PlanetText = ERUI::MakeText(this, INVTEXT("端点星"), EERTypeScale::Hero, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), false);
	ERUI::ApplyResonantText(PlanetText, PausePlanetFontSize, true, 0,
		FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(6.0f, 8.0f), 0.62f);
	PlanetText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(PlanetText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// 2. 虹膜白色徽标
	if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, PauseIrisWhiteLogoPath))
	{
		LogoImage = ERUI::MakeLogoImage(this, Tex, PauseLogoHeight);
	}
	if (LogoImage)
	{
		LogoImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(LogoImage))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 48.0f, 0.0f, 0.0f));
		}
	}

	// 3. 居中竖排三项纯白错位阴影操作: 继续连接 / 中止连接 / 设置
	UVerticalBox* MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	MenuBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* MS = CenterCol->AddChildToVerticalBox(MenuBox))
	{
		MS->SetHorizontalAlignment(HAlign_Center);
		MS->SetPadding(FMargin(0.0f, 64.0f, 0.0f, 0.0f));
	}

	const FSlateBrush Tr = ERUI::TransparentBrush();
	FButtonStyle BS;
	BS.SetNormal(Tr);
	BS.SetHovered(Tr);
	BS.SetPressed(Tr);
	BS.SetDisabled(Tr);
	BS.SetNormalPadding(FMargin(28.0f, 8.0f));
	BS.SetPressedPadding(FMargin(28.0f, 8.0f));

	auto MakeMenuItem = [&](const FText& LabelText, bool bIsDanger, bool bTopGap, FPauseMenuItem& OutItem)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetStyle(BS);
		Btn->SetCursor(EMouseCursor::Hand);

		UVerticalBox* ItemCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ItemCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Btn->SetContent(ItemCol);

		UTextBlock* TB = ERUI::MakeText(this, LabelText, EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 0.80f), false);
		ERUI::ApplyResonantText(TB, PauseButtonFontSize, true, 180,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.80f), FVector2D(2.0f, 3.0f), 0.88f);
		TB->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* TS = ItemCol->AddChildToVerticalBox(TB))
		{
			TS->SetHorizontalAlignment(HAlign_Center);
		}

		UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Line->SetBrush(ERUI::WhiteBrush(FVector2D(112.0f, 1.5f)));
		Line->SetColorAndOpacity(bIsDanger ? FLinearColor(1.0f, 0.36f, 0.24f, 0.92f) : FLinearColor(0.99f, 0.99f, 0.98f, 0.90f));
		Line->SetRenderOpacity(0.0f);
		Line->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UVerticalBoxSlot* LS = ItemCol->AddChildToVerticalBox(Line))
		{
			LS->SetHorizontalAlignment(HAlign_Center);
			LS->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}

		if (UVerticalBoxSlot* BSItem = MenuBox->AddChildToVerticalBox(Btn))
		{
			BSItem->SetHorizontalAlignment(HAlign_Center);
			BSItem->SetPadding(FMargin(0.0f, bTopGap ? 14.0f : 0.0f, 0.0f, 0.0f));
		}

		OutItem.Button = Btn;
		OutItem.Label = TB;
		OutItem.Underline = Line;
		OutItem.HoverAlpha = 0.0f;
		OutItem.PressAlpha = 0.0f;
		OutItem.bDanger = bIsDanger;
	};

	MakeMenuItem(INVTEXT("继续连接"), false, false, ResumeItem);
	MakeMenuItem(INVTEXT("中止连接"), true,  true,  AbortItem);
	MakeMenuItem(INVTEXT("设置"),     false, true,  SettingsItem);

	ResumeItem.Button->OnClicked.AddDynamic(this, &ULegionPauseScreen::HandleResumeClicked);
	AbortItem.Button->OnClicked.AddDynamic(this, &ULegionPauseScreen::HandleAbortClicked);
	SettingsItem.Button->OnClicked.AddDynamic(this, &ULegionPauseScreen::HandleSettingsClicked);
}

void ULegionPauseScreen::ShowPauseMenu()
{
	bAborting = false;
	for (FPauseMenuItem* Item : { &ResumeItem, &AbortItem, &SettingsItem })
	{
		if (Item->Button) { Item->Button->SetIsEnabled(true); }
		Item->HoverAlpha = 0.0f;
		Item->PressAlpha = 0.0f;
		TickItemVisual(*Item, 0.0f);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void ULegionPauseScreen::HidePauseMenu()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool ULegionPauseScreen::IsPauseMenuVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void ULegionPauseScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsPauseMenuVisible()) { return; }

	TickItemVisual(ResumeItem, InDeltaTime);
	TickItemVisual(AbortItem, InDeltaTime);
	TickItemVisual(SettingsItem, InDeltaTime);
}

void ULegionPauseScreen::TickItemVisual(FPauseMenuItem& Item, float DeltaTime)
{
	if (!Item.Button || !Item.Label || !Item.Underline) { return; }

	const bool bHovered = !bAborting && Item.Button->IsHovered();
	const bool bPressed = !bAborting && Item.Button->IsPressed();

	if (DeltaTime > 0.0f)
	{
		Item.HoverAlpha = FMath::FInterpTo(Item.HoverAlpha, bHovered ? 1.0f : 0.0f, DeltaTime, 14.0f);
		Item.PressAlpha = FMath::FInterpTo(Item.PressAlpha, bPressed ? 1.0f : 0.0f, DeltaTime, 22.0f);
	}
	else
	{
		Item.HoverAlpha = bHovered ? 1.0f : 0.0f;
		Item.PressAlpha = bPressed ? 1.0f : 0.0f;
	}

	const FLinearColor IdleCol(0.99f, 0.99f, 0.98f, 0.80f);
	const FLinearColor HoverCol = Item.bDanger
		? FLinearColor(1.0f, 0.36f, 0.24f, 1.0f)
		: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor CurCol = FMath::Lerp(IdleCol, HoverCol, FMath::Clamp(Item.HoverAlpha, 0.0f, 1.0f));
	const FVector2D ShadowOff = FMath::Lerp(FVector2D(2.0f, 3.0f), FVector2D(1.0f, 1.5f), FMath::Clamp(Item.PressAlpha, 0.0f, 1.0f));

	ERUI::ApplyResonantText(Item.Label, PauseButtonFontSize, true, 180, CurCol, ShadowOff, 0.88f);

	Item.Underline->SetRenderOpacity(Item.HoverAlpha);
	Item.Underline->SetRenderScale(FVector2D(FMath::Lerp(0.35f, 1.0f, Item.HoverAlpha), 1.0f));

	const float Scale = FMath::Lerp(1.0f, 1.04f, Item.HoverAlpha) * FMath::Lerp(1.0f, 0.97f, Item.PressAlpha);
	Item.Button->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Item.Button->SetRenderScale(FVector2D(Scale, Scale));
}

void ULegionPauseScreen::HandleResumeClicked()
{
	if (bAborting) { return; }
	OnResumeClicked.ExecuteIfBound();
}

void ULegionPauseScreen::HandleAbortClicked()
{
	if (bAborting) { return; }
	bAborting = true;
	if (AbortItem.Button) { AbortItem.Button->SetIsEnabled(false); }
	OnAbortClicked.ExecuteIfBound();
}

void ULegionPauseScreen::HandleSettingsClicked()
{
	if (bAborting) { return; }
	OnSettingsClicked.ExecuteIfBound();
}
