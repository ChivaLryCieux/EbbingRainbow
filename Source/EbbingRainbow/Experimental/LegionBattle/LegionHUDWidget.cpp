// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ERLog.h"
#include "Engine/World.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"
#include "Kismet/GameplayStatics.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUITheme.h"
#include "UI/Primitives/ERTechPrimitives.h"

TSharedRef<SWidget> ULegionHUDWidget::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	return Super::RebuildWidget();
}

void ULegionHUDWidget::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	RootPanel = Root;

	auto Anchor = [](UWidget* W, UCanvasPanel* P, const FAnchors& A, const FVector2D& Align,
		const FVector2D& Pos, bool bAutoSize = true)
	{
		UCanvasPanelSlot* S = P->AddChildToCanvas(W);
		S->SetAnchors(A);
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		S->SetAutoSize(bAutoSize);
		return S;
	};

	auto MakeFlatBtn = [&](const TCHAR* Name, const FLinearColor& Bg, const FLinearColor& BorderCol,
		TObjectPtr<UTextBlock>& OutLabel, const FText& Text, const FLinearColor& TextCol) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FSlateBrush Tr = ERUI::TransparentBrush();
		FButtonStyle BS;
		BS.SetNormal(Tr); BS.SetHovered(Tr); BS.SetPressed(Tr); BS.SetDisabled(Tr);
		BS.SetNormalPadding(FMargin(0.0f)); BS.SetPressedPadding(FMargin(0.0f));
		B->SetStyle(BS);
		B->SetCursor(EMouseCursor::Hand);

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(ERUI::RoundedBrush(0.0f, Bg, 1.0f, BorderCol));
		Frame->SetBrushColor(FLinearColor::White);
		Frame->SetPadding(FMargin(14.0f, 7.0f));
		B->SetContent(Frame);

		OutLabel = ERUI::MakeText(this, Text, EERTypeScale::Caption, TextCol, true);
		OutLabel->SetFont(ERUI::FontBold(11, 140));
		OutLabel->SetJustification(ETextJustify::Center);
		Frame->SetContent(OutLabel);
		return B;
	};

	// ---- 左上: 虹彩竖条 + 委托/战场标题 ----
	{
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));

		UERPrismBar* Prism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("Prism"));
		Prism->bVertical = true;
		Prism->BarSize = FVector2D(4.0f, 56.0f);
		TitleRow->AddChildToHorizontalBox(Prism)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));

		UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));

		TitleText = ERUI::MakeText(this, INVTEXT("虹膜军团指挥终端"), EERTypeScale::Title, T->Bone);
		TitleText->SetFont(ERUI::FontBold(26, 50));
		TitleCol->AddChildToVerticalBox(TitleText);

		SubTitleText = ERUI::MakeText(this, INVTEXT("IRIS FOUNDATION // GALACTIC MILITARY COMMAND"), EERTypeScale::Micro, T->BoneSecondary, true);
		SubTitleText->SetFont(ERUI::FontRegular(9, 260));
		TitleCol->AddChildToVerticalBox(SubTitleText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

		TitleRow->AddChildToHorizontalBox(TitleCol);
		Anchor(TitleRow, Root, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(T->ScreenMargin, 34.0f));
	}

	// ---- 右上: 编制/波次微标 + 时缓/暂停开关 ----
	{
		UVerticalBox* RightTopCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightTopCol"));

		ParamsText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, true);
		ParamsText->SetFont(ERUI::FontRegular(9, 200));
		ParamsText->SetJustification(ETextJustify::Right);
		RightTopCol->AddChildToVerticalBox(ParamsText);

		UHorizontalBox* TimeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TimeRow"));
		SlowMoButton = MakeFlatBtn(TEXT("SlowMoBtn"), T->InkPanel, T->BoneGhost, SlowMoLabel,
			INVTEXT("TAB  0.2x 时缓"), T->BoneSecondary);
		SlowMoButton->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnSlowMoClicked);
		TimeRow->AddChildToHorizontalBox(SlowMoButton);

		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(8.0f, 1.0f));
		TimeRow->AddChildToHorizontalBox(Gap);

		PauseButton = MakeFlatBtn(TEXT("PauseBtn"), T->InkPanel, T->BoneGhost, PauseLabel,
			INVTEXT("SPACE  暂停"), T->BoneSecondary);
		PauseButton->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPauseClicked);
		TimeRow->AddChildToHorizontalBox(PauseButton);

		RightTopCol->AddChildToVerticalBox(TimeRow)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		Anchor(RightTopCol, Root, FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(-T->ScreenMargin, 36.0f));
	}

	// ---- 左下: 双方编制存活行 ----
	{
		UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));

		StatusAllyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->AllyMark, true);
		StatusAllyText->SetFont(ERUI::FontBold(12, 110));
		StatusCol->AddChildToVerticalBox(StatusAllyText);

		StatusEnemyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->EnemyMark, true);
		StatusEnemyText->SetFont(ERUI::FontBold(12, 110));
		StatusCol->AddChildToVerticalBox(StatusEnemyText)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		Anchor(StatusCol, Root, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f),
			FVector2D(T->ScreenMargin, -84.0f));
	}

	// ---- 右下: 选中兵团主动协议按钮 ----
	{
		SkillButton = MakeFlatBtn(TEXT("SkillBtn"), T->ObsidianElevated, T->Accent, SkillLabel,
			INVTEXT("[ Z ] 激活协议"), T->Accent);
		SkillButton->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnSkillClicked);
		Anchor(SkillButton, Root, FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f),
			FVector2D(-T->ScreenMargin, -86.0f));
	}

	// ---- 底部居中: 兵团栏容器 ----
	{
		LegionBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LegionBar"));
		Anchor(LegionBar, Root, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -72.0f));
	}

	// ---- 底部居中: 操作提示 (黑曜石半透底板保证白色等高线上可读性) ----
	{
		UBorder* HintBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HintBg"));
		HintBg->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.009f, 0.012f, 0.80f), 1.0f, T->BoneGhost));
		HintBg->SetBrushColor(FLinearColor::White);
		HintBg->SetPadding(FMargin(18.0f, 6.0f));

		UVerticalBox* HintCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HintCol"));
		HintBg->SetContent(HintCol);

		HintText = ERUI::MakeText(this,
			INVTEXT("左键点/拖 框选兵团 · 右键点 移动/集火 · 右键拖拽 慢动作定阵型朝向 · Z 主动协议 · X 停驻 · 1-8 快选 · G 全军突击"),
			EERTypeScale::Micro, T->BoneSecondary);
		HintText->SetFont(ERUI::FontRegular(10, 110));
		HintText->SetJustification(ETextJustify::Center);
		HintCol->AddChildToVerticalBox(HintText);

		UTextBlock* CamHint = ERUI::MakeText(this,
			INVTEXT("WASD/推屏 平移 · 滚轮 缩放 · 中键 旋转 · Q/E 转向 · TAB 时缓 · SPACE 暂停"),
			EERTypeScale::Micro, T->BoneTertiary, true);
		CamHint->SetFont(ERUI::FontRegular(8, 160));
		CamHint->SetJustification(ETextJustify::Center);
		HintCol->AddChildToVerticalBox(CamHint)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

		Anchor(HintBg, Root, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -14.0f));
	}

	// ---- 中央结算面板 ----
	{
		ResultBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultBox"));
		ResultBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.010f, 0.014f, 0.94f), 1.5f, T->Accent));
		ResultBox->SetBrushColor(FLinearColor::White);
		ResultBox->SetPadding(FMargin(56.0f, 38.0f));
		ResultBox->SetVisibility(ESlateVisibility::Collapsed);

		UVerticalBox* ResCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ResultBox->SetContent(ResCol);

		ResultText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Hero, T->Bone);
		ResultText->SetFont(ERUI::FontBold(84, 240));
		ResultText->SetJustification(ETextJustify::Center);
		ResCol->AddChildToVerticalBox(ResultText);

		ResultSummaryText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->BoneSecondary);
		ResultSummaryText->SetFont(ERUI::FontRegular(14, 60));
		ResultSummaryText->SetJustification(ETextJustify::Center);
		ResCol->AddChildToVerticalBox(ResultSummaryText)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 22.0f));

		TObjectPtr<UTextBlock> RetLbl = nullptr;
		ReturnHallButton = MakeFlatBtn(TEXT("ReturnHallBtn"), T->Bone, T->Bone, RetLbl,
			INVTEXT("返 回 虹 膜 基 金 会 大 厅  //  RETURN TO FOUNDATION"), T->PureInk);
		ReturnHallButton->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnReturnHallClicked);
		ResCol->AddChildToVerticalBox(ReturnHallButton);

		ResultSubText = ERUI::MakeText(this, INVTEXT("或按 R 键就地重新部署沙盘"),
			EERTypeScale::Micro, T->BoneTertiary, true);
		ResultSubText->SetFont(ERUI::FontRegular(10, 180));
		ResultSubText->SetJustification(ETextJustify::Center);
		ResCol->AddChildToVerticalBox(ResultSubText)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));

		Anchor(ResultBox, Root, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	}
}

void ULegionHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}

	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		if (TitleText) { TitleText->SetText(FText::FromString(GM->GetBattleTitleText())); }
		if (SubTitleText) { SubTitleText->SetText(FText::FromString(GM->GetBattleSubText())); }
	}
	RefreshStatus();
	RefreshTimeControls();
}

void ULegionHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PollTimer -= InDeltaTime;
	if (PollTimer > 0.0f) { return; }
	PollTimer = 0.2f;
	RefreshStatus();
	RefreshTimeControls();
	RefreshLegionBar();

	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (GM && GM->IsBattleOver() && !bResultShown)
	{
		bResultShown = true;
		ShowResult(GM->DidPlayerWin());
	}
}

void ULegionHUDWidget::RefreshStatus()
{
	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GM) { return; }

	const int32 AllyAlive = GM->CountAliveSoldiers(ELegionTeam::Player);
	const int32 EnemyAlive = GM->CountAliveSoldiers(ELegionTeam::Enemy);
	const int32 Pending = GM->CountPendingEnemyWaves();

	if (AllyAlive != CachedAllyAlive || EnemyAlive != CachedEnemyAlive || Pending != CachedPendingWaves)
	{
		CachedAllyAlive = AllyAlive;
		CachedEnemyAlive = EnemyAlive;
		CachedPendingWaves = Pending;

		const int32 AllyLegions = GM->CountAliveLegions(ELegionTeam::Player);
		const int32 EnemyLegions = GM->CountAliveLegions(ELegionTeam::Enemy);
		StatusAllyText->SetText(FText::FromString(FString::Printf(
			TEXT("IRIS ALLY  //  %d 团 · %d 兵"), AllyLegions, AllyAlive)));
		StatusEnemyText->SetText(FText::FromString(FString::Printf(
			TEXT("HOSTILE  //  %d 团 · %d 兵  (+%d 待降波次)"), EnemyLegions, EnemyAlive, Pending)));
	}

	if (ParamsText)
	{
		ParamsText->SetText(FText::FromString(FString::Printf(
			TEXT("ELAPSED %.0fs  //  PENDING WAVES %d"), GM->GetBattleElapsed(), Pending)));
	}
}

