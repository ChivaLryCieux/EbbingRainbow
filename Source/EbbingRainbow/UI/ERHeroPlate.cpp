// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHeroPlate.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/ERPipBar.h"

#include "Battle/Units/BattleUnit.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> UERHeroPlate::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!NameText) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERHeroPlate::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		if (!NameText) { Build(); }
	}
}

void UERHeroPlate::Build()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Col;
	Col->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 名字 + 队伍刻度同行
	{
		UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		TeamTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		TeamTick->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 10.0f)));
		TeamTick->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* TkSlot = NameRow->AddChildToHorizontalBox(TeamTick);
		TkSlot->SetVerticalAlignment(VAlign_Center);
		TkSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::Theme()->BoneSecondary);
		NameRow->AddChildToHorizontalBox(NameText)->SetVerticalAlignment(VAlign_Center);
		Col->AddChildToVerticalBox(NameRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	// HP 大数字行
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		HpNumber = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Display, ERUI::Theme()->Bone, true);
		Row->AddChildToHorizontalBox(HpNumber)->SetVerticalAlignment(VAlign_Bottom);

		HpMaxText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::Theme()->BoneTertiary, true);
		UHorizontalBoxSlot* MxSlot = Row->AddChildToHorizontalBox(HpMaxText);
		MxSlot->SetVerticalAlignment(VAlign_Bottom);
		MxSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 8.0f));

		BarrierText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::Theme()->AllyMark, true);
		UHorizontalBoxSlot* BaSlot = Row->AddChildToHorizontalBox(BarrierText);
		BaSlot->SetVerticalAlignment(VAlign_Bottom);
		BaSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 8.0f));

		Col->AddChildToVerticalBox(Row);
	}

	// 血线
	{
		HpLine = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle PBStyle;
		PBStyle.SetBackgroundImage(ERUI::WhiteBrush());
		PBStyle.BackgroundImage.TintColor = FSlateColor(ERUI::Theme()->BoneGhost);
		PBStyle.SetFillImage(ERUI::WhiteBrush());
		HpLine->SetWidgetStyle(PBStyle);
		HpLine->SetFillColorAndOpacity(ERUI::Theme()->Bone);
		HpLine->SetPercent(1.0f);
		HpLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(190.0f);
		Box->SetHeightOverride(2.0f);
		Box->SetContent(HpLine);
		Col->AddChildToVerticalBox(Box)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 10.0f));
	}

	// AP 点阵 + 数字
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ApPips = WidgetTree->ConstructWidget<UERPipBar>(UERPipBar::StaticClass());
		Row->AddChildToHorizontalBox(ApPips)->SetVerticalAlignment(VAlign_Center);

		ApText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::Theme()->BoneTertiary, true);
		UHorizontalBoxSlot* ApSlot = Row->AddChildToHorizontalBox(ApText);
		ApSlot->SetVerticalAlignment(VAlign_Center);
		ApSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

		Col->AddChildToVerticalBox(Row);
	}

	// 状态摘要
	StatusText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::Theme()->BoneTertiary);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	Col->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
}

void UERHeroPlate::BindUnit(ABattleUnit* InUnit)
{
	Unit = InUnit;
	Refresh();
}

void UERHeroPlate::Refresh()
{
	const UERUITheme* T = ERUI::Theme();
	if (!Unit || !Unit->IsAlive())
	{
		ShowWaiting(true);
		return;
	}
	ShowWaiting(false);

	const UBattleSkillComponent* SC = Unit->GetSkillComponent();
	const float MaxHp = SC ? SC->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
	const int32 MaxAp = SC ? SC->GetEffectiveMaxAP() : Unit->Stats.MaxAP;
	const float Pct = (MaxHp > 0.0f) ? FMath::Clamp(Unit->Integrity / MaxHp, 0.0f, 1.0f) : 0.0f;
	const bool bIsPlayer = (Unit->Team == EBattleTeam::Player);

	if (NameText)
	{
		NameText->SetText(FText::FromString(Unit->Stats.DisplayName));
		NameText->SetColorAndOpacity(FSlateColor(bIsPlayer ? T->BoneSecondary : T->EnemyMark));
	}
	if (TeamTick)
	{
		TeamTick->SetColorAndOpacity(bIsPlayer ? T->AllyMark : T->EnemyMark);
	}
	if (HpNumber)
	{
		HpNumber->SetText(FText::AsNumber(FMath::RoundToInt(Unit->Integrity)));
		HpNumber->SetColorAndOpacity(FSlateColor(Pct < 0.30f ? T->DangerColor : T->Bone));
	}
	if (HpMaxText)
	{
		HpMaxText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), FMath::RoundToInt(MaxHp))));
	}
	if (BarrierText)
	{
		const bool bHas = Unit->Barrier > 0.0f;
		BarrierText->SetVisibility(bHas ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHas)
		{
			BarrierText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), FMath::RoundToInt(Unit->Barrier))));
		}
	}
	if (HpLine)
	{
		HpLine->SetPercent(Pct);
		HpLine->SetFillColorAndOpacity(Pct < 0.30f ? T->DangerColor : T->Bone);
	}
	if (ApPips)
	{
		ApPips->SetPipColor(T->Bone);
		ApPips->SetPips(FMath::Clamp(Unit->AP, 0, MaxAp), FMath::Max(1, MaxAp));
	}
	if (ApText)
	{
		ApText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Unit->AP, MaxAp)));
	}
	if (StatusText)
	{
		const FString Summary = SC ? SC->GetStatusSummary() : FString();
		StatusText->SetVisibility(Summary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		StatusText->SetText(FText::FromString(Summary));
	}
}

void UERHeroPlate::ShowWaiting(bool bWaiting)
{
	if (bWaiting)
	{
		if (NameText)   { NameText->SetText(FText::GetEmpty()); }
		if (HpNumber)   { HpNumber->SetText(FText::FromString(TEXT("—"))); }
		if (HpMaxText)  { HpMaxText->SetText(FText::GetEmpty()); }
		if (BarrierText){ BarrierText->SetVisibility(ESlateVisibility::Collapsed); }
		if (HpLine)     { HpLine->SetPercent(0.0f); }
		if (ApPips)     { ApPips->SetPips(0, 0); }
		if (ApText)     { ApText->SetText(FText::GetEmpty()); }
		if (StatusText) { StatusText->SetVisibility(ESlateVisibility::Collapsed); }
	}
}
