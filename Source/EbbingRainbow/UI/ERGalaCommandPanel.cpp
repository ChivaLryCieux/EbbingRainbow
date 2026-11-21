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
			? INVTEXT("虹膜基金会 // 银河委托与战局终端")
			: INVTEXT("虹膜基金会 // 虹膜兵团整备终端"));
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

UButton* UERGalaCommandPanel::MakeStyledButton(const FText& Label, const FLinearColor& BgColor,
	const FLinearColor& BorderColor, const FLinearColor& TextColor,
	float PadX, float PadY, int32 FontSize)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FSlateBrush Tr = ERUI::TransparentBrush();
	FButtonStyle BS;
	BS.SetNormal(Tr); BS.SetHovered(Tr); BS.SetPressed(Tr); BS.SetDisabled(Tr);
	BS.SetNormalPadding(FMargin(0.0f)); BS.SetPressedPadding(FMargin(0.0f));
	B->SetStyle(BS);
	B->SetCursor(EMouseCursor::Hand);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrush(ERUI::RoundedBrush(0.0f, BgColor, 1.0f, BorderColor));
	Frame->SetBrushColor(FLinearColor::White);
	Frame->SetPadding(FMargin(PadX, PadY));
	B->SetContent(Frame);

	UTextBlock* Txt = ERUI::MakeText(this, Label, EERTypeScale::Caption, TextColor, true);
	Txt->SetFont(ERUI::FontBold(FontSize, 100));
	Txt->SetJustification(ETextJustify::Center);
	Frame->SetContent(Txt);
	return B;
}