void ULegionHUDWidget::BuildLegionBar(int32 Count)
{
	if (!LegionBar) { return; }
	const UERUITheme* T = ERUI::Theme();
	const FSlateBrush Tr = ERUI::TransparentBrush();
	for (int32 i = 0; i < Count && i < 8; ++i)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle BS;
		BS.SetNormal(Tr); BS.SetHovered(Tr); BS.SetPressed(Tr); BS.SetDisabled(Tr);
		BS.SetNormalPadding(FMargin(0.0f)); BS.SetPressedPadding(FMargin(0.0f));
		Btn->SetStyle(BS);
		Btn->SetCursor(EMouseCursor::Hand);
		switch (i)
		{
		case 0: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked0); break;
		case 1: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked1); break;
		case 2: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked2); break;
		case 3: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked3); break;
		case 4: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked4); break;
		case 5: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked5); break;
		case 6: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked6); break;
		default: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked7); break;
		}

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(ERUI::RoundedBrush(0.0f, T->InkPanel, 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.22f)));
		Frame->SetBrushColor(FLinearColor::White);
		Frame->SetPadding(FMargin(12.0f, 6.0f));
		Btn->SetContent(Frame);

		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Col);

		UTextBlock* Name = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->Bone, true);
		Name->SetFont(ERUI::FontBold(11, 100));
		Name->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(Name);

		UTextBlock* CountText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary, true);
		CountText->SetFont(ERUI::FontRegular(9, 100));
		CountText->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(CountText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		UTextBlock* StateText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary, true);
		StateText->SetFont(ERUI::FontRegular(8, 120));
		StateText->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(StateText)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));

		UTextBlock* SkillText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->AllyMark, true);
		SkillText->SetFont(ERUI::FontRegular(8, 80));
		SkillText->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(SkillText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		LegionBar->AddChildToHorizontalBox(Btn)->SetPadding(FMargin(i > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f));
		LegionPlates.Add(Frame);
		LegionNameTexts.Add(Name);
		LegionCountTexts.Add(CountText);
		LegionStateTexts.Add(StateText);
		LegionSkillTexts.Add(SkillText);
	}
}

