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
#include "Components/Overlay.h"
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
#include "UI/ERActionButton.h"
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

		SubTitleText = ERUI::MakeText(this, INVTEXT("虹膜基金会  ·  银河军事指挥"), EERTypeScale::Micro, T->BoneSecondary, false);
		SubTitleText->SetFont(ERUI::FontBold(13, 120));
		TitleCol->AddChildToVerticalBox(SubTitleText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

		TitleRow->AddChildToHorizontalBox(TitleCol);
		Anchor(TitleRow, Root, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(T->ScreenMargin, 34.0f));
	}

	// ---- 右上: 编制/波次微标 + 时缓/暂停开关 ----
	{
		UVerticalBox* RightTopCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightTopCol"));

		ParamsText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, false);
		ParamsText->SetFont(ERUI::FontBold(14, 80));
		ParamsText->SetJustification(ETextJustify::Right);
		RightTopCol->AddChildToVerticalBox(ParamsText);

		UHorizontalBox* TimeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TimeRow"));
		SlowMoButton = UERActionButton::Make(this, INVTEXT("0.2倍时缓"), 20);
		SlowMoButton->OnActionClicked.AddDynamic(this, &ULegionHUDWidget::OnSlowMoClicked);
		TimeRow->AddChildToHorizontalBox(SlowMoButton);

		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(10.0f, 1.0f));
		TimeRow->AddChildToHorizontalBox(Gap);

		PauseButton = UERActionButton::Make(this, INVTEXT("暂停"), 20);
		PauseButton->OnActionClicked.AddDynamic(this, &ULegionHUDWidget::OnPauseClicked);
		TimeRow->AddChildToHorizontalBox(PauseButton);

		RightTopCol->AddChildToVerticalBox(TimeRow)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		Anchor(RightTopCol, Root, FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(-T->ScreenMargin, 36.0f));
	}

	// ---- 左下: 双方编制存活行 ----
	{
		UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));

		StatusAllyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->AllyMark, false);
		StatusAllyText->SetFont(ERUI::FontBold(18, 60));
		StatusCol->AddChildToVerticalBox(StatusAllyText);

		StatusEnemyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->EnemyMark, false);
		StatusEnemyText->SetFont(ERUI::FontBold(18, 60));
		StatusCol->AddChildToVerticalBox(StatusEnemyText)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		Anchor(StatusCol, Root, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f),
			FVector2D(T->ScreenMargin, -84.0f));
	}

	// ---- 右下: 选中兵团主动协议按钮 ----
	{
		SkillButton = UERActionButton::Make(this, INVTEXT("激活协议"), 26, true);
		SkillButton->SetMinSize(260.0f, 0.0f);
		SkillButton->OnActionClicked.AddDynamic(this, &ULegionHUDWidget::OnSkillClicked);
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
			INVTEXT("准星瞄兵团 左键 点选 (Ctrl 累选) · 右键 点地面机动 / 点敌团集火 · 按住右键转动视角定阵型朝向 · Z 主动协议 · X 停驻 · 1-8 快选 · G 全军突击 · F 跟随"),
			EERTypeScale::Micro, T->BoneSecondary);
		HintText->SetFont(ERUI::FontRegular(10, 110));
		HintText->SetJustification(ETextJustify::Center);
		HintCol->AddChildToVerticalBox(HintText);

		CamHintText = ERUI::MakeText(this,
			INVTEXT("WASD 移动 · 鼠标 视角 · Shift 疾跑 · 空格 跳跃 · M / 点小地图 展开等高线图 · 按住 ALT 呼出光标 · TAB 时缓 · E 暂停"),
			EERTypeScale::Micro, T->BoneTertiary, true);
		UTextBlock* CamHint = CamHintText;
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

		ReturnHallButton = UERActionButton::Make(this, INVTEXT("返回虹膜基金会大厅"), 30, true);
		ReturnHallButton->SetMinSize(520.0f, 0.0f);
		ReturnHallButton->OnActionClicked.AddDynamic(this, &ULegionHUDWidget::OnReturnHallClicked);
		ResCol->AddChildToVerticalBox(ReturnHallButton);

		ResultSubText = ERUI::MakeText(this, INVTEXT("也可按 R 键就地重新部署沙盘"),
			EERTypeScale::Micro, T->BoneTertiary, false);
		ResultSubText->SetFont(ERUI::FontBold(15, 120));
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

	RefreshPlateHover();

	PollTimer -= InDeltaTime;
	if (PollTimer > 0.0f) { return; }
	PollTimer = 0.2f;
	RefreshStatus();
	RefreshTimeControls();
	RefreshLegionBar();
	RefreshHints();

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
			TEXT("我方  %d 团 · %d 兵"), AllyLegions, AllyAlive)));
		StatusEnemyText->SetText(FText::FromString(FString::Printf(
			TEXT("敌方  %d 团 · %d 兵  (另有 %d 个待降波次)"), EnemyLegions, EnemyAlive, Pending)));
	}

	if (ParamsText)
	{
		ParamsText->SetText(FText::FromString(FString::Printf(
			TEXT("战斗用时 %.0f 秒  ·  待降波次 %d"), GM->GetBattleElapsed(), Pending)));
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
		LegionPlateButtons.Add(Btn);
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

		// 白底墨字彩条栏目: 切角骨白底板 + 左侧点睛条, 内容全部墨字
		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, T->AllyMark, false, FMargin(16.0f, 9.0f));
		Btn->SetContent(Card.Root);
		LegionPlateFrames.Add(Card.Frame);
		LegionPlateSelected.Add(false);
		LegionPlateDead.Add(false);
		UVerticalBox* Col = Card.Content;

		UTextBlock* Name = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::InkText(), false);
		Name->SetFont(ERUI::FontBold(19, 60));
		Name->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(Name);

		UTextBlock* CountText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::InkText(0.7f), false);
		CountText->SetFont(ERUI::FontBold(15, 40));
		CountText->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(CountText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		UTextBlock* StateText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::InkText(0.5f), false);
		StateText->SetFont(ERUI::FontBold(14, 60));
		StateText->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(StateText)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));

		UTextBlock* SkillText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::InkText(0.6f), false);
		SkillText->SetFont(ERUI::FontBold(14, 40));
		SkillText->SetJustification(ETextJustify::Center);
		Col->AddChildToVerticalBox(SkillText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		LegionBar->AddChildToHorizontalBox(Btn)->SetPadding(FMargin(i > 0 ? 8.0f : 0.0f, 0.0f, 0.0f, 0.0f));
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
	if (Num != LegionPlateFrames.Num())
	{
		LegionBar->ClearChildren();
		LegionPlateButtons.Reset();
		LegionPlateFrames.Reset();
		LegionPlateSelected.Reset();
		LegionPlateDead.Reset();
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

	// 骨白底上的状态墨色 (深于暗底版本以保证对比)
	const FLinearColor InkDanger(0.62f, 0.04f, 0.02f, 1.0f);
	const FLinearColor InkAmber(0.58f, 0.31f, 0.0f, 1.0f);
	const FLinearColor InkBlue(0.02f, 0.30f, 0.62f, 1.0f);

	for (int32 i = 0; i < LegionPlateFrames.Num(); ++i)
	{
		ALegionUnit* L = GM->GetPlayerLegions()[i];
		if (!L) { continue; }
		const bool bSel = PC && PC->IsLegionSelected(L);
		const bool bDead = L->IsWiped();
		const ELegionState St = L->GetLegionState();
		LegionPlateSelected[i] = bSel;
		LegionPlateDead[i] = bDead;

		LegionNameTexts[i]->SetText(FText::FromString(FString::Printf(
			TEXT("%d·%s"), i + 1, *ERGalacticMeta::GetClassName(L->GetArchetype()))));
		LegionCountTexts[i]->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d 兵"), L->GetAliveCount(), L->GetSoldierCount())));
		LegionStateTexts[i]->SetText(FText::FromString(StateNames[(int32)St]));

		if (bDead)
		{
			LegionSkillTexts[i]->SetText(INVTEXT("信号断连"));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(ERUI::InkText(0.4f)));
		}
		else if (L->IsSkillActive())
		{
			LegionSkillTexts[i]->SetText(INVTEXT("协议超频中"));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(InkAmber));
		}
		else if (L->IsSkillReady())
		{
			LegionSkillTexts[i]->SetText(FText::FromString(
				FString::Printf(TEXT("就绪 · %s"), *ERGalacticMeta::GetClassSkillName(L->GetArchetype()))));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(InkBlue));
		}
		else
		{
			LegionSkillTexts[i]->SetText(FText::FromString(
				FString::Printf(TEXT("冷却 %.0f 秒"), L->GetSkillCooldownRemaining())));
			LegionSkillTexts[i]->SetColorAndOpacity(FSlateColor(ERUI::InkText(0.45f)));
		}

		LegionNameTexts[i]->SetColorAndOpacity(FSlateColor(ERUI::InkText(bDead ? 0.4f : 1.0f)));
		LegionCountTexts[i]->SetColorAndOpacity(FSlateColor(ERUI::InkText(bDead ? 0.35f : 0.7f)));
		const FLinearColor StCol = bDead ? ERUI::InkText(0.4f)
			: (St == ELegionState::Engaged ? InkDanger
			: (St == ELegionState::Moving ? InkAmber : ERUI::InkText(0.5f)));
		LegionStateTexts[i]->SetColorAndOpacity(FSlateColor(StCol));
	}

	if (SkillButton && PC)
	{
		ALegionUnit* Prim = PC->GetSelectedLegion();
		if (Prim && !Prim->IsWiped())
		{
			SkillButton->SetButtonEnabled(Prim->IsSkillReady());
			SkillButton->SetLabel(FText::FromString(Prim->IsSkillReady()
				? FString::Printf(TEXT("激活 · %s"), *ERGalacticMeta::GetClassSkillName(Prim->GetArchetype()))
				: FString::Printf(TEXT("冷却中 %.0f 秒"), Prim->GetSkillCooldownRemaining())));
		}
		else
		{
			SkillButton->SetButtonEnabled(false);
			SkillButton->SetLabel(INVTEXT("选中兵团后启用协议"));
		}
	}
}

