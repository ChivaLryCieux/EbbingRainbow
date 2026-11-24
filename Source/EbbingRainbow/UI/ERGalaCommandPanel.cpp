// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERGalaCommandPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ERLog.h"
#include "UI/ERActionButton.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUITheme.h"
#include "UI/Primitives/ERTechPrimitives.h"

TSharedRef<SWidget> UERGalaCommandPanel::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UERGalaCommandPanel::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		BuildLayout();
	}
	Super::NativeConstruct();
	SetVisibility(bIsOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RefreshAll();
}

void UERGalaCommandPanel::OpenPanel(int32 InitialTab)
{
	bIsOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	SetActiveTab(InitialTab);
	RefreshAll();
}

void UERGalaCommandPanel::ClosePanel()
{
	if (!bIsOpen) { return; }
	bIsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	OnPanelClosed.Broadcast();
}

void UERGalaCommandPanel::SetActiveTab(int32 TabIndex)
{
	ActiveTab = FMath::Clamp(TabIndex, 0, 1);
	if (PanelTitleText)
	{
		PanelTitleText->SetText(ActiveTab == 0
			? INVTEXT("虹膜基金会  ·  银河委托与战局终端")
			: INVTEXT("虹膜基金会  ·  虹膜兵团整备终端"));
	}
	if (ContractsPage)
	{
		ContractsPage->SetVisibility(ActiveTab == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RosterPage)
	{
		RosterPage->SetVisibility(ActiveTab == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

UERActionButton* UERGalaCommandPanel::MakeBoneButton(const FText& Label, int32 FontSize, bool bArrow,
	TFunction<void()> OnClick)
{
	UERActionButton* B = UERActionButton::Make(this, Label, FontSize, bArrow);
	B->SetNativeClick(MoveTemp(OnClick));
	return B;
}

void UERGalaCommandPanel::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// 全屏哑光黑曜石底幕
	UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Scrim"));
	Scrim->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.005f, 0.006f, 0.010f, 0.95f), 0.0f, FLinearColor::Transparent));
	Scrim->SetBrushColor(FLinearColor::White);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Scrim);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// 顶部 3px 通栏虹彩条
	UERPrismBar* TopPrism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("TopPrism"));
	TopPrism->bVertical = false;
	TopPrism->BarSize = FVector2D(1920.0f, 3.0f);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(TopPrism);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	// 主纵向容器
	UVerticalBox* MainCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainCol"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(MainCol);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(48.0f, 28.0f, 48.0f, 32.0f));
	}

	// ---- 1. 顶栏: 虹膜基金会标题 + 口号 + 经费 + 返回 ----
	{
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* BrandCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PanelTitleText = ERUI::MakeText(this, INVTEXT("虹膜基金会  ·  银河军事指挥总署"), EERTypeScale::Title, T->Bone);
		PanelTitleText->SetFont(ERUI::FontBold(34, 80));
		BrandCol->AddChildToVerticalBox(PanelTitleText);

		UTextBlock* Slogan = ERUI::MakeText(this,
			INVTEXT("「 我们面向全银河提供军事指挥服务 」"),
			EERTypeScale::Caption, T->Accent, false);
		Slogan->SetFont(ERUI::FontBold(17, 120));
		BrandCol->AddChildToVerticalBox(Slogan)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(BrandCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 经费与周期
		UVerticalBox* StatCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CreditsText = ERUI::MakeText(this, INVTEXT("经费 1200 信用点"), EERTypeScale::Title, T->Bone, false);
		CreditsText->SetFont(ERUI::FontBold(28, 40));
		CreditsText->SetJustification(ETextJustify::Right);
		StatCol->AddChildToVerticalBox(CreditsText);

		CycleText = ERUI::MakeText(this, INVTEXT("第 01 周期 · 虹膜神经链路在线"), EERTypeScale::Micro, T->BoneSecondary, false);
		CycleText->SetFont(ERUI::FontBold(15, 60));
		CycleText->SetJustification(ETextJustify::Right);
		StatCol->AddChildToVerticalBox(CycleText);
		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(StatCol);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 28.0f, 0.0f));
		}

		// 仅保留返回大厅按钮 (兵团整备与银河委托为两块互相独立、不可互达的面板)
		UERActionButton* CloseBtn = MakeBoneButton(INVTEXT("返回大厅"), 26, true, [this]() { ClosePanel(); });
		CloseBtn->SetMinSize(220.0f, 0.0f);
		HeaderRow->AddChildToHorizontalBox(CloseBtn)->SetVerticalAlignment(VAlign_Center);

		MainCol->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	// 分割线
	UERTechDivider* Div = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass());
	Div->AnchorLength = 42.0f;
	Div->AnchorColor = T->Accent;
	Div->LineColor = T->BoneGhost;
	MainCol->AddChildToVerticalBox(Div)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	// 内容区容器 (Overlay 切换 Tab0 / Tab1)
	UOverlay* ContentOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ContentOverlay"));
	{
		UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(ContentOv);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// =========================================================================
	// TAB 0: 银河委托与战局态势
	// =========================================================================
	{
		UHorizontalBox* Page0 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContractsPage"));
		ContractsPage = Page0;
		{
			UOverlaySlot* S = ContentOv->AddChildToOverlay(Page0);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		// 左侧: 三大阵营控制度 + 星区控制表 + 上局战报
		UScrollBox* LeftScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* LeftCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftScroll->AddChild(LeftCol);
		{
			UHorizontalBoxSlot* S = Page0->AddChildToHorizontalBox(LeftScroll);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
		}

		LeftCol->AddChildToVerticalBox(ERUI::MakeSectionBar(this, INVTEXT("银河三方势力态势"), 22))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		FactionListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftCol->AddChildToVerticalBox(FactionListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

		LeftCol->AddChildToVerticalBox(ERUI::MakeSectionBar(this, INVTEXT("重点星区归属"), 22))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		SectorListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftCol->AddChildToVerticalBox(SectorListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		// 上一战况 (骨白栏目卡, 默认折叠)
		LastOutcomeBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LastOutcomeBox->SetBrush(FSlateBrush());
		LastOutcomeBox->SetVisibility(ESlateVisibility::Collapsed);
		{
			ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, T->Accent, true, FMargin(18.0f, 12.0f));
			LastOutcomeBox->SetContent(Card.Root);
			LastOutcomeText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, ERUI::InkText(), false);
			LastOutcomeText->SetFont(ERUI::FontBold(16, 30));
			LastOutcomeText->SetAutoWrapText(true);
			Card.Content->AddChildToVerticalBox(LastOutcomeText);
		}
		LeftCol->AddChildToVerticalBox(LastOutcomeBox);

		// 右侧: 阵营军事委托列表
		UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			UHorizontalBoxSlot* S = Page0->AddChildToHorizontalBox(RightCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UHorizontalBoxSlot* LS = Cast<UHorizontalBoxSlot>(LeftScroll->Slot))
		{
			FSlateChildSize SZ(ESlateSizeRule::Fill);
			SZ.Value = 0.40f;
			LS->SetSize(SZ);
		}
		if (UHorizontalBoxSlot* RS = Cast<UHorizontalBoxSlot>(RightCol->Slot))
		{
			FSlateChildSize SZ(ESlateSizeRule::Fill);
			SZ.Value = 0.60f;
			RS->SetSize(SZ);
		}

		UHorizontalBox* ContractHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		{
			UHorizontalBoxSlot* S = ContractHeader->AddChildToHorizontalBox(ERUI::MakeSectionBar(this, INVTEXT("待接取军事委托"), 22));
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}
		UERActionButton* RefreshBtn = MakeBoneButton(INVTEXT("刷新加密频道"), 21, false, [this]() { OnRefreshContractsClicked(); });
		ContractHeader->AddChildToHorizontalBox(RefreshBtn)->SetVerticalAlignment(VAlign_Center);
		RightCol->AddChildToVerticalBox(ContractHeader)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		UScrollBox* ContractScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		{
			UVerticalBoxSlot* S = RightCol->AddChildToVerticalBox(ContractScroll);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		ContractListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ContractScroll->AddChild(ContractListBox);
	}

	// =========================================================================
	// TAB 1: 虹膜兵团整备与扩编
	// =========================================================================
	{
		UVerticalBox* Page1 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RosterPage"));
		RosterPage = Page1;
		Page1->SetVisibility(ESlateVisibility::Collapsed);
		{
			UOverlaySlot* S = ContentOv->AddChildToOverlay(Page1);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RosterSummaryText = ERUI::MakeText(this, INVTEXT("虹膜链接兵团"), EERTypeScale::Body, T->Bone, false);
		RosterSummaryText->SetFont(ERUI::FontBold(21, 60));
		{
			UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(RosterSummaryText);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UERActionButton* ReinforceAllBtn = MakeBoneButton(INVTEXT("一键满编补员"), 24, false, [this]() { OnReinforceAllClicked(); });
		TopBar->AddChildToHorizontalBox(ReinforceAllBtn)->SetVerticalAlignment(VAlign_Center);
		Page1->AddChildToVerticalBox(TopBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		RecruitBarBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Page1->AddChildToVerticalBox(RecruitBarBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		UScrollBox* RosterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		{
			UVerticalBoxSlot* S = Page1->AddChildToVerticalBox(RosterScroll);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		RosterListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RosterScroll->AddChild(RosterListBox);
	}
}

void UERGalaCommandPanel::RefreshAll()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (!Campaign) { return; }

	if (CreditsText)
	{
		CreditsText->SetText(FText::FromString(FString::Printf(TEXT("经费 %d 信用点"), Campaign->GetCredits())));
	}
	if (CycleText)
	{
		CycleText->SetText(FText::FromString(FString::Printf(TEXT("第 %02d 周期 · 虹膜神经链路在线"), Campaign->GetCycleIndex())));
	}

	RebuildFactionAndSectorList();
	RebuildContractList();
	RebuildRosterList();
}

void UERGalaCommandPanel::RebuildFactionAndSectorList()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (!Campaign || !FactionListBox || !SectorListBox) { return; }
	const UERUITheme* T = ERUI::Theme();

	FactionListBox->ClearChildren();
	for (const FFactionStatus& F : Campaign->GetFactions())
	{
		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, F.Color, false, FMargin(16.0f, 12.0f));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* NameTxt = ERUI::MakeText(this, FText::FromString(F.DisplayName), EERTypeScale::Caption, ERUI::InkText(), false);
		NameTxt->SetFont(ERUI::FontBold(21, 60));
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameTxt);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		UTextBlock* PctTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("控制率 %.1f%%  ·  声望 %+d"), F.GalacticControl, F.Reputation)),
			EERTypeScale::Micro, ERUI::InkText(0.8f), false);
		PctTxt->SetFont(ERUI::FontBold(16, 40));
		Row->AddChildToHorizontalBox(PctTxt);
		Card.Content->AddChildToVerticalBox(Row);

		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Bar->SetPercent(FMath::Clamp(F.GalacticControl / 100.0f, 0.0f, 1.0f));
		Bar->SetFillColorAndOpacity(F.Color);
		Card.Content->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));

		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(F.DoctrineBrief), EERTypeScale::Micro, ERUI::InkText(0.62f), false);
		Brief->SetFont(ERUI::FontRegular(14, 20));
		Brief->SetAutoWrapText(true);
		Card.Content->AddChildToVerticalBox(Brief);

		FactionListBox->AddChildToVerticalBox(Card.Root)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	SectorListBox->ClearChildren();
	for (const FGalacticSector& Sec : Campaign->GetSectors())
	{
		const FLinearColor OwnerCol = ERGalacticMeta::GetFactionColor(Sec.ControllingFaction);
		ERUI::FBoneCard Row = ERUI::MakeBoneCard(this, OwnerCol, false, FMargin(14.0f, 8.0f), false);

		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* SecName = ERUI::MakeText(this, FText::FromString(Sec.Name), EERTypeScale::Micro, ERUI::InkText(), false);
		SecName->SetFont(ERUI::FontBold(18, 40));
		{
			UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(SecName);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UTextBlock* OwnerTxt = ERUI::MakeText(this,
			FText::FromString(ERGalacticMeta::GetFactionName(Sec.ControllingFaction)),
			EERTypeScale::Micro, ERUI::InkText(0.78f), false);
		OwnerTxt->SetFont(ERUI::FontBold(17, 60));
		H->AddChildToHorizontalBox(OwnerTxt);
		Row.Content->AddChildToVerticalBox(H);

		SectorListBox->AddChildToVerticalBox(Row.Root)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	if (LastOutcomeBox && LastOutcomeText)
	{
		const FLegionBattleOutcome& Out = Campaign->GetLastOutcome();
		if (Out.bValid && Out.bFromContract)
		{
			LastOutcomeBox->SetVisibility(ESlateVisibility::Visible);
			LastOutcomeText->SetText(FText::FromString(FString::Printf(
				TEXT("上一战况 [%s]: %s\n经费 +%d 信用点 · 我方伤亡 %d · %s控制率 %+.1f%%"),
				Out.bVictory ? TEXT("胜利") : TEXT("撤离"),
				*Out.ContractTitle,
				Out.CreditsEarned,
				Out.PlayerCasualties,
				*ERGalacticMeta::GetFactionName(Out.Employer),
				Out.ControlShiftApplied)));
		}
		else
		{
			LastOutcomeBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UERGalaCommandPanel::RebuildContractList()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (!Campaign || !ContractListBox) { return; }
	const UERUITheme* T = ERUI::Theme();

	ContractListBox->ClearChildren();
	for (const FGalacticContract& C : Campaign->GetAvailableContracts())
	{
		const FLinearColor EmpCol = ERGalacticMeta::GetFactionColor(C.Employer);
		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, EmpCol, false, FMargin(22.0f, 16.0f));
		UVerticalBox* Inner = Card.Content;

		// 标题行
		UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Title = ERUI::MakeText(this, FText::FromString(C.Title), EERTypeScale::Title, ERUI::InkText(), false);
		Title->SetFont(ERUI::FontBold(26, 40));
		{
			UHorizontalBoxSlot* S = TopRow->AddChildToHorizontalBox(Title);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		FString Stars;
		for (int32 s = 0; s < 5; ++s) { Stars += (s < C.DangerRating) ? TEXT("★") : TEXT("☆"); }
		UTextBlock* DangerTxt = ERUI::MakeText(this, FText::FromString(FString::Printf(TEXT("威胁 %s"), *Stars)),
			EERTypeScale::Caption, FLinearColor(0.58f, 0.31f, 0.0f, 1.0f), false);
		DangerTxt->SetFont(ERUI::FontBold(18, 60));
		TopRow->AddChildToHorizontalBox(DangerTxt);
		Inner->AddChildToVerticalBox(TopRow);

		// 雇主与战区行
		const FGalacticSector* Sec = Campaign->FindSector(C.TargetSectorId);
		const FString SecName = Sec ? Sec->Name : TEXT("未知星区");
		UTextBlock* Meta = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("雇主: %s   →   敌对: %s   |   战区: %s"),
				*ERGalacticMeta::GetFactionName(C.Employer),
				*ERGalacticMeta::GetFactionName(C.EnemyFaction),
				*SecName)),
			EERTypeScale::Micro, ERUI::InkText(0.82f), false);
		Meta->SetFont(ERUI::FontBold(16, 40));
		Inner->AddChildToVerticalBox(Meta)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 7.0f));

		// 简报
		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(C.Briefing), EERTypeScale::Body, ERUI::InkText(0.68f), false);
		Brief->SetFont(ERUI::FontRegular(16, 20));
		Brief->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(Brief)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		// 敌情与报酬 + 出击按钮
		UHorizontalBox* BotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Intel = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("敌情: %d 波次 · %d 名作战单位  |  酬金: +%d 信用点  |  控制权 %+.1f%%"),
				C.EnemyWaves.Num(), C.GetTotalEnemySoldiers(), C.RewardCredits, C.ControlShift)),
			EERTypeScale::Micro, ERUI::InkText(), false);
		Intel->SetFont(ERUI::FontBold(16, 30));
		Intel->SetAutoWrapText(true);
		{
			UHorizontalBoxSlot* S = BotRow->AddChildToHorizontalBox(Intel);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}

		const int32 ContractId = C.ContractId;
		UERActionButton* DeployBtn = MakeBoneButton(INVTEXT("接入虹膜 · 出击部署"), 24, true,
			[this, ContractId]()
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					Camp->AcceptAndDeployContract(ContractId, this);
				}
			});
		BotRow->AddChildToHorizontalBox(DeployBtn)->SetVerticalAlignment(VAlign_Center);
		Inner->AddChildToVerticalBox(BotRow);

		ContractListBox->AddChildToVerticalBox(Card.Root)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}
}

