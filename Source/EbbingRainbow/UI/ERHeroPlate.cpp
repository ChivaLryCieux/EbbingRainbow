// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHeroPlate.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERUIStatics.h"
#include "UI/ERPipBar.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Battle/Units/BattleUnit.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
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
		HpSpring.SnapTo(TargetHpPct);
		HpTrailSpring.SnapTo(TargetHpPct);
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
	const UERUITheme* T = ERUI::Theme();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PlateRoot"));
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 哑光黑曜石 45° 切角战术铭牌 (确保在明亮灰混凝土战场上极度锐利可读)
	PlateFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("PlateFrame"));
	PlateFrame->Chamfer = FVector4(0.0f, 14.0f, 0.0f, 0.0f);
	PlateFrame->FillColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.90f);
	PlateFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.20f);
	PlateFrame->bDrawCornerBrackets = true;
	PlateFrame->BracketLength = 9.0f;
	PlateFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.72f);
	PlateFrame->AccentEdge = EERAccentEdge::Left;
	PlateFrame->AccentThickness = 3.5f;
	PlateFrame->AccentColor = T->AllyMark;
	PlateFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Root->AddChildToOverlay(PlateFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Col->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = Root->AddChildToOverlay(Col);
		S->SetPadding(FMargin(18.0f, 12.0f, 22.0f, 14.0f));
	}

	// 顶行: 队伍刻度 + 粗体单位名 + 极小遥测标识
	{
		UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NameRow"));
		NameRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		TeamTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TeamTick"));
		TeamTick->SetBrush(ERUI::WhiteBrush(FVector2D(4.0f, 12.0f)));
		TeamTick->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* TkSlot = NameRow->AddChildToHorizontalBox(TeamTick);
		TkSlot->SetVerticalAlignment(VAlign_Center);
		TkSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->Bone);
		NameText->SetFont(ERUI::FontBold(14, 60));
		NameRow->AddChildToHorizontalBox(NameText)->SetVerticalAlignment(VAlign_Center);

		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TagSpacer"));
		Sp->SetSize(FVector2D(16.0f, 1.0f));
		NameRow->AddChildToHorizontalBox(Sp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UnitTagText = ERUI::MakeText(this, INVTEXT("ACTIVE // HP"), EERTypeScale::Micro, T->BoneTertiary, true);
		UnitTagText->SetFont(ERUI::FontRegular(8, 220));
		NameRow->AddChildToHorizontalBox(UnitTagText)->SetVerticalAlignment(VAlign_Center);

		Col->AddChildToVerticalBox(NameRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	// Control: 共振式 54pt 巨幅 Linux Biolinum 粗体 HP 数字 + 极小 /Max + 护盾
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		HpNumber = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Display, T->Bone, true);
		HpNumber->SetFont(ERUI::FontBold(52, -10));
		Row->AddChildToHorizontalBox(HpNumber)->SetVerticalAlignment(VAlign_Bottom);

		HpMaxText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneTertiary, true);
		HpMaxText->SetFont(ERUI::FontRegular(13, 60));
		UHorizontalBoxSlot* MxSlot = Row->AddChildToHorizontalBox(HpMaxText);
		MxSlot->SetVerticalAlignment(VAlign_Bottom);
		MxSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 10.0f));

		BarrierText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->AllyMark, true);
		BarrierText->SetFont(ERUI::FontBold(13, 60));
		UHorizontalBoxSlot* BaSlot = Row->AddChildToHorizontalBox(BarrierText);
		BaSlot->SetVerticalAlignment(VAlign_Bottom);
		BaSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 10.0f));

		Col->AddChildToVerticalBox(Row);
	}

	// 双层精密 3px 血条 (底层弹簧拖尾伤血条 + 上层主血条)
	{
		UOverlay* BarOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HpBarOverlay"));
		BarOv->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		FProgressBarStyle TrailStyle;
		TrailStyle.SetBackgroundImage(ERUI::WhiteBrush());
		TrailStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.92f, 0.92f, 0.90f, 0.14f));
		TrailStyle.SetFillImage(ERUI::WhiteBrush());

		HpTrailLine = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HpTrailLine"));
		HpTrailLine->SetWidgetStyle(TrailStyle);
		HpTrailLine->SetFillColorAndOpacity(T->DangerColor);
		HpTrailLine->SetPercent(1.0f);
		HpTrailLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = BarOv->AddChildToOverlay(HpTrailLine);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		FProgressBarStyle MainStyle;
		MainStyle.SetBackgroundImage(ERUI::TransparentBrush());
		MainStyle.SetFillImage(ERUI::WhiteBrush());

		HpLine = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HpLine"));
		HpLine->SetWidgetStyle(MainStyle);
		HpLine->SetFillColorAndOpacity(T->Bone);
		HpLine->SetPercent(1.0f);
		HpLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = BarOv->AddChildToOverlay(HpLine);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LineBox"));
		Box->SetWidthOverride(236.0f);
		Box->SetHeightOverride(3.0f);
		Box->SetContent(BarOv);
		Col->AddChildToVerticalBox(Box)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 11.0f));
	}

	// AP 机能横条格 + Linux Biolinum 读数
	{
		UHorizontalBox* ApRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ApRow"));
		ApRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		ApPips = WidgetTree->ConstructWidget<UERPipBar>(UERPipBar::StaticClass(), TEXT("ApPips"));
		ApRow->AddChildToHorizontalBox(ApPips)->SetVerticalAlignment(VAlign_Center);

		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("ApSpacer"));
		Sp->SetSize(FVector2D(12.0f, 1.0f));
		ApRow->AddChildToHorizontalBox(Sp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ApText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, true);
		ApText->SetFont(ERUI::FontBold(11, 140));
		UHorizontalBoxSlot* ApSlot = ApRow->AddChildToHorizontalBox(ApText);
		ApSlot->SetVerticalAlignment(VAlign_Center);
		ApSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));

		Col->AddChildToVerticalBox(ApRow);
	}

	// 状态摘要
	StatusText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->Accent);
	StatusText->SetFont(ERUI::FontRegular(9, 140));
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	Col->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 7.0f, 0.0f, 0.0f));
}