void ULegionHUDWidget::RefreshLegionBar()
{
	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GM || !LegionBar) { return; }

	const int32 Num = GM->GetPlayerLegions().Num();
	if (Num != LegionPlates.Num())
	{
		LegionBar->ClearChildren();
		LegionPlates.Reset();
		LegionNameTexts.Reset();
		LegionCountTexts.Reset();
		LegionStateTexts.Reset();
		LegionSkillTexts.Reset();
		BuildLegionBar(Num);
	}
	if (Num == 0) { return; }

	ALegionPlayerController* PC = Cast<ALegionPlayerController>(World->GetFirstPlayerController());
	const UERUITheme* T = ERUI::Theme();
	static const TCHAR* StateNames[] = {
		TEXT("驻守"), TEXT("机动"), TEXT("交战"), TEXT("覆灭")
	};

	for (int32 i = 0; i < LegionPlates.Num(); ++i)
	{
		ALegionUnit* L = GM->GetPlayerLegions()[i];
		if (!L) { continue; }
		const bool bSel = PC && PC->IsLegionSelected(L);
		const bool bDead = L->IsWiped();
		const ELegionState St = L->GetLegionState();

		LegionNameTexts[i]->SetText(FText::FromString(FString::Printf(
			TEXT("%d·%s"), i + 1, *ERGalacticMeta::GetClassName(L->GetArchetype()))));
		LegionCountTexts[i]->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d 兵"), L->GetAliveCount(), L->GetSoldierCount())));
		LegionStateTexts[i]->SetText(FText::FromString(StateNames[(int32)St]));

		if (bDead)
		{
			LegionSkillTexts[i]->SetText(INVTEXT("信号断连"));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(T->DeadTint));
		}
		else if (L->IsSkillActive())
		{
			LegionSkillTexts[i]->SetText(INVTEXT("协议超频中"));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(T->Accent));
		}
		else if (L->IsSkillReady())
		{
			LegionSkillTexts[i]->SetText(FText::FromString(
				FString::Printf(TEXT("[Z] %s"), *ERGalacticMeta::GetClassSkillName(L->GetArchetype()))));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(T->AllyMark));
		}
		else
		{
			LegionSkillTexts[i]->SetText(FText::FromString(
				FString::Printf(TEXT("CD %.0fs"), L->GetSkillCooldownRemaining())));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(T->BoneTertiary));
		}

		LegionPlates[i]->SetBrush(ERUI::RoundedBrush(0.0f,
			bDead ? T->PureInk : (bSel ? T->ObsidianElevated : T->InkPanel),
			bSel ? 1.6f : 1.0f,
			bDead ? T->BoneGhost : (bSel ? T->Accent : FLinearColor(0.92f, 0.92f, 0.90f, 0.22f))));
		LegionPlates[i]->SetBrushColor(FLinearColor::White);

		LegionNameTexts[i]->SetColorAndOpacity(FSlateColor(bDead ? T->DeadTint : (bSel ? T->Accent : T->Bone)));
		LegionCountTexts[i]->SetColorAndOpacity(FSlateColor(bDead ? T->DeadTint : T->BoneSecondary));
		const FLinearColor StCol = bDead ? T->DeadTint
			: (St == ELegionState::Engaged ? T->DangerColor
			: (St == ELegionState::Moving ? T->Accent : T->BoneTertiary));
		LegionStateTexts[i]->SetColorAndOpacity(FSlateColor(StCol));
	}

	if (SkillButton && SkillLabel && PC)
	{
		ALegionUnit* Prim = PC->GetSelectedLegion();
		if (Prim && !Prim->IsWiped())
		{
			SkillButton->SetIsEnabled(Prim->IsSkillReady());
			SkillLabel->SetText(FText::FromString(Prim->IsSkillReady()
				? FString::Printf(TEXT("[Z] 激活 · %s"), *ERGalacticMeta::GetClassSkillName(Prim->GetArchetype()))
				: FString::Printf(TEXT("冷却中 %.0fs"), Prim->GetSkillCooldownRemaining())));
		}
		else
		{
			SkillButton->SetIsEnabled(false);
			SkillLabel->SetText(INVTEXT("[Z] 选中兵团启用协议"));
		}
	}
}

