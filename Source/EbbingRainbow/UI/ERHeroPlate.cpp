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
#include "Materials/MaterialInstanceDynamic.h"

TSharedRef<SWidget> UERHeroPlate::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!NameText && !HpNumber) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERHeroPlate::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!NameText && !HpNumber) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		ApplyValues(PreviewName.ToString(), bPreviewIsPlayer, (float)PreviewHp, (float)FMath::Max(1, PreviewMaxHp),
			(float)PreviewBarrier, PreviewAp, FMath::Max(1, PreviewMaxAp), PreviewStatus.ToString());
	}
}

void UERHeroPlate::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		if (!NameText && !HpNumber) { Build(); }
	}
}

void UERHeroPlate::Build()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	WidgetTree->RootWidget = Col;
	Col->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 名字 + 队伍刻度同行
	{
		UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NameRow"));
		TeamTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TeamTick"));
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
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
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
		HpLine = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HpLine"));
		FProgressBarStyle PBStyle;
		PBStyle.SetBackgroundImage(ERUI::WhiteBrush());
		PBStyle.BackgroundImage.TintColor = FSlateColor(ERUI::Theme()->BoneGhost);
		PBStyle.SetFillImage(ERUI::WhiteBrush());
		HpLine->SetWidgetStyle(PBStyle);
		HpLine->SetFillColorAndOpacity(ERUI::Theme()->Bone);
		HpLine->SetPercent(1.0f);
		HpLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LineBox"));
		Box->SetWidthOverride(190.0f);
		Box->SetHeightOverride(2.0f);
		Box->SetContent(HpLine);
		Col->AddChildToVerticalBox(Box)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 10.0f));
	}

	// AP 点阵 + 数字
	{
		UHorizontalBox* ApRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ApRow"));
		ApPips = WidgetTree->ConstructWidget<UERPipBar>(UERPipBar::StaticClass(), TEXT("ApPips"));
		ApRow->AddChildToHorizontalBox(ApPips)->SetVerticalAlignment(VAlign_Center);

		ApText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::Theme()->BoneTertiary, true);
		UHorizontalBoxSlot* ApSlot = ApRow->AddChildToHorizontalBox(ApText);
		ApSlot->SetVerticalAlignment(VAlign_Center);
		ApSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

		Col->AddChildToVerticalBox(ApRow);
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
	if (!IsDesignTime())
	{
		BP_OnUnitBound(InUnit);
	}
}

void UERHeroPlate::ApplyValues(const FString& DisplayName, bool bIsPlayer, float CurHp, float MaxHp,
	float Barrier, int32 CurAp, int32 MaxAp, const FString& StatusSummary)
{
	const UERUITheme* T = ERUI::Theme();
	const float Pct = (MaxHp > 0.0f) ? FMath::Clamp(CurHp / MaxHp, 0.0f, 1.0f) : 0.0f;

	if (NameText)
	{
		NameText->SetText(FText::FromString(DisplayName));
		NameText->SetColorAndOpacity(FSlateColor(bIsPlayer ? T->BoneSecondary : T->EnemyMark));
	}
	if (TeamTick)
	{
		TeamTick->SetColorAndOpacity(bIsPlayer ? T->AllyMark : T->EnemyMark);
	}
	if (HpNumber)
	{
		HpNumber->SetText(FText::AsNumber(FMath::RoundToInt(CurHp)));
		HpNumber->SetColorAndOpacity(FSlateColor(Pct < LowHpThreshold ? T->DangerColor : T->Bone));
	}
	if (HpMaxText)
	{
		HpMaxText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), FMath::RoundToInt(MaxHp))));
	}
	if (BarrierText)
	{
		const bool bHas = Barrier > 0.0f;
		BarrierText->SetVisibility(bHas ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHas)
		{
			BarrierText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), FMath::RoundToInt(Barrier))));
		}
	}
	if (HpLine)
	{
		HpLine->SetPercent(Pct);
		HpLine->SetFillColorAndOpacity(Pct < LowHpThreshold ? T->DangerColor : T->Bone);
	}
	if (PortraitImage)
	{
		if (UMaterialInstanceDynamic* MID = PortraitImage->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(TEXT("GrayFraction"), 1.0f - Pct);
		}
	}
	if (ApPips)
	{
		ApPips->SetPipColor(T->Bone);
		ApPips->SetPips(FMath::Clamp(CurAp, 0, MaxAp), FMath::Max(1, MaxAp));
	}
	if (ApText)
	{
		ApText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurAp, MaxAp)));
	}
	if (StatusText)
	{
		StatusText->SetVisibility(StatusSummary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		StatusText->SetText(FText::FromString(StatusSummary));
	}
}

void UERHeroPlate::Refresh()
{
	if (!Unit || !Unit->IsAlive())
	{
		ShowWaiting(true);
		return;
	}
	ShowWaiting(false);

	const UBattleSkillComponent* SC = Unit->GetSkillComponent();
	const float MaxHp = SC ? SC->GetEffectiveMaxIntegrity() : Unit->Stats.MaxIntegrity;
	const int32 MaxAp = SC ? SC->GetEffectiveMaxAP() : Unit->Stats.MaxAP;
	const bool bIsPlayer = (Unit->Team == EBattleTeam::Player);
	const FString Summary = SC ? SC->GetStatusSummary() : FString();

	ApplyValues(Unit->Stats.DisplayName, bIsPlayer, Unit->Integrity, MaxHp, Unit->Barrier, Unit->AP, MaxAp, Summary);
}

void UERHeroPlate::ShowWaiting(bool bWaiting)
{
	if (bWaiting)
	{
		if (NameText)   { NameText->SetText(FText::GetEmpty()); }
		if (HpNumber)   { HpNumber->SetText(WaitingPlaceholder); }
		if (HpMaxText)  { HpMaxText->SetText(FText::GetEmpty()); }
		if (BarrierText){ BarrierText->SetVisibility(ESlateVisibility::Collapsed); }
		if (HpLine)     { HpLine->SetPercent(0.0f); }
		if (ApPips)     { ApPips->SetPips(0, 0); }
		if (ApText)     { ApText->SetText(FText::GetEmpty()); }
		if (StatusText) { StatusText->SetVisibility(ESlateVisibility::Collapsed); }
	}
}