void ULegionHUDWidget::RefreshPlateHover()
{
	for (int32 i = 0; i < LegionPlateFrames.Num(); ++i)
	{
		const bool bHover = LegionPlateButtons.IsValidIndex(i) && LegionPlateButtons[i] && LegionPlateButtons[i]->IsHovered();
		const bool bSel = LegionPlateSelected.IsValidIndex(i) && LegionPlateSelected[i];
		const bool bDead = LegionPlateDead.IsValidIndex(i) && LegionPlateDead[i];
		const UERUITheme* T = ERUI::Theme();
		ERUI::StyleBoneFrame(LegionPlateFrames[i], bHover ? 1.0f : 0.0f, !bDead, bSel,
			bSel ? T->Accent : T->AllyMark, false);
	}
}

void ULegionHUDWidget::RefreshHints()
{
	const ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	if (!PC || !HintText || !CamHintText) { return; }
	const bool bMap = PC->IsStrategicMapOpen();
	if (bMap == bHintMapMode) { return; }
	bHintMapMode = bMap;
	if (bMap)
	{
		HintText->SetText(INVTEXT("左键点我方番号 选中 (Ctrl 累选) · 再点敌方番号 集火 · 点空地 / 右键 机动 · Z 协议 · X 停驻 · 1-8 快选"));
		CamHintText->SetText(INVTEXT("滚轮 以鼠标为中心缩放 · 左键拖拽 / 中键拖拽 平移 · E 暂停 / 0.2 倍观战切换 · 顶部「返回战场」/ M / ESC 退出"));
	}
	else
	{
		HintText->SetText(INVTEXT("准星瞄兵团 左键 点选 (Ctrl 累选) · 右键 点地面机动 / 点敌团集火 · 按住右键转动视角定阵型朝向 · Z 主动协议 · X 停驻 · 1-8 快选 · G 全军突击 · F 跟随"));
		CamHintText->SetText(INVTEXT("WASD 移动 · 鼠标 视角 · Shift 疾跑 · 空格 跳跃 · M / 点小地图 展开等高线图 · 按住 ALT 呼出光标 · TAB 时缓 · E 暂停"));
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

	const ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	const bool bMap = PC && PC->IsStrategicMapOpen();
	if (SlowMoButton)
	{
		SlowMoButton->SetLabel(M == ELegionTimeMode::SlowMo ? INVTEXT("0.2倍时缓中") : INVTEXT("0.2倍时缓"));
		SlowMoButton->SetUrgent(M == ELegionTimeMode::SlowMo);
		SlowMoButton->SetButtonEnabled(!bMap);
	}
	if (PauseButton)
	{
		if (bMap)
		{
			PauseButton->SetLabel(M == ELegionTimeMode::Paused ? INVTEXT("地图暂停中") : INVTEXT("0.2倍观战中"));
			PauseButton->SetUrgent(false);
			PauseButton->SetButtonEnabled(false);
		}
		else
		{
			PauseButton->SetLabel(M == ELegionTimeMode::Paused ? INVTEXT("继续") : INVTEXT("暂停"));
			PauseButton->SetUrgent(M == ELegionTimeMode::Paused);
			PauseButton->SetButtonEnabled(true);
		}
	}
}

void ULegionHUDWidget::OnSlowMoClicked()
{
	if (const ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		if (PC->IsStrategicMapOpen()) { return; }
	}
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		GM->SetTacticalTimeMode(GM->GetTacticalTimeMode() == ELegionTimeMode::SlowMo
			? ELegionTimeMode::Normal : ELegionTimeMode::SlowMo);
		RefreshTimeControls();
	}
}

void ULegionHUDWidget::OnPauseClicked()
{
	if (const ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		if (PC->IsStrategicMapOpen()) { return; }
	}
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
				TEXT("委托: %s\n基金经费 +%d 信用点  |  我方阵亡 %d 人  |  歼敌 %d 人\n%s 星区控制率 %+.1f%%"),
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