void ULegionHUDWidget::SelectPlate(int32 Index)
{
	if (ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		PC->SelectLegionByIndex(Index);
	}
}

void ULegionHUDWidget::RefreshTimeControls()
{
	ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
	if (!GM) { return; }
	const UERUITheme* T = ERUI::Theme();
	const ELegionTimeMode M = GM->GetTacticalTimeMode();

	if (SlowMoLabel)
	{
		SlowMoLabel->SetText(M == ELegionTimeMode::SlowMo ? INVTEXT("▶  0.2x 时缓中") : INVTEXT("TAB  0.2x 时缓"));
		SlowMoLabel->SetColorAndOpacity(FSlateColor(M == ELegionTimeMode::SlowMo ? T->Accent : T->BoneSecondary));
	}
	if (PauseLabel)
	{
		PauseLabel->SetText(M == ELegionTimeMode::Paused ? INVTEXT("▶  继续指令") : INVTEXT("SPACE  暂停"));
		PauseLabel->SetColorAndOpacity(FSlateColor(M == ELegionTimeMode::Paused ? T->DangerColor : T->BoneSecondary));
	}
}

void ULegionHUDWidget::OnSlowMoClicked()
{
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		GM->SetTacticalTimeMode(GM->GetTacticalTimeMode() == ELegionTimeMode::SlowMo
			? ELegionTimeMode::Normal : ELegionTimeMode::SlowMo);
		RefreshTimeControls();
	}
}

void ULegionHUDWidget::OnPauseClicked()
{
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		GM->SetTacticalTimeMode(GM->GetTacticalTimeMode() == ELegionTimeMode::Paused
			? ELegionTimeMode::Normal : ELegionTimeMode::Paused);
		RefreshTimeControls();
	}
}

void ULegionHUDWidget::OnSkillClicked()
{
	if (ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		PC->TriggerSelectedSkills();
	}
}

void ULegionHUDWidget::OnReturnHallClicked()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")));
	}
}

void ULegionHUDWidget::ShowResult(bool bPlayerWon)
{
	const UERUITheme* T = ERUI::Theme();
	ResultText->SetText(bPlayerWon ? INVTEXT("胜  利") : INVTEXT("败  北"));
	ResultText->SetColorAndOpacity(FSlateColor(bPlayerWon ? T->Bone : T->DangerColor));

	FString Summary;
	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		const FLegionBattleOutcome& Out = Campaign->GetLastOutcome();
		if (Out.bValid && Out.bFromContract)
		{
			Summary = FString::Printf(
				TEXT("委托: %s\n基金经费 +%d CR  |  我方阵亡 %d 人  |  歼敌 %d 人\n%s 星区控制率 %+.1f%%"),
				*Out.ContractTitle,
				Out.CreditsEarned,
				Out.PlayerCasualties,
				Out.EnemyEliminated,
				*ERGalacticMeta::GetFactionName(Out.Employer),
				Out.ControlShiftApplied);
		}
		else if (Out.bValid)
		{
			Summary = FString::Printf(TEXT("演武结束  |  我方阵亡 %d 人  |  歼敌 %d 人"),
				Out.PlayerCasualties, Out.EnemyEliminated);
		}
	}
	if (ResultSummaryText)
	{
		ResultSummaryText->SetText(FText::FromString(Summary));
	}

	if (ResultBox)
	{
		ResultBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (HintText)
	{
		HintText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