void UERHeroPlate::BindUnit(ABattleUnit* InUnit)
{
	const bool bChanged = (Unit != InUnit);
	Unit = InUnit;
	Refresh();
	if (bChanged && !IsDesignTime())
	{
		HpSpring.SnapTo(TargetHpPct);
		HpTrailSpring.SnapTo(TargetHpPct);
		SweepProgress = 0.01f;
	}
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
	TargetHpPct = Pct;
	HpSpring.SetTarget(Pct);
	HpTrailSpring.SetTarget(Pct);
	if (Pct > HpTrailSpring.Value)
	{
		HpTrailSpring.SnapTo(Pct);
	}

	const FLinearColor TeamCol = bIsPlayer ? T->AllyMark : T->EnemyMark;
	if (PlateFrame)
	{
		PlateFrame->SetFrameColors(
			FLinearColor(0.008f, 0.009f, 0.012f, 0.90f),
			FLinearColor(0.92f, 0.92f, 0.90f, 0.22f),
			FLinearColor(0.92f, 0.92f, 0.90f, 0.72f),
			TeamCol);
	}
	if (UnitTagText)
	{
		UnitTagText->SetText(bIsPlayer ? INVTEXT("ACTIVE // ALLY") : INVTEXT("HOSTILE // TURN"));
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(DisplayName));
		NameText->SetColorAndOpacity(FSlateColor(bIsPlayer ? T->Bone : T->EnemyMark));
	}
	if (TeamTick)
	{
		TeamTick->SetColorAndOpacity(TeamCol);
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
		HpLine->SetPercent(IsDesignTime() ? Pct : HpSpring.Value);
		HpLine->SetFillColorAndOpacity(Pct < LowHpThreshold ? T->DangerColor : T->Bone);
	}
	if (HpTrailLine)
	{
		HpTrailLine->SetPercent(IsDesignTime() ? Pct : FMath::Max(HpSpring.Value, HpTrailSpring.Value));
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
		ApPips->SetPipColor(CurAp > 0 ? T->Bone : T->BoneTertiary);
		ApPips->SetPips(FMath::Clamp(CurAp, 0, MaxAp), FMath::Max(1, MaxAp));
	}
	if (ApText)
	{
		ApText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d AP"), CurAp, MaxAp)));
	}
	if (StatusText)
	{
		StatusText->SetVisibility(StatusSummary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		StatusText->SetText(FText::FromString(StatusSummary));
	}
}

void UERHeroPlate::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float MainPct  = HpSpring.Tick(InDeltaTime);
	const float TrailPct = HpTrailSpring.Tick(InDeltaTime);
	if (HpLine)      { HpLine->SetPercent(FMath::Clamp(MainPct, 0.0f, 1.0f)); }
	if (HpTrailLine) { HpTrailLine->SetPercent(FMath::Clamp(FMath::Max(MainPct, TrailPct), 0.0f, 1.0f)); }

	if (SweepProgress < 1.0f)
	{
		SweepProgress = FMath::Min(1.0f, SweepProgress + InDeltaTime / 0.45f);
		if (PlateFrame)
		{
			PlateFrame->SetMotionState(1.0f, 0.0f, SweepProgress);
		}
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
		if (NameText)    { NameText->SetText(FText::GetEmpty()); }
		if (HpNumber)    { HpNumber->SetText(WaitingPlaceholder); }
		if (HpMaxText)   { HpMaxText->SetText(FText::GetEmpty()); }
		if (BarrierText) { BarrierText->SetVisibility(ESlateVisibility::Collapsed); }
		if (HpLine)      { HpLine->SetPercent(0.0f); }
		if (HpTrailLine) { HpTrailLine->SetPercent(0.0f); }
		if (ApPips)      { ApPips->SetPips(0, 0); }
		if (ApText)      { ApText->SetText(FText::GetEmpty()); }
		if (StatusText)  { StatusText->SetVisibility(ESlateVisibility::Collapsed); }
	}
}