UERGalaIndexButton* UERGalaCommandPanel::MakeIndexedStyledButton(int32 Index, TFunction<void(int32)> Callback,
	const FText& Label, const FLinearColor& BgColor, const FLinearColor& BorderColor,
	const FLinearColor& TextColor, float PadX, float PadY, int32 FontSize)
{
	UERGalaIndexButton* B = WidgetTree->ConstructWidget<UERGalaIndexButton>(UERGalaIndexButton::StaticClass());
	B->InitAction(Index, MoveTemp(Callback));
	FSlateBrush Tr = ERUI::TransparentBrush();
	FButtonStyle BS;
	BS.SetNormal(Tr); BS.SetHovered(Tr); BS.SetPressed(Tr); BS.SetDisabled(Tr);
	BS.SetNormalPadding(FMargin(0.0f)); BS.SetPressedPadding(FMargin(0.0f));
	B->SetStyle(BS);
	B->SetCursor(EMouseCursor::Hand);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrush(ERUI::RoundedBrush(0.0f, BgColor, 1.0f, BorderColor));
	Frame->SetBrushColor(FLinearColor::White);
	Frame->SetPadding(FMargin(PadX, PadY));
	B->SetContent(Frame);

	UTextBlock* Txt = ERUI::MakeText(this, Label, EERTypeScale::Caption, TextColor, true);
	Txt->SetFont(ERUI::FontBold(FontSize, 100));
	Txt->SetJustification(ETextJustify::Center);
	Frame->SetContent(Txt);
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

	// ---- 1. 顶栏: 虹膜基金会标题 + 口号 + 经费 + 页签切换 + 关闭 ----
	{
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* BrandCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PanelTitleText = ERUI::MakeText(this, INVTEXT("虹膜基金会 // 银河军事指挥总署"), EERTypeScale::Title, T->Bone);
		PanelTitleText->SetFont(ERUI::FontBold(28, 60));
		BrandCol->AddChildToVerticalBox(PanelTitleText);

		UTextBlock* Slogan = ERUI::MakeText(this,
			INVTEXT("「 我们面向全银河提供军事指挥服务 」  //  IRIS FOUNDATION COMMAND NETWORK"),
			EERTypeScale::Caption, T->Accent, true);
		Slogan->SetFont(ERUI::FontBold(12, 160));
		BrandCol->AddChildToVerticalBox(Slogan)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(BrandCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 遥测经费与周期
		UVerticalBox* StatCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CreditsText = ERUI::MakeText(this, INVTEXT("FUNDS: 1200 CR"), EERTypeScale::Title, T->Bone, true);
		CreditsText->SetFont(ERUI::FontBold(22, 40));
		CreditsText->SetJustification(ETextJustify::Right);
		StatCol->AddChildToVerticalBox(CreditsText);

		CycleText = ERUI::MakeText(this, INVTEXT("CYCLE 01 // IRIS LINK ONLINE"), EERTypeScale::Micro, T->BoneTertiary, true);
		CycleText->SetFont(ERUI::FontRegular(9, 220));
		CycleText->SetJustification(ETextJustify::Right);
		StatCol->AddChildToVerticalBox(CycleText);
		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(StatCol);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
		}

		// 仅保留返回大厅按钮 (兵团整备与银河委托为两块互相独立、不可互达的面板)
		UButton* CloseBtn = MakeStyledButton(INVTEXT("ESC  返回大厅"), T->InkPanel, T->DangerColor, T->BoneSecondary, 16.0f, 10.0f, 11);
		CloseBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnCloseClicked);
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

		UTextBlock* Sec1Title = ERUI::MakeText(this, INVTEXT("GALACTIC WARFRONT // 银河三方势力态势"), EERTypeScale::Caption, T->BoneSecondary, true);
		Sec1Title->SetFont(ERUI::FontBold(12, 160));
		LeftCol->AddChildToVerticalBox(Sec1Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		FactionListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftCol->AddChildToVerticalBox(FactionListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

		UTextBlock* Sec2Title = ERUI::MakeText(this, INVTEXT("SECTOR CONTROL // 重点星区归属"), EERTypeScale::Caption, T->BoneSecondary, true);
		Sec2Title->SetFont(ERUI::FontBold(12, 160));
		LeftCol->AddChildToVerticalBox(Sec2Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		SectorListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftCol->AddChildToVerticalBox(SectorListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		LastOutcomeBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LastOutcomeBox->SetBrush(ERUI::RoundedBrush(0.0f, T->ObsidianElevated, 1.0f, T->Accent));
		LastOutcomeBox->SetBrushColor(FLinearColor::White);
		LastOutcomeBox->SetPadding(FMargin(14.0f, 10.0f));
		LastOutcomeBox->SetVisibility(ESlateVisibility::Collapsed);
		LastOutcomeText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->Bone);
		LastOutcomeText->SetFont(ERUI::FontRegular(12, 30));
		LastOutcomeText->SetAutoWrapText(true);
		LastOutcomeBox->SetContent(LastOutcomeText);
		LeftCol->AddChildToVerticalBox(LastOutcomeBox);

		// 右侧: 阵营军事委托列表
		UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			UHorizontalBoxSlot* S = Page0->AddChildToHorizontalBox(RightCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			// 比例 1 : 1.55 左右
		}
		// 用 SizeBox 辅助让右侧占更多宽度 (先简单把左侧设为 0.42 权重, 右侧 0.58 权重)
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
		UTextBlock* Sec3Title = ERUI::MakeText(this, INVTEXT("INCOMING CONTRACTS // 待接取军事委托"), EERTypeScale::Caption, T->BoneSecondary, true);
		Sec3Title->SetFont(ERUI::FontBold(12, 160));
		{
			UHorizontalBoxSlot* S = ContractHeader->AddChildToHorizontalBox(Sec3Title);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UButton* RefreshBtn = MakeStyledButton(INVTEXT("↻  刷新加密频道"), T->ObsidianElevated, T->BoneGhost, T->BoneSecondary, 12.0f, 6.0f, 10);
		RefreshBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnRefreshContractsClicked);
		ContractHeader->AddChildToHorizontalBox(RefreshBtn);
		RightCol->AddChildToVerticalBox(ContractHeader)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

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
		RosterSummaryText = ERUI::MakeText(this, INVTEXT("IRIS LINKED LEGIONS"), EERTypeScale::Body, T->BoneSecondary);
		RosterSummaryText->SetFont(ERUI::FontBold(13, 60));
		{
			UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(RosterSummaryText);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UButton* ReinforceAllBtn = MakeStyledButton(INVTEXT("一键满编补员"), T->Bone, T->Bone, T->PureInk, 16.0f, 8.0f, 11);
		ReinforceAllBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnReinforceAllClicked);
		TopBar->AddChildToHorizontalBox(ReinforceAllBtn)->SetVerticalAlignment(VAlign_Center);
		Page1->AddChildToVerticalBox(TopBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		RecruitBarBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Page1->AddChildToVerticalBox(RecruitBarBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

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
		CreditsText->SetText(FText::FromString(FString::Printf(TEXT("FUNDS: %d CR"), Campaign->GetCredits())));
	}
	if (CycleText)
	{
		CycleText->SetText(FText::FromString(FString::Printf(TEXT("CYCLE %02d // IRIS NEURAL LINK ONLINE"), Campaign->GetCycleIndex())));
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
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrush(ERUI::RoundedBrush(0.0f, T->InkPanel, 1.0f, T->BoneGhost));
		Card->SetBrushColor(FLinearColor::White);
		Card->SetPadding(FMargin(14.0f, 10.0f));

		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Col);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* NameTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("%s  [%s]"), *F.DisplayName, *F.CodeName)),
			EERTypeScale::Caption, F.Color, true);
		NameTxt->SetFont(ERUI::FontBold(13, 60));
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameTxt);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		UTextBlock* PctTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("控制率 %.1f%%  ·  声望 %+d"), F.GalacticControl, F.Reputation)),
			EERTypeScale::Micro, T->Bone, true);
		PctTxt->SetFont(ERUI::FontBold(11, 80));
		Row->AddChildToHorizontalBox(PctTxt);
		Col->AddChildToVerticalBox(Row);

		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Bar->SetPercent(FMath::Clamp(F.GalacticControl / 100.0f, 0.0f, 1.0f));
		Bar->SetFillColorAndOpacity(F.Color);
		Col->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 5.0f));

		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(F.DoctrineBrief), EERTypeScale::Micro, T->BoneTertiary);
		Brief->SetFont(ERUI::FontRegular(10, 20));
		Brief->SetAutoWrapText(true);
		Col->AddChildToVerticalBox(Brief);

		FactionListBox->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	SectorListBox->ClearChildren();
	for (const FGalacticSector& Sec : Campaign->GetSectors())
	{
		UBorder* RowBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		RowBox->SetBrush(ERUI::RoundedBrush(0.0f, T->ObsidianElevated, 1.0f, T->BoneGhost));
		RowBox->SetBrushColor(FLinearColor::White);
		RowBox->SetPadding(FMargin(12.0f, 6.0f));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RowBox->SetContent(Row);

		UTextBlock* SecName = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("%s  (%s)"), *Sec.Name, *Sec.Code)),
			EERTypeScale::Micro, T->Bone);
		SecName->SetFont(ERUI::FontRegular(11, 40));
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(SecName);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UTextBlock* OwnerTxt = ERUI::MakeText(this,
			FText::FromString(ERGalacticMeta::GetFactionName(Sec.ControllingFaction)),
			EERTypeScale::Micro, ERGalacticMeta::GetFactionColor(Sec.ControllingFaction), true);
		OwnerTxt->SetFont(ERUI::FontBold(11, 80));
		Row->AddChildToHorizontalBox(OwnerTxt);

		SectorListBox->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	if (LastOutcomeBox && LastOutcomeText)
	{
		const FLegionBattleOutcome& Out = Campaign->GetLastOutcome();
		if (Out.bValid && Out.bFromContract)
		{
			LastOutcomeBox->SetVisibility(ESlateVisibility::Visible);
			LastOutcomeText->SetText(FText::FromString(FString::Printf(
				TEXT("上一战况 [%s]: %s\n经费 +%d CR · 我方伤亡 %d · %s控制率 %+.1f%%"),
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
		UOverlay* CardOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		const FLinearColor EmpCol = ERGalacticMeta::GetFactionColor(C.Employer);

		UERTechFrame* Frame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass());
		Frame->Chamfer = FVector4(0.0f, 16.0f, 0.0f, 0.0f);
		Frame->FillColor = T->ObsidianElevated;
		Frame->BorderColor = T->BoneGhost;
		Frame->AccentEdge = EERAccentEdge::Left;
		Frame->AccentColor = EmpCol;
		Frame->AccentThickness = 4.0f;
		Frame->bDrawCornerBrackets = true;
		{
			UOverlaySlot* S = CardOv->AddChildToOverlay(Frame);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			UOverlaySlot* S = CardOv->AddChildToOverlay(Inner);
			S->SetPadding(FMargin(22.0f, 16.0f, 22.0f, 16.0f));
		}

		// 标题行
		UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Title = ERUI::MakeText(this, FText::FromString(C.Title), EERTypeScale::Title, T->Bone);
		Title->SetFont(ERUI::FontBold(19, 40));
		{
			UHorizontalBoxSlot* S = TopRow->AddChildToHorizontalBox(Title);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		FString Stars;
		for (int32 s = 0; s < 5; ++s) { Stars += (s < C.DangerRating) ? TEXT("★") : TEXT("☆"); }
		UTextBlock* DangerTxt = ERUI::MakeText(this, FText::FromString(FString::Printf(TEXT("威胁 %s"), *Stars)),
			EERTypeScale::Caption, T->Accent, true);
		DangerTxt->SetFont(ERUI::FontBold(12, 80));
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
			EERTypeScale::Micro, EmpCol, true);
		Meta->SetFont(ERUI::FontBold(11, 80));
		Inner->AddChildToVerticalBox(Meta)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 6.0f));

		// 简报
		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(C.Briefing), EERTypeScale::Body, T->BoneSecondary);
		Brief->SetFont(ERUI::FontRegular(12, 20));
		Brief->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(Brief)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		// 敌情与报酬 + 出击按钮
		UHorizontalBox* BotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Intel = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("敌情: %d 波次 · %d 名作战单位  |  酬金: +%d CR  |  控制权 %+.1f%%"),
				C.EnemyWaves.Num(), C.GetTotalEnemySoldiers(), C.RewardCredits, C.ControlShift)),
			EERTypeScale::Micro, T->Bone, true);
		Intel->SetFont(ERUI::FontBold(11, 50));
		{
			UHorizontalBoxSlot* S = BotRow->AddChildToHorizontalBox(Intel);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UERGalaIndexButton* DeployBtn = MakeIndexedStyledButton(C.ContractId,
			[this](int32 ContractId)
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					Camp->AcceptAndDeployContract(ContractId, this);
				}
			},
			INVTEXT("接入虹膜 · 出击部署 →"), T->Bone, T->Bone, T->PureInk, 18.0f, 9.0f, 12);
		BotRow->AddChildToHorizontalBox(DeployBtn)->SetVerticalAlignment(VAlign_Center);
		Inner->AddChildToVerticalBox(BotRow);

		ContractListBox->AddChildToVerticalBox(CardOv)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
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
	UTextBlock* RecLabel = ERUI::MakeText(this, INVTEXT("新兵团扩编: "), EERTypeScale::Micro, T->BoneSecondary, true);
	RecLabel->SetFont(ERUI::FontBold(11, 80));
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
		UERGalaIndexButton* Btn = MakeIndexedStyledButton((int32)Cls,
			[this](int32 RawClass)
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->RecruitLegion((ELegionClass)RawClass))
					{
						RefreshAll();
					}
				}
			},
			FText::FromString(FString::Printf(TEXT("+ %s (%d CR)"), *ERGalacticMeta::GetClassName(Cls), Cost)),
			T->ObsidianElevated, bCanAfford ? T->AllyMark : T->BoneGhost,
			bCanAfford ? T->Bone : T->BoneTertiary, 12.0f, 6.0f, 10);
		Btn->SetIsEnabled(bCanAfford);
		RecruitBarBox->AddChildToHorizontalBox(Btn)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}

	RosterListBox->ClearChildren();
	for (const FManagedLegionData& L : Campaign->GetPlayerLegions())
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrush(ERUI::RoundedBrush(0.0f, T->ObsidianElevated, 1.0f, T->BoneGhost));
		Card->SetBrushColor(FLinearColor::White);
		Card->SetPadding(FMargin(18.0f, 12.0f));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Card->SetContent(Row);

		UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InfoCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* NameTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("%s   [%s]   ·   AI 同步 LV.%d"),
				*L.Callsign, *ERGalacticMeta::GetClassCode(L.Archetype), L.SyncLevel)),
			EERTypeScale::Title, T->Bone);
		NameTxt->SetFont(ERUI::FontBold(16, 40));
		InfoCol->AddChildToVerticalBox(NameTxt);

		UTextBlock* RoleTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("定位: %s   |   主动协议: %s   |   战历: %d 场"),
				*ERGalacticMeta::GetClassRoleDesc(L.Archetype),
				*ERGalacticMeta::GetClassSkillName(L.Archetype),
				L.BattlesSurvived)),
			EERTypeScale::Micro, T->BoneSecondary);
		RoleTxt->SetFont(ERUI::FontRegular(11, 20));
		InfoCol->AddChildToVerticalBox(RoleTxt)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		UTextBlock* CountTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("兵员  %d / %d"), L.AliveSoldiers, L.MaxSoldiers)),
			EERTypeScale::Title, L.GetMissingSoldiers() > 0 ? T->Accent : T->AllyMark, true);
		CountTxt->SetFont(ERUI::FontBold(18, 40));
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CountTxt);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 20.0f, 0.0f));
		}

		const int32 ReinforceCost = L.GetFullReinforceCost();
		const bool bNeedReinforce = L.GetMissingSoldiers() > 0 && Campaign->GetCredits() >= L.GetReinforceCostPerSoldier();
		UERGalaIndexButton* ReinfBtn = MakeIndexedStyledButton(L.LegionId,
			[this](int32 Lid)
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->ReinforceLegion(Lid)) { RefreshAll(); }
				}
			},
			FText::FromString(L.GetMissingSoldiers() > 0
				? FString::Printf(TEXT("补充兵员 (%d CR)"), ReinforceCost)
				: TEXT("兵员满编")),
			T->InkPanel, bNeedReinforce ? T->Accent : T->BoneGhost,
			bNeedReinforce ? T->Bone : T->BoneTertiary, 12.0f, 8.0f, 10);
		ReinfBtn->SetIsEnabled(bNeedReinforce);
		Row->AddChildToHorizontalBox(ReinfBtn)->SetVerticalAlignment(VAlign_Center);

		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(10.0f, 1.0f));
		Row->AddChildToHorizontalBox(Gap);

		const int32 SyncCost = L.GetSyncUpgradeCost();
		const bool bCanUpgrade = (L.SyncLevel < 5) && (Campaign->GetCredits() >= SyncCost);
		UERGalaIndexButton* UpgBtn = MakeIndexedStyledButton(L.LegionId,
			[this](int32 Lid)
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->UpgradeLegionSync(Lid)) { RefreshAll(); }
				}
			},
			FText::FromString(L.SyncLevel >= 5
				? TEXT("AI 同步已满级")
				: FString::Printf(TEXT("升级同步 (%d CR)"), SyncCost)),
			T->InkPanel, bCanUpgrade ? T->AllyMark : T->BoneGhost,
			bCanUpgrade ? T->Bone : T->BoneTertiary, 12.0f, 8.0f, 10);
		UpgBtn->SetIsEnabled(bCanUpgrade);
		Row->AddChildToHorizontalBox(UpgBtn)->SetVerticalAlignment(VAlign_Center);

		RosterListBox->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
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