void UERGalaCommandPanel::RebuildRosterList()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (!Campaign || !RosterListBox || !RecruitBarBox) { return; }
	const UERUITheme* T = ERUI::Theme();

	int32 TotalAlive = 0, TotalMax = 0;
	for (const FManagedLegionData& L : Campaign->GetPlayerLegions())
	{
		TotalAlive += L.AliveSoldiers;
		TotalMax += L.MaxSoldiers;
	}
	if (RosterSummaryText)
	{
		RosterSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("现役虹膜链接兵团: %d / %d 支   |   总可用兵力: %d / %d"),
			Campaign->GetPlayerLegions().Num(), UGalacticCampaignSubsystem::MaxPlayerLegions, TotalAlive, TotalMax)));
	}

	RecruitBarBox->ClearChildren();
	UTextBlock* RecLabel = ERUI::MakeText(this, INVTEXT("新兵团扩编："), EERTypeScale::Micro, T->BoneSecondary, false);
	RecLabel->SetFont(ERUI::FontBold(18, 60));
	RecruitBarBox->AddChildToHorizontalBox(RecLabel)->SetVerticalAlignment(VAlign_Center);

	static const ELegionClass AllClasses[4] = {
		ELegionClass::Shieldbearer, ELegionClass::Pikeman, ELegionClass::Vanguard, ELegionClass::Marksman
	};
	const bool bCanRecruitMore = Campaign->GetPlayerLegions().Num() < UGalacticCampaignSubsystem::MaxPlayerLegions;
	for (int32 i = 0; i < 4; ++i)
	{
		const ELegionClass Cls = AllClasses[i];
		const int32 Cost = Campaign->GetRecruitCost(Cls);
		const bool bCanAfford = bCanRecruitMore && (Campaign->GetCredits() >= Cost);
		UERActionButton* Btn = MakeBoneButton(
			FText::FromString(FString::Printf(TEXT("招募%s · %d 信用点"), *ERGalacticMeta::GetClassName(Cls), Cost)),
			20, false,
			[this, Cls]()
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->RecruitLegion(Cls)) { RefreshAll(); }
				}
			});
		Btn->SetAccentColor(T->AllyMark, false);
		Btn->SetButtonEnabled(bCanAfford);
		RecruitBarBox->AddChildToHorizontalBox(Btn)->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
	}

	RosterListBox->ClearChildren();
	for (const FManagedLegionData& L : Campaign->GetPlayerLegions())
	{
		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, L.GetMissingSoldiers() > 0 ? T->Accent : T->AllyMark, false, FMargin(20.0f, 14.0f));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Card.Content->AddChildToVerticalBox(Row);

		UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		InfoCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InfoCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* NameTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("%s   ·   %s   ·   虹膜同步 %d 级"),
				*L.Callsign, *ERGalacticMeta::GetClassCode(L.Archetype), L.SyncLevel)),
			EERTypeScale::Title, ERUI::InkText(), false);
		NameTxt->SetFont(ERUI::FontBold(23, 40));
		InfoCol->AddChildToVerticalBox(NameTxt);

		UTextBlock* RoleTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("定位: %s   |   主动协议: %s   |   战历: %d 场"),
				*ERGalacticMeta::GetClassRoleDesc(L.Archetype),
				*ERGalacticMeta::GetClassSkillName(L.Archetype),
				L.BattlesSurvived)),
			EERTypeScale::Micro, ERUI::InkText(0.66f), false);
		RoleTxt->SetFont(ERUI::FontBold(15, 20));
		InfoCol->AddChildToVerticalBox(RoleTxt)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));

		UTextBlock* CountTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("兵员  %d / %d"), L.AliveSoldiers, L.MaxSoldiers)),
			EERTypeScale::Title, L.GetMissingSoldiers() > 0 ? FLinearColor(0.58f, 0.31f, 0.0f, 1.0f) : FLinearColor(0.02f, 0.30f, 0.62f, 1.0f), false);
		CountTxt->SetFont(ERUI::FontBold(24, 40));
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CountTxt);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));
		}

		const int32 ReinforceCost = L.GetFullReinforceCost();
		const bool bNeedReinforce = L.GetMissingSoldiers() > 0 && Campaign->GetCredits() >= L.GetReinforceCostPerSoldier();
		const int32 LegionId = L.LegionId;
		UERActionButton* ReinfBtn = MakeBoneButton(
			FText::FromString(L.GetMissingSoldiers() > 0
				? FString::Printf(TEXT("补充兵员 · %d 信用点"), ReinforceCost)
				: TEXT("兵员满编")),
			20, false,
			[this, LegionId]()
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->ReinforceLegion(LegionId)) { RefreshAll(); }
				}
			});
		ReinfBtn->SetButtonEnabled(bNeedReinforce);
		Row->AddChildToHorizontalBox(ReinfBtn)->SetVerticalAlignment(VAlign_Center);

		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(12.0f, 1.0f));
		Row->AddChildToHorizontalBox(Gap);

		const int32 SyncCost = L.GetSyncUpgradeCost();
		const bool bCanUpgrade = (L.SyncLevel < 5) && (Campaign->GetCredits() >= SyncCost);
		UERActionButton* UpgBtn = MakeBoneButton(
			FText::FromString(L.SyncLevel >= 5
				? TEXT("虹膜同步已满级")
				: FString::Printf(TEXT("升级同步 · %d 信用点"), SyncCost)),
			20, false,
			[this, LegionId]()
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->UpgradeLegionSync(LegionId)) { RefreshAll(); }
				}
			});
		UpgBtn->SetAccentColor(T->AllyMark, false);
		UpgBtn->SetButtonEnabled(bCanUpgrade);
		Row->AddChildToHorizontalBox(UpgBtn)->SetVerticalAlignment(VAlign_Center);

		RosterListBox->AddChildToVerticalBox(Card.Root)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}
}

void UERGalaCommandPanel::OnRefreshContractsClicked()
{
	if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
	{
		Camp->RefreshContracts();
		RefreshAll();
	}
}

void UERGalaCommandPanel::OnReinforceAllClicked()
{
	if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
	{
		if (Camp->ReinforceAllLegions() > 0)
		{
			RefreshAll();
		}
	}
}
