// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERUnitToken.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"

#include "Battle/Units/BattleUnit.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> UERUnitToken::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(ERUI::Theme()->SquadChipSize); }
	}
	return Super::RebuildWidget();
}

void UERUnitToken::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!HitButton) { Build(ERUI::Theme()->SquadChipSize); }
	}
	Super::NativeConstruct();
}

void UERUnitToken::NativeDestruct()
{
	Super::NativeDestruct();
}

void UERUnitToken::Build(float ChipSize)
{
	const UERUITheme* T = ERUI::Theme();

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle BtnStyle;
	const FSlateBrush Transparent = ERUI::TransparentBrush();
	BtnStyle.SetNormal(Transparent);
	BtnStyle.SetHovered(Transparent);
	BtnStyle.SetPressed(Transparent);
	BtnStyle.SetDisabled(Transparent);
	HitButton->SetStyle(BtnStyle);
	HitButton->OnClicked.AddDynamic(this, &UERUnitToken::HandleClicked);
	HitButton->OnHovered.AddDynamic(this, &UERUnitToken::HandleHoverBegin);
	HitButton->OnUnhovered.AddDynamic(this, &UERUnitToken::HandleHoverEnd);
	WidgetTree->RootWidget = HitButton;

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	HitButton->SetContent(Col);

	// 队伍色刻度
	TeamTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TeamTick->SetBrush(ERUI::WhiteBrush(FVector2D(ChipSize, 3.0f)));
	TeamTick->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Col->AddChildToVerticalBox(TeamTick)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));

	// 方块主体: 首字符
	ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChipBox->SetWidthOverride(ChipSize);
	ChipBox->SetHeightOverride(ChipSize);
	{
		UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		GlyphText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone);
		UOverlaySlot* S = Ov->AddChildToOverlay(GlyphText);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
		ChipBox->SetContent(Ov);
	}
	Col->AddChildToVerticalBox(ChipBox);

	// 2px 血线
	HpLine = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	{
		FProgressBarStyle PBStyle;
		PBStyle.SetBackgroundImage(ERUI::WhiteBrush());
		PBStyle.BackgroundImage.TintColor = FSlateColor(T->BoneGhost);
		PBStyle.SetFillImage(ERUI::WhiteBrush());
		HpLine->SetWidgetStyle(PBStyle);
		HpLine->SetFillColorAndOpacity(T->Bone);
		HpLine->SetPercent(1.0f);
		HpLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	{
		USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LineBox->SetHeightOverride(2.0f);
		LineBox->SetContent(HpLine);
		Col->AddChildToVerticalBox(LineBox)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
	}

	// 当前回合金线
	ActiveRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	ActiveRule->SetBrush(ERUI::WhiteBrush(FVector2D(ChipSize, 2.0f)));
	ActiveRule->SetColorAndOpacity(T->Accent);
	ActiveRule->SetVisibility(ESlateVisibility::Collapsed);
	Col->AddChildToVerticalBox(ActiveRule)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	// 名字 (默认折叠)
	NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary);
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetVisibility(ESlateVisibility::Collapsed);
	Col->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
}

void UERUnitToken::Setup(ABattleUnit* InUnit, bool bInIsSquad)
{
	Unit = InUnit;
	bIsSquad = bInIsSquad;
	bShowName = bInIsSquad;

	const float ChipSize = bIsSquad ? ERUI::Theme()->SquadChipSize : ERUI::Theme()->ChipSize;

	// 尺寸随用途切换: 重建布局太麻烦, 直接覆盖固定尺寸
	if (TeamTick)  { TeamTick->SetDesiredSizeOverride(FVector2D(ChipSize, 3.0f)); }
	if (ActiveRule){ ActiveRule->SetDesiredSizeOverride(FVector2D(ChipSize, 2.0f)); }
	if (ChipBox)   { ChipBox->SetWidthOverride(ChipSize); ChipBox->SetHeightOverride(ChipSize); }
	if (GlyphText)
	{
		const UERUITheme* T = ERUI::Theme();
		GlyphText->SetFont(T->GetFont(bIsSquad ? EERTypeScale::Title : EERTypeScale::Caption));
	}

	RefreshFromUnit();
	UpdateNameVisibility();
}

void UERUnitToken::RefreshFromUnit()
{
	if (!Unit)
	{
		return;
	}
	const UERUITheme* T = ERUI::Theme();
	const bool bAlive = Unit->IsAlive();
	const bool bIsPlayer = (Unit->Team == EBattleTeam::Player);

	// 首字符
	if (GlyphText)
	{
		const FString& Name = Unit->Stats.DisplayName;
		GlyphText->SetText(FText::FromString(Name.Left(1)));
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(Unit->Stats.DisplayName));
	}

	// 队伍刻度
	if (TeamTick)
	{
		TeamTick->SetColorAndOpacity(bIsPlayer ? T->AllyMark : T->EnemyMark);
	}

	// 血线
	if (HpLine)
	{
		const float MaxHp = Unit->GetSkillComponent() ? Unit->GetSkillComponent()->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
		const float Pct = (MaxHp > 0.0f) ? FMath::Clamp(Unit->Integrity / MaxHp, 0.0f, 1.0f) : 0.0f;
		HpLine->SetPercent(Pct);
		HpLine->SetFillColorAndOpacity(Pct < 0.30f ? T->DangerColor : T->Bone);
	}

	ApplyDeadVisual();
	UpdateNameVisibility();
}

void UERUnitToken::ApplyDeadVisual()
{
	if (!Unit)
	{
		return;
	}
	const bool bAlive = Unit->IsAlive();
	SetRenderOpacity(bAlive ? 1.0f : 0.32f);
}

void UERUnitToken::SetActiveFlag(bool bInActive)
{
	bActive = bInActive;
	if (ActiveRule)
	{
		ActiveRule->SetVisibility(bInActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (GlyphText)
	{
		GlyphText->SetColorAndOpacity(FSlateColor(bInActive ? ERUI::Theme()->Accent : ERUI::Theme()->Bone));
	}
	UpdateNameVisibility();
}

void UERUnitToken::SetFocusedFlag(bool bInFocused)
{
	bFocused = bInFocused;
	if (NameText)
	{
		NameText->SetColorAndOpacity(FSlateColor(bInFocused ? ERUI::Theme()->Accent : ERUI::Theme()->BoneSecondary));
	}
	UpdateNameVisibility();
}

void UERUnitToken::UpdateNameVisibility()
{
	if (!NameText)
	{
		return;
	}
	const bool bShow = bShowName && (bActive || bFocused || bHovered);
	NameText->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UERUnitToken::HandleClicked()
{
	OnTokenClicked.Broadcast(Unit);
}

void UERUnitToken::HandleHoverBegin()
{
	bHovered = true;
	UpdateNameVisibility();
}

void UERUnitToken::HandleHoverEnd()
{
	bHovered = false;
	UpdateNameVisibility();
}

UWidget* UERUnitToken::GetHoverCheckWidget() const
{
	return HitButton.Get();
}
