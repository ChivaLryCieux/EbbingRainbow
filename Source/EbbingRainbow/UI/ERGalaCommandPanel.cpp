// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERGalaCommandPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ERLog.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Experimental/AITacticalCommand/UBattleAICommandService.h"
#include "Experimental/LegionBattle/LegionDeployScreen.h"
#include "GameFramework/PlayerController.h"
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

void UERGalaCommandPanel::OpenPanel(int32 InitialTab, bool bInSettingsOnlyMode)
{
	bIsOpen = true;
	bSettingsOnlyMode = bInSettingsOnlyMode;
	SetVisibility(ESlateVisibility::Visible);
	if (TerminalTabRow)
	{
		TerminalTabRow->SetVisibility(bSettingsOnlyMode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	SetActiveTab(bSettingsOnlyMode ? 3 : InitialTab);
	RefreshAll();
	SyncSettingsInputsFromService();
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
	ActiveTab = bSettingsOnlyMode ? 3 : FMath::Clamp(TabIndex, 0, 3);
	static const FText Titles[] = {
		INVTEXT("军事委托"),
		INVTEXT("兵团整备"),
		INVTEXT("银河局势"),
		INVTEXT("终端设置"),
	};
	if (PanelTitleText)
	{
		PanelTitleText->SetText(Titles[ActiveTab]);
	}

	const UERUITheme* T = ERUI::Theme();
	UWidget* Pages[] = { ContractsPage, RosterPage, SituationPage, SettingsPage };
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bActive = (i == ActiveTab);
		if (Pages[i])
		{
			Pages[i]->SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (NavTabs.IsValidIndex(i))
		{
			if (NavTabs[i].Label)
			{
				ERUI::ApplyResonantText(NavTabs[i].Label, 16, true, 180,
					bActive ? FLinearColor(0.99f, 0.99f, 0.98f, 1.0f) : FLinearColor(0.99f, 0.99f, 0.98f, 0.48f),
					FVector2D(1.5f, 2.0f), bActive ? 0.88f : 0.65f);
			}
			if (NavTabs[i].Indicator)
			{
				NavTabs[i].Indicator->SetColorAndOpacity(bActive ? T->Accent : FLinearColor(0.99f, 0.99f, 0.98f, 0.0f));
				NavTabs[i].Indicator->SetRenderOpacity(bActive ? 0.96f : 0.0f);
			}
		}
	}
}

UERActionButton* UERGalaCommandPanel::MakeResonantButton(const FText& Label, int32 FontSize, bool bArrow,
	bool bLeftAlign, TFunction<void()> OnClick)
{
	UERActionButton* B = UERActionButton::Make(this, Label, FontSize, bArrow);
	B->SetResonantHudStyle(true, bLeftAlign);
	B->SetNativeClick(MoveTemp(OnClick));
	return B;
}

UImage* UERGalaCommandPanel::MakeHairlineH(float Alpha)
{
	UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Line->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 1.0f)));
	Line->SetColorAndOpacity(FLinearColor(0.99f, 0.99f, 0.98f, Alpha));
	Line->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Line;
}

UImage* UERGalaCommandPanel::MakeHairlineV(float Alpha)
{
	UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Line->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 1.0f)));
	Line->SetColorAndOpacity(FLinearColor(0.99f, 0.99f, 0.98f, Alpha));
	Line->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Line;
}

UWidget* UERGalaCommandPanel::MakeSectionKicker(const FText& Text, const FLinearColor& AccentCol)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UOverlay* TickOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UImage* TickShadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TickShadow->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 14.0f)));
	TickShadow->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	TickShadow->SetRenderTranslation(FVector2D(1.5f, 2.0f));
	TickOv->AddChildToOverlay(TickShadow);

	UImage* Tick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Tick->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 14.0f)));
	Tick->SetColorAndOpacity(AccentCol);
	TickOv->AddChildToOverlay(Tick);

	if (UHorizontalBoxSlot* TS = Row->AddChildToHorizontalBox(TickOv))
	{
		TS->SetVerticalAlignment(VAlign_Center);
		TS->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
	}

	UTextBlock* TB = ERUI::MakeText(this, Text, EERTypeScale::Caption, FLinearColor(0.99f, 0.99f, 0.98f, 0.80f), false);
	ERUI::ApplyResonantText(TB, 14, true, 220, FLinearColor(0.99f, 0.99f, 0.98f, 0.80f), FVector2D(1.5f, 2.0f), 0.86f);
	Row->AddChildToHorizontalBox(TB)->SetVerticalAlignment(VAlign_Center);
	return Row;
}

UWidget* UERGalaCommandPanel::MakeFactionSeal(EFactionId Faction, float StampSize)
{
	UBorder* Stamp = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Stamp->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.95f, 0.95f, 0.93f, 0.96f), 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f)));
	Stamp->SetBrushColor(FLinearColor::White);
	Stamp->SetPadding(FMargin(4.0f));

	if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetFactionLogoAssetPath(Faction)))
	{
		if (UImage* Img = ERUI::MakeLogoImage(this, Tex, StampSize))
		{
			Stamp->SetContent(Img);
		}
	}
	return Stamp;
}

void UERGalaCommandPanel::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// 深墨半透明全屏底幕: 与空间站圣殿形成静谧建筑层次
	UImage* Scrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Scrim"));
	Scrim->SetBrush(ERUI::WhiteBrush());
	Scrim->SetColorAndOpacity(FLinearColor(0.004f, 0.005f, 0.008f, 0.84f));
	Scrim->SetVisibility(ESlateVisibility::Visible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Scrim);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// 顶部 2px 通栏虹彩标线
	UERPrismBar* TopPrism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("TopPrism"));
	TopPrism->bVertical = false;
	TopPrism->BarSize = FVector2D(1920.0f, 2.0f);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(TopPrism);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	// 主纵向容器
	UVerticalBox* MainCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainCol"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(MainCol);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(64.0f, 38.0f, 64.0f, 40.0f));
	}

	// ---- 1. 顶栏: 巨幅标题 + 基金会口号 + 经费/周期 + 竖发丝分隔 + 断开连接 ----
	{
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* BrandCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* Slogan = ERUI::MakeText(this,
			INVTEXT("虹膜基金会  ·  我们面向全银河提供军事指挥服务"),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.58f), false);
		ERUI::ApplyResonantText(Slogan, 12, false, 260, FLinearColor(0.99f, 0.99f, 0.98f, 0.58f), FVector2D(1.5f, 2.0f), 0.85f);
		BrandCol->AddChildToVerticalBox(Slogan)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		PanelTitleText = ERUI::MakeText(this, INVTEXT("军事委托"), EERTypeScale::Display, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f));
		ERUI::ApplyResonantText(PanelTitleText, 42, true, 140, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(3.0f, 4.0f), 0.86f);
		BrandCol->AddChildToVerticalBox(PanelTitleText);

		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(BrandCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 右侧: 经费与周期
		UVerticalBox* StatCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CreditsText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), false);
		ERUI::ApplyResonantText(CreditsText, 28, true, 60, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(2.0f, 3.0f), 0.86f);
		CreditsText->SetJustification(ETextJustify::Right);
		StatCol->AddChildToVerticalBox(CreditsText);

		CycleText = ERUI::MakeText(this, INVTEXT("第 01 周期 · 虹膜神经链路在线"), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.58f), false);
		ERUI::ApplyResonantText(CycleText, 13, false, 140, FLinearColor(0.99f, 0.99f, 0.98f, 0.58f), FVector2D(1.5f, 2.0f), 0.85f);
		CycleText->SetJustification(ETextJustify::Right);
		StatCol->AddChildToVerticalBox(CycleText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(StatCol);
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 1px 竖发丝分隔线
		USizeBox* DivBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DivBox->SetWidthOverride(1.0f);
		DivBox->SetHeightOverride(42.0f);
		DivBox->SetContent(MakeHairlineV(0.20f));
		{
			UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(DivBox);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(28.0f, 0.0f));
		}

		// 右上断开连接按钮 (纯白错位阴影文字)
		UERActionButton* CloseBtn = MakeResonantButton(INVTEXT("断开连接"), 20, true, false, [this]() { ClosePanel(); });
		HeaderRow->AddChildToHorizontalBox(CloseBtn)->SetVerticalAlignment(VAlign_Center);

		MainCol->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	// ---- 2. 纯排印导航条 + 1px 建筑通栏线 ----
	{
		UVerticalBox* TabWrap = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TerminalTabRow"));
		TerminalTabRow = TabWrap;

		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		static const FText TabLabels[4] = {
			INVTEXT("接取委托"), INVTEXT("兵团整备"), INVTEXT("银河局势"), INVTEXT("终端设置")
		};
		const FSlateBrush Tr = ERUI::TransparentBrush();
		FButtonStyle TabBtnStyle;
		TabBtnStyle.SetNormal(Tr); TabBtnStyle.SetHovered(Tr); TabBtnStyle.SetPressed(Tr); TabBtnStyle.SetDisabled(Tr);
		TabBtnStyle.SetNormalPadding(FMargin(0.0f, 6.0f));
		TabBtnStyle.SetPressedPadding(FMargin(0.0f, 6.0f));

		NavTabs.Reset();
		for (int32 Tab = 0; Tab < 4; ++Tab)
		{
			UButton* TabBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			TabBtn->SetStyle(TabBtnStyle);
			TabBtn->SetCursor(EMouseCursor::Hand);

			UVerticalBox* TabCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			TabCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			TabBtn->SetContent(TabCol);

			UTextBlock* TabLbl = ERUI::MakeText(this, TabLabels[Tab], EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), false);
			ERUI::ApplyResonantText(TabLbl, 16, true, 180, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), FVector2D(1.5f, 2.0f), 0.70f);
			TabCol->AddChildToVerticalBox(TabLbl)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

			UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Bar->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 2.0f)));
			Bar->SetColorAndOpacity(T->Accent);
			Bar->SetRenderOpacity(0.0f);
			TabCol->AddChildToVerticalBox(Bar);

			switch (Tab)
			{
			case 0: TabBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnTabClicked0); break;
			case 1: TabBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnTabClicked1); break;
			case 2: TabBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnTabClicked2); break;
			default: TabBtn->OnClicked.AddDynamic(this, &UERGalaCommandPanel::OnTabClicked3); break;
			}

			TabRow->AddChildToHorizontalBox(TabBtn)->SetPadding(FMargin(0.0f, 0.0f, 36.0f, 0.0f));

			FResonantTabItem Item;
			Item.Button = TabBtn;
			Item.Label = TabLbl;
			Item.Indicator = Bar;
			NavTabs.Add(MoveTemp(Item));
		}

		TabWrap->AddChildToVerticalBox(TabRow);
		TabWrap->AddChildToVerticalBox(MakeHairlineH(0.18f));
		MainCol->AddChildToVerticalBox(TabWrap)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	// 战役存档状态提示 (无异常时折叠)
	CampaignSaveNotice = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, FLinearColor(1.0f, 0.45f, 0.30f, 0.95f), false);
	ERUI::ApplyResonantText(CampaignSaveNotice, 13, false, 80, FLinearColor(1.0f, 0.45f, 0.30f, 0.95f), FVector2D(1.5f, 2.0f), 0.85f);
	CampaignSaveNotice->SetAutoWrapText(true);
	CampaignSaveNotice->SetVisibility(ESlateVisibility::Collapsed);
	MainCol->AddChildToVerticalBox(CampaignSaveNotice)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// =========================================================================
	// 内容区: Overlay 页签
	// =========================================================================
	UOverlay* ContentOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ContentOverlay"));
	{
		UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(ContentOv);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// =========================================================================
	// TAB 0: 军事委托 (三栏建筑竖分栏, 无白底卡片)
	// =========================================================================
	{
		UVerticalBox* Page0 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContractsPage"));
		ContractsPage = Page0;
		{
			UOverlaySlot* S = ContentOv->AddChildToOverlay(Page0);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UHorizontalBox* ContractHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* SubHint = ERUI::MakeText(this, INVTEXT("当前周期加密频道  ·  择一委托接入前线神经链路"),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.54f), false);
		ERUI::ApplyResonantText(SubHint, 13, false, 100, FLinearColor(0.99f, 0.99f, 0.98f, 0.54f), FVector2D(1.5f, 2.0f), 0.85f);
		{
			UHorizontalBoxSlot* S = ContractHeader->AddChildToHorizontalBox(SubHint);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		RefreshContractsButton = MakeResonantButton(INVTEXT("刷新加密频道"), 15, false, false, [this]() { OnRefreshContractsClicked(); });
		ContractHeader->AddChildToHorizontalBox(RefreshContractsButton)->SetVerticalAlignment(VAlign_Center);
		Page0->AddChildToVerticalBox(ContractHeader)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		ContractListBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		{
			UVerticalBoxSlot* S = Page0->AddChildToVerticalBox(ContractListBox);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// =========================================================================
	// TAB 2: 银河局势 (左 42% 暗底控制率环图 + 战报 | 右 58% 势力简报 + 星区归属)
	// =========================================================================
	{
		UHorizontalBox* Page2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SituationPage"));
		SituationPage = Page2;
		Page2->SetVisibility(ESlateVisibility::Collapsed);
		{
			UOverlaySlot* S = ContentOv->AddChildToOverlay(Page2);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		// 左列: 控制率环图 + 上一战况
		UVerticalBox* LeftCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			UHorizontalBoxSlot* S = Page2->AddChildToHorizontalBox(LeftCol);
			FSlateChildSize LeftSz(ESlateSizeRule::Fill);
			LeftSz.Value = 0.42f;
			S->SetSize(LeftSz);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		LeftCol->AddChildToVerticalBox(MakeSectionKicker(INVTEXT("银河总体控制率"), T->Bone))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		UVerticalBox* ChartWrap = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ControlShareCard = ChartWrap;
		ControlShareCard->SetVisibility(ESlateVisibility::Collapsed);

		ControlSharePie = WidgetTree->ConstructWidget<UERControlSharePie>(UERControlSharePie::StaticClass());
		ControlSharePie->SetLightOnDark(true);
		ControlSharePie->SetVisibility(ESlateVisibility::HitTestInvisible);
		{
			UVerticalBoxSlot* PS = ChartWrap->AddChildToVerticalBox(ControlSharePie);
			PS->SetHorizontalAlignment(HAlign_Center);
		}

		UTextBlock* ChartNote = ERUI::MakeText(this, INVTEXT("扇区面积对应控制率，不按重点星区数量计算"),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), false);
		ERUI::ApplyResonantText(ChartNote, 12, false, 60, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), FVector2D(1.5f, 2.0f), 0.85f);
		ChartNote->SetJustification(ETextJustify::Center);
		ChartNote->SetAutoWrapText(true);
		ChartWrap->AddChildToVerticalBox(ChartNote)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		LeftCol->AddChildToVerticalBox(ChartWrap)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));

		// 上一战况 (3px 虹彩竖条 + 纯白阴影摘要, 无白底框, 默认折叠)
		LastOutcomeBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LastOutcomeBox->SetBrush(ERUI::TransparentBrush());
		LastOutcomeBox->SetPadding(FMargin(0.0f));
		LastOutcomeBox->SetVisibility(ESlateVisibility::Collapsed);
		{
			UHorizontalBox* OutRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			LastOutcomeBox->SetContent(OutRow);

			UERPrismBar* OutPrism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass());
			OutPrism->bVertical = true;
			OutPrism->BarSize = FVector2D(3.0f, 44.0f);
			OutRow->AddChildToHorizontalBox(OutPrism)->SetPadding(FMargin(0.0f, 2.0f, 14.0f, 2.0f));

			LastOutcomeText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.94f), false);
			ERUI::ApplyResonantText(LastOutcomeText, 15, true, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.94f), FVector2D(1.5f, 2.0f), 0.86f);
			LastOutcomeText->SetAutoWrapText(true);
			UHorizontalBoxSlot* TS = OutRow->AddChildToHorizontalBox(LastOutcomeText);
			TS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TS->SetVerticalAlignment(VAlign_Center);
		}
		LeftCol->AddChildToVerticalBox(LastOutcomeBox);

		// 中央 1px 竖发丝分割
		{
			UHorizontalBoxSlot* LS = Page2->AddChildToHorizontalBox(MakeHairlineV(0.14f));
			LS->SetVerticalAlignment(VAlign_Fill);
			LS->SetPadding(FMargin(32.0f, 0.0f));
		}

		// 右列: 势力简报 + 重点星区归属
		UScrollBox* RightScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightScroll->AddChild(RightCol);
		{
			UHorizontalBoxSlot* S = Page2->AddChildToHorizontalBox(RightScroll);
			FSlateChildSize RightSz(ESlateSizeRule::Fill);
			RightSz.Value = 0.58f;
			S->SetSize(RightSz);
		}

		RightCol->AddChildToVerticalBox(MakeSectionKicker(INVTEXT("势力简报"), T->Bone))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		FactionListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightCol->AddChildToVerticalBox(FactionListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));

		RightCol->AddChildToVerticalBox(MakeSectionKicker(INVTEXT("重点星区归属"), T->Accent))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		SectorListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightCol->AddChildToVerticalBox(SectorListBox);
	}

	// =========================================================================
	// TAB 3: 终端设置 (限定 820px 宽的极简暗曜石编辑排版)
	// =========================================================================
	{
		UVerticalBox* Page3 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPage"));
		SettingsPage = Page3;
		Page3->SetVisibility(ESlateVisibility::Collapsed);
		{
			UOverlaySlot* S = ContentOv->AddChildToOverlay(Page3);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* FormWidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FormWidthBox->SetWidthOverride(820.0f);
		{
			UVerticalBoxSlot* FS = Page3->AddChildToVerticalBox(FormWidthBox);
			FS->SetHorizontalAlignment(HAlign_Left);
			FS->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		FormWidthBox->SetContent(Inner);

		CampaignSaveRetryButton = MakeResonantButton(INVTEXT("重试战役存档"), 16, false, true, [this]() {
			if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
			{
				Camp->RetryCampaignSave();
				RefreshAll();
			}
		});
		CampaignSaveRetryButton->SetVisibility(ESlateVisibility::Collapsed);
		Inner->AddChildToVerticalBox(CampaignSaveRetryButton)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		Inner->AddChildToVerticalBox(MakeSectionKicker(INVTEXT("AI 战术指挥接入"), T->Accent))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		UTextBlock* AIDesc = ERUI::MakeText(this,
			INVTEXT("配置大模型接口后，战场上可通过右上「接入 AI」以自然语言指挥全军。接口地址同时决定通信协议：以 /messages 结尾走 Anthropic 协议，其余走 OpenAI 兼容协议。"),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.62f), false);
		ERUI::ApplyResonantText(AIDesc, 13, false, 30, FLinearColor(0.99f, 0.99f, 0.98f, 0.62f), FVector2D(1.5f, 2.0f), 0.85f);
		AIDesc->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(AIDesc)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));

		auto AddInputRow = [this, Inner](const FText& Label, TObjectPtr<UEditableTextBox>& OutInput, const FText& Hint, bool bPassword)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelBox->SetWidthOverride(104.0f);
			UTextBlock* Lbl = ERUI::MakeText(this, Label, EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.82f), false);
			ERUI::ApplyResonantText(Lbl, 14, true, 80, FLinearColor(0.99f, 0.99f, 0.98f, 0.82f), FVector2D(1.5f, 2.0f), 0.86f);
			LabelBox->AddChild(Lbl);
			Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

			UEditableTextBox* InputWidget = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			StyleDarkInputBox(InputWidget, Hint, bPassword);
			OutInput = InputWidget;
			{
				UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InputWidget);
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			Inner->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		};

		AddInputRow(INVTEXT("接口地址"), SettingsEndpointInput, INVTEXT("https://api.deepseek.com/v1/chat/completions"), false);
		AddInputRow(INVTEXT("模型名称"), SettingsModelInput,    INVTEXT("deepseek-chat"), false);
		AddInputRow(INVTEXT("API Key"), SettingsApiKeyInput,   INVTEXT("sk-... (仅保存于本地 Saved/ER_AIConfig.json)"), true);

		UTextBlock* KeyNote = ERUI::MakeText(this,
			INVTEXT("密钥仅保存在本机 Saved/ER_AIConfig.json，不上传任何其他服务器，也不进入版本库。"),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), false);
		ERUI::ApplyResonantText(KeyNote, 12, false, 30, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), FVector2D(1.5f, 2.0f), 0.85f);
		KeyNote->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(KeyNote)->SetPadding(FMargin(104.0f, 0.0f, 0.0f, 18.0f));

		{
			UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UERActionButton* SaveBtn = MakeResonantButton(INVTEXT("保存配置"), 18, true, true,
				[this]() { OnSaveAIConfigClicked(); });
			SaveRow->AddChildToHorizontalBox(SaveBtn)->SetPadding(FMargin(104.0f, 0.0f, 20.0f, 0.0f));

			SettingsStatusText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.65f), false);
			ERUI::ApplyResonantText(SettingsStatusText, 14, false, 30, FLinearColor(0.99f, 0.99f, 0.98f, 0.65f), FVector2D(1.5f, 2.0f), 0.85f);
			SettingsStatusText->SetAutoWrapText(true);
			{
				UHorizontalBoxSlot* S = SaveRow->AddChildToHorizontalBox(SettingsStatusText);
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			Inner->AddChildToVerticalBox(SaveRow);
		}

		Inner->AddChildToVerticalBox(MakeHairlineH(0.14f))->SetPadding(FMargin(0.0f, 26.0f, 0.0f, 16.0f));

		UTextBlock* Placeholder = ERUI::MakeText(this,
			INVTEXT("画面、音频与指挥终端偏好将在后续周期开放。"),
			EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), false);
		ERUI::ApplyResonantText(Placeholder, 13, false, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), FVector2D(1.5f, 2.0f), 0.85f);
		Inner->AddChildToVerticalBox(Placeholder);
	}

	// =========================================================================
	// TAB 1: 兵团整备与扩编 (极简账本式列表 + 1px 横发丝分割)
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
		RosterSummaryText = ERUI::MakeText(this, INVTEXT("虹膜链接兵团"), EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.92f), false);
		ERUI::ApplyResonantText(RosterSummaryText, 18, true, 80, FLinearColor(0.99f, 0.99f, 0.98f, 0.92f), FVector2D(2.0f, 2.5f), 0.86f);
		{
			UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(RosterSummaryText);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		ReinforceAllButton = MakeResonantButton(INVTEXT("一键满编补员"), 17, false, false, [this]() { OnReinforceAllClicked(); });
		TopBar->AddChildToHorizontalBox(ReinforceAllButton)->SetVerticalAlignment(VAlign_Center);
		Page1->AddChildToVerticalBox(TopBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		RecruitBarBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Page1->AddChildToVerticalBox(RecruitBarBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		Page1->AddChildToVerticalBox(MakeHairlineH(0.16f));

		UScrollBox* RosterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		{
			UVerticalBoxSlot* S = Page1->AddChildToVerticalBox(RosterScroll);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		RosterListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RosterScroll->AddChild(RosterListBox);
	}
}

void UERGalaCommandPanel::RefreshCampaignSaveStatus()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (!Campaign)
	{
		if (CampaignSaveNotice) { CampaignSaveNotice->SetVisibility(ESlateVisibility::Collapsed); }
		if (CampaignSaveRetryButton) { CampaignSaveRetryButton->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	const FText Notice = Campaign->GetSaveNotice();
	if (CampaignSaveNotice)
	{
		CampaignSaveNotice->SetText(Notice);
		CampaignSaveNotice->SetVisibility(Notice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CampaignSaveRetryButton)
	{
		const EGalacticCampaignSaveStatus Status = Campaign->GetSaveStatus();
		const bool bShowRetry = Status == EGalacticCampaignSaveStatus::WriteFailed || Status == EGalacticCampaignSaveStatus::LoadFailed;
		CampaignSaveRetryButton->SetVisibility(bShowRetry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UERGalaCommandPanel::RefreshAll()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	if (!Campaign) { return; }

	RefreshCampaignSaveStatus();
	const bool bCampaignAvailable = Campaign->IsCampaignAvailable();
	if (RefreshContractsButton) { RefreshContractsButton->SetButtonEnabled(bCampaignAvailable); }
	if (ReinforceAllButton) { ReinforceAllButton->SetButtonEnabled(bCampaignAvailable); }
	if (!bCampaignAvailable)
	{
		if (CreditsText) { CreditsText->SetText(INVTEXT("经费 —")); }
		if (CycleText) { CycleText->SetText(INVTEXT("周期 —")); }
		if (FactionListBox) { FactionListBox->ClearChildren(); }
		if (SectorListBox) { SectorListBox->ClearChildren(); }
		if (ContractListBox) { ContractListBox->ClearChildren(); }
		if (RosterListBox) { RosterListBox->ClearChildren(); }
		if (RecruitBarBox) { RecruitBarBox->ClearChildren(); }
		if (RosterSummaryText) { RosterSummaryText->SetText(INVTEXT("虹膜链接兵团")); }
		if (LastOutcomeBox) { LastOutcomeBox->SetVisibility(ESlateVisibility::Collapsed); }
		if (ControlShareCard) { ControlShareCard->SetVisibility(ESlateVisibility::Collapsed); }
		if (ControlSharePie) { ControlSharePie->SetSlices({}); }
		return;
	}

	if (CreditsText)
	{
		CreditsText->SetText(FText::FromString(FString::Printf(TEXT("经费 %d 银河币"), Campaign->GetCredits())));
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

	const TArray<FFactionStatus>& Factions = Campaign->GetFactions();
	// 暗底环图采用高对比单色灰阶层次
	static const FColor DarkModeGrayRanks[] = { FColor(235, 235, 230), FColor(145, 145, 142), FColor(68, 68, 72) };
	TArray<FERControlShareSlice> Slices;
	Slices.Reserve(Factions.Num());
	for (const FFactionStatus& F : Factions)
	{
		const float Share = FMath::IsFinite(F.GalacticControl) ? FMath::Max(0.0f, F.GalacticControl) : 0.0f;
		int32 Rank = 0;
		for (const FFactionStatus& Other : Factions)
		{
			if (FMath::IsFinite(Other.GalacticControl) && Other.GalacticControl > Share + 0.001f)
			{
				++Rank;
			}
		}
		Slices.Add({ Share, FLinearColor::FromSRGBColor(DarkModeGrayRanks[FMath::Min(Rank, static_cast<int32>(UE_ARRAY_COUNT(DarkModeGrayRanks)) - 1)]) });
	}

	FactionListBox->ClearChildren();
	for (int32 Index = 0; Index < Factions.Num(); ++Index)
	{
		const FFactionStatus& F = Factions[Index];
		const FLinearColor SliceFill = Slices[Index].Fill;

		UVerticalBox* EntryCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Key = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Key->SetBrush(ERUI::RoundedBrush(0.0f, SliceFill, 0.0f, FLinearColor::Transparent));
		Key->SetPadding(FMargin(6.0f, 3.0f));
		UTextBlock* KeyText = ERUI::MakeText(this, FText::FromString(FString::Printf(TEXT("%02d"), Index + 1)),
			EERTypeScale::Micro, SliceFill.GetLuminance() > 0.24f ? ERUI::InkText() : FLinearColor::White, false);
		KeyText->SetFont(ERUI::FontBold(12, 40));
		KeyText->SetJustification(ETextJustify::Center);
		Key->SetContent(KeyText);
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Key);
			S->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 紧凑骨白印章托住黑墨势力徽标
		{
			UWidget* Seal = MakeFactionSeal(F.Faction, 26.0f);
			UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(Seal);
			LS->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
			LS->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* NameTxt = ERUI::MakeText(this, FText::FromString(F.DisplayName), EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), false);
		ERUI::ApplyResonantText(NameTxt, 21, true, 60, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(2.0f, 2.5f), 0.86f);
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameTxt);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* PctTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("控制率 %.1f%%"), F.GalacticControl)),
			EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), false);
		ERUI::ApplyResonantText(PctTxt, 19, true, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), FVector2D(1.5f, 2.0f), 0.86f);
		Row->AddChildToHorizontalBox(PctTxt)->SetVerticalAlignment(VAlign_Center);

		UTextBlock* RepTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("  ·  声望 %+d"), F.Reputation)),
			EERTypeScale::Caption, FLinearColor(0.99f, 0.99f, 0.98f, 0.66f), false);
		ERUI::ApplyResonantText(RepTxt, 14, false, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.66f), FVector2D(1.5f, 2.0f), 0.85f);
		Row->AddChildToHorizontalBox(RepTxt)->SetVerticalAlignment(VAlign_Center);

		EntryCol->AddChildToVerticalBox(Row);

		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(F.DoctrineBrief), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.58f), false);
		ERUI::ApplyResonantText(Brief, 13, false, 20, FLinearColor(0.99f, 0.99f, 0.98f, 0.58f), FVector2D(1.5f, 2.0f), 0.85f);
		Brief->SetAutoWrapText(true);
		EntryCol->AddChildToVerticalBox(Brief)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

		FactionListBox->AddChildToVerticalBox(EntryCol)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 10.0f));
		if (Index + 1 < Factions.Num())
		{
			FactionListBox->AddChildToVerticalBox(MakeHairlineH(0.12f));
		}
	}

	if (ControlShareCard)
	{
		ControlShareCard->SetVisibility(Slices.ContainsByPredicate([](const FERControlShareSlice& Slice) { return Slice.Share > 0.0f; })
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ControlSharePie)
	{
		ControlSharePie->SetSlices(MoveTemp(Slices));
	}

	SectorListBox->ClearChildren();
	const TArray<FGalacticSector>& Sectors = Campaign->GetSectors();
	for (int32 s = 0; s < Sectors.Num(); ++s)
	{
		const FGalacticSector& Sec = Sectors[s];
		const FLinearColor OwnerCol = ERGalacticMeta::GetFactionColor(Sec.ControllingFaction);

		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UImage* Tick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Tick->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 16.0f)));
		Tick->SetColorAndOpacity(OwnerCol);
		H->AddChildToHorizontalBox(Tick)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));

		UTextBlock* SecName = ERUI::MakeText(this, FText::FromString(Sec.Name), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.94f), false);
		ERUI::ApplyResonantText(SecName, 16, true, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.94f), FVector2D(1.5f, 2.0f), 0.85f);
		{
			UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(SecName);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* OwnerTxt = ERUI::MakeText(this,
			FText::FromString(ERGalacticMeta::GetFactionName(Sec.ControllingFaction)),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.78f), false);
		ERUI::ApplyResonantText(OwnerTxt, 14, true, 60, FLinearColor(0.99f, 0.99f, 0.98f, 0.78f), FVector2D(1.5f, 2.0f), 0.85f);
		H->AddChildToHorizontalBox(OwnerTxt)->SetVerticalAlignment(VAlign_Center);

		SectorListBox->AddChildToVerticalBox(H)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 8.0f));
		if (s + 1 < Sectors.Num())
		{
			SectorListBox->AddChildToVerticalBox(MakeHairlineH(0.10f));
		}
	}

	if (LastOutcomeBox && LastOutcomeText)
	{
		const FLegionBattleOutcome& Out = Campaign->GetLastOutcome();
		if (Out.bValid && Out.bFromContract)
		{
			LastOutcomeBox->SetVisibility(ESlateVisibility::Visible);
			LastOutcomeText->SetText(FText::FromString(FString::Printf(
				TEXT("上一战况 [%s]：%s\n经费 +%d 银河币  ·  我方伤亡 %d  ·  %s控制率 %+.1f%%"),
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
	const TArray<FGalacticContract>& Contracts = Campaign->GetAvailableContracts();
	for (int32 Idx = 0; Idx < Contracts.Num(); ++Idx)
	{
		const FGalacticContract& C = Contracts[Idx];
		if (Idx > 0)
		{
			UHorizontalBoxSlot* SepSlot = ContractListBox->AddChildToHorizontalBox(MakeHairlineV(0.14f));
			SepSlot->SetVerticalAlignment(VAlign_Fill);
			SepSlot->SetPadding(FMargin(28.0f, 4.0f));
		}

		const FLinearColor EmpCol = ERGalacticMeta::GetFactionColor(C.Employer);
		UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			UHorizontalBoxSlot* CS = ContractListBox->AddChildToHorizontalBox(Inner);
			CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CS->SetVerticalAlignment(VAlign_Fill);
		}

		const FGalacticSector* Sec = Campaign->FindSector(C.TargetSectorId);
		const FString SecName = Sec ? Sec->Name : TEXT("未知星区");

		// 1. 顶部 3px 雇主阵营色横条
		UImage* TopBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		TopBar->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 3.0f)));
		TopBar->SetColorAndOpacity(EmpCol);
		Inner->AddChildToVerticalBox(TopBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		// 2. 序号 (01/02/03) + 右侧交战双方印章徽标
		{
			UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* NumTxt = ERUI::MakeText(this, FText::FromString(FString::Printf(TEXT("%02d"), Idx + 1)),
				EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 0.28f), true);
			ERUI::ApplyResonantText(NumTxt, 34, true, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.28f), FVector2D(2.0f, 3.0f), 0.70f);
			{
				UHorizontalBoxSlot* S = TopRow->AddChildToHorizontalBox(NumTxt);
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}

			TopRow->AddChildToHorizontalBox(MakeFactionSeal(C.Employer, 38.0f))->SetVerticalAlignment(VAlign_Center);

			UTextBlock* VsTxt = ERUI::MakeText(this, INVTEXT("×"), EERTypeScale::Caption, FLinearColor(0.99f, 0.99f, 0.98f, 0.50f), true);
			ERUI::ApplyResonantText(VsTxt, 14, true, 0, FLinearColor(0.99f, 0.99f, 0.98f, 0.50f), FVector2D(1.5f, 2.0f), 0.85f);
			{
				UHorizontalBoxSlot* VS = TopRow->AddChildToHorizontalBox(VsTxt);
				VS->SetVerticalAlignment(VAlign_Center);
				VS->SetPadding(FMargin(10.0f, 0.0f));
			}

			TopRow->AddChildToHorizontalBox(MakeFactionSeal(C.EnemyFaction, 38.0f))->SetVerticalAlignment(VAlign_Center);

			Inner->AddChildToVerticalBox(TopRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
		}

		// 3. 任务标题 (大字纯白 + 错位黑影)
		UTextBlock* Title = ERUI::MakeText(this, FText::FromString(C.Title), EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), false);
		ERUI::ApplyResonantText(Title, 25, true, 50, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(2.5f, 3.5f), 0.86f);
		Title->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		// 4. 任务地点 + 难度星级同行
		{
			UHorizontalBox* LocStarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* LocLabel = ERUI::MakeText(this, INVTEXT("任务地点 "), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), false);
			ERUI::ApplyResonantText(LocLabel, 12, false, 60, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), FVector2D(1.5f, 2.0f), 0.85f);
			LocStarRow->AddChildToHorizontalBox(LocLabel)->SetVerticalAlignment(VAlign_Center);

			UTextBlock* LocValue = ERUI::MakeText(this, FText::FromString(SecName), EERTypeScale::Caption, FLinearColor(0.99f, 0.99f, 0.98f, 0.94f), false);
			ERUI::ApplyResonantText(LocValue, 15, true, 50, FLinearColor(0.99f, 0.99f, 0.98f, 0.94f), FVector2D(1.5f, 2.0f), 0.85f);
			{
				UHorizontalBoxSlot* S = LocStarRow->AddChildToHorizontalBox(LocValue);
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}

			FString Stars;
			const int32 Rating = FMath::Clamp(C.DangerRating, 1, 5);
			for (int32 s = 0; s < 5; ++s) { Stars += (s < Rating) ? TEXT("★") : TEXT("☆"); }
			UTextBlock* DangerTxt = ERUI::MakeText(this, FText::FromString(Stars), EERTypeScale::Caption, T->Accent, false);
			ERUI::ApplyResonantText(DangerTxt, 15, true, 40, T->Accent, FVector2D(1.5f, 2.0f), 0.86f);
			LocStarRow->AddChildToHorizontalBox(DangerTxt)->SetVerticalAlignment(VAlign_Center);

			Inner->AddChildToVerticalBox(LocStarRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		// 5. 雇主 · 敌对 · 地形倾向
		UTextBlock* Meta = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("雇主 %s  ·  敌对 %s  ·  %s"),
				*ERGalacticMeta::GetFactionName(C.Employer),
				*ERGalacticMeta::GetFactionName(C.EnemyFaction),
				*ERGalacticMeta::GetTacticalProfileName(C.TacticalProfile))),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.56f), false);
		ERUI::ApplyResonantText(Meta, 13, false, 30, FLinearColor(0.99f, 0.99f, 0.98f, 0.56f), FVector2D(1.5f, 2.0f), 0.85f);
		Meta->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(Meta)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		Inner->AddChildToVerticalBox(MakeHairlineH(0.12f))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// 6. 简报正文 (占据弹性高)
		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(C.Briefing), EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.78f), false);
		ERUI::ApplyResonantText(Brief, 14, false, 20, FLinearColor(0.99f, 0.99f, 0.98f, 0.78f), FVector2D(1.5f, 2.0f), 0.85f);
		Brief->SetAutoWrapText(true);
		{
			UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(Brief);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
		}

		Inner->AddChildToVerticalBox(MakeHairlineH(0.14f))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// 7. 任务报酬 + 控制权收益同行
		{
			UHorizontalBox* PayRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* RewardVal = ERUI::MakeText(this,
				FText::FromString(FString::Printf(TEXT("+%d"), C.RewardCredits)),
				EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), true);
			ERUI::ApplyResonantText(RewardVal, 30, true, 20, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(2.0f, 3.0f), 0.86f);
			PayRow->AddChildToHorizontalBox(RewardVal)->SetVerticalAlignment(VAlign_Bottom);

			UTextBlock* UnitLbl = ERUI::MakeText(this, INVTEXT(" 银河币"), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.62f), false);
			ERUI::ApplyResonantText(UnitLbl, 13, false, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.62f), FVector2D(1.5f, 2.0f), 0.85f);
			{
				UHorizontalBoxSlot* US = PayRow->AddChildToHorizontalBox(UnitLbl);
				US->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				US->SetVerticalAlignment(VAlign_Bottom);
				US->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			}

			UTextBlock* ControlTxt = ERUI::MakeText(this,
				FText::FromString(FString::Printf(TEXT("控制权 %+.1f%%"), C.ControlShift)),
				EERTypeScale::Caption, T->Accent, false);
			ERUI::ApplyResonantText(ControlTxt, 15, true, 40, T->Accent, FVector2D(1.5f, 2.0f), 0.86f);
			PayRow->AddChildToHorizontalBox(ControlTxt)->SetVerticalAlignment(VAlign_Bottom);

			Inner->AddChildToVerticalBox(PayRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		UTextBlock* Intel = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("敌情 %d 波次 · %d 名作战单位"),
				C.EnemyWaves.Num(), C.GetTotalEnemySoldiers())),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.56f), false);
		ERUI::ApplyResonantText(Intel, 13, false, 30, FLinearColor(0.99f, 0.99f, 0.98f, 0.56f), FVector2D(1.5f, 2.0f), 0.85f);
		Inner->AddChildToVerticalBox(Intel)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		// 8. 接取按钮 (《控制:共振》式纯白阴影文字)
		const int32 ContractId = C.ContractId;
		UERActionButton* DeployBtn = MakeResonantButton(INVTEXT("接取委托 · 出击部署"), 19, true, true,
			[this, ContractId]()
			{
				UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this);
				if (!Camp || bDeployInTransit) { return; }
				bDeployInTransit = true;

				// 先盖上静态「神经接驳」过渡屏, 随后直接调 OpenLevel —
				// SetClientTravel 本就延迟到下一帧引擎 tick 才处理 (UEngine::TickWorldTravel),
				// 本帧过渡屏照常光栅化, 关卡加载卡顿发生在接驳屏上,
				// 与 Lvl_Legion 内的交互式接驳屏视觉连续。
				// 切勿经 FTSTicker 转一手: 核心 ticker 回调脱离 PIE 世界上下文 (GPlayInEditorID==-1)。
				ULegionDeployScreen* Screen = nullptr;
				if (UWorld* World = GetWorld())
				{
					if (APlayerController* PC = World->GetFirstPlayerController())
					{
						Screen = CreateWidget<ULegionDeployScreen>(PC, ULegionDeployScreen::StaticClass());
						if (Screen)
						{
							Screen->SetupTransit();
							Screen->AddToViewport(999);
						}
					}
				}

				if (!Camp->AcceptAndDeployContract(ContractId, this))
				{
					bDeployInTransit = false;
					if (Screen) { Screen->RemoveFromParent(); }
					RefreshAll();
				}
			});
		{
			UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(DeployBtn);
			S->SetHorizontalAlignment(HAlign_Left);
		}
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
			TEXT("现役虹膜链接兵团 %d / %d 支   ·   总可用兵力 %d / %d"),
			Campaign->GetPlayerLegions().Num(), UGalacticCampaignSubsystem::MaxPlayerLegions, TotalAlive, TotalMax)));
	}

	RecruitBarBox->ClearChildren();
	UTextBlock* RecLabel = ERUI::MakeText(this, INVTEXT("新兵团扩编"), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.55f), false);
	ERUI::ApplyResonantText(RecLabel, 13, true, 80, FLinearColor(0.99f, 0.99f, 0.98f, 0.55f), FVector2D(1.5f, 2.0f), 0.85f);
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
		UERActionButton* Btn = MakeResonantButton(
			FText::FromString(FString::Printf(TEXT("招募%s · %d 银河币"), *ERGalacticMeta::GetClassName(Cls), Cost)),
			14, false, true,
			[this, Cls]()
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->RecruitLegion(Cls)) { RefreshAll(); }
				}
			});
		Btn->SetButtonEnabled(bCanAfford);
		RecruitBarBox->AddChildToHorizontalBox(Btn)->SetPadding(FMargin(20.0f, 0.0f, 0.0f, 0.0f));
	}

	RosterListBox->ClearChildren();
	const TArray<FManagedLegionData>& Legions = Campaign->GetPlayerLegions();
	for (int32 i = 0; i < Legions.Num(); ++i)
	{
		const FManagedLegionData& L = Legions[i];
		const bool bDamaged = L.GetMissingSoldiers() > 0;
		const FLinearColor BarCol = bDamaged ? T->Accent : T->AllyMark;

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// 1. 左侧 3px 状态竖条
		UImage* StatusBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		StatusBar->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 44.0f)));
		StatusBar->SetColorAndOpacity(BarCol);
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(StatusBar);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
		}

		// 2. 部队占位 Logo (1px 暗影框)
		{
			UBorder* LogoBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			LogoBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.010f, 0.014f, 0.85f), 1.0f, FLinearColor(0.99f, 0.99f, 0.98f, 0.16f)));
			LogoBox->SetBrushColor(FLinearColor::White);
			LogoBox->SetPadding(FMargin(2.0f));

			UImage* LogoImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush IconBrush;
			IconBrush.ImageSize = FVector2D(44.0f, 44.0f);
			IconBrush.DrawAs = ESlateBrushDrawType::Image;
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetClassLogoAssetPath(L.Archetype, true)))
			{
				IconBrush.SetResourceObject(Tex);
			}
			LogoImg->SetBrush(IconBrush);
			LogoBox->SetContent(LogoImg);

			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LogoBox))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
			}
		}

		// 3. 番号 / 兵种 / 同步等级 / 定位
		UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		InfoCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InfoCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		{
			UHorizontalBox* TitleLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* NameTxt = ERUI::MakeText(this, FText::FromString(L.Callsign), EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), false);
			ERUI::ApplyResonantText(NameTxt, 23, true, 60, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(2.0f, 2.5f), 0.86f);
			TitleLine->AddChildToHorizontalBox(NameTxt)->SetVerticalAlignment(VAlign_Bottom);

			UTextBlock* MetaTxt = ERUI::MakeText(this,
				FText::FromString(FString::Printf(TEXT("   ·   %s   ·   虹膜同步 %d 级"), *ERGalacticMeta::GetClassName(L.Archetype), L.SyncLevel)),
				EERTypeScale::Caption, FLinearColor(0.99f, 0.99f, 0.98f, 0.68f), false);
			ERUI::ApplyResonantText(MetaTxt, 14, false, 40, FLinearColor(0.99f, 0.99f, 0.98f, 0.68f), FVector2D(1.5f, 2.0f), 0.85f);
			{
				UHorizontalBoxSlot* MS = TitleLine->AddChildToHorizontalBox(MetaTxt);
				MS->SetVerticalAlignment(VAlign_Bottom);
				MS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
			}
			InfoCol->AddChildToVerticalBox(TitleLine);
		}

		UTextBlock* RoleTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("定位：%s   |   战历：%d 场"),
				*ERGalacticMeta::GetClassRoleDesc(L.Archetype), L.BattlesSurvived)),
			EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.54f), false);
		ERUI::ApplyResonantText(RoleTxt, 13, false, 20, FLinearColor(0.99f, 0.99f, 0.98f, 0.54f), FVector2D(1.5f, 2.0f), 0.85f);
		InfoCol->AddChildToVerticalBox(RoleTxt)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));

		// 4. 巨型兵员读数
		{
			UVerticalBox* CountCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			const FLinearColor CountColTint = bDamaged ? T->Accent : FLinearColor(0.99f, 0.99f, 0.98f, 0.98f);
			UTextBlock* CountTxt = ERUI::MakeText(this,
				FText::FromString(FString::Printf(TEXT("%d / %d"), L.AliveSoldiers, L.MaxSoldiers)),
				EERTypeScale::Title, CountColTint, true);
			ERUI::ApplyResonantText(CountTxt, 26, true, 40, CountColTint, FVector2D(2.0f, 2.5f), 0.86f);
			CountTxt->SetJustification(ETextJustify::Right);
			CountCol->AddChildToVerticalBox(CountTxt);

			UTextBlock* CountLbl = ERUI::MakeText(this, INVTEXT("在编兵员"), EERTypeScale::Micro, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), false);
			ERUI::ApplyResonantText(CountLbl, 11, false, 120, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f), FVector2D(1.5f, 2.0f), 0.85f);
			CountLbl->SetJustification(ETextJustify::Right);
			CountCol->AddChildToVerticalBox(CountLbl);

			UHorizontalBoxSlot* CS = Row->AddChildToHorizontalBox(CountCol);
			CS->SetVerticalAlignment(VAlign_Center);
			CS->SetPadding(FMargin(0.0f, 0.0f, 32.0f, 0.0f));
		}

		// 5. 补员与同步升级按钮 (纯白错位阴影)
		const int32 ReinforceCost = L.GetFullReinforceCost();
		const bool bNeedReinforce = bDamaged && Campaign->GetCredits() >= L.GetReinforceCostPerSoldier();
		const int32 LegionId = L.LegionId;
		UERActionButton* ReinfBtn = MakeResonantButton(
			FText::FromString(bDamaged
				? FString::Printf(TEXT("补充兵员 · %d 银河币"), ReinforceCost)
				: TEXT("兵员满编")),
			15, false, false,
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
		Gap->SetSize(FVector2D(22.0f, 1.0f));
		Row->AddChildToHorizontalBox(Gap);

		const int32 SyncCost = L.GetSyncUpgradeCost();
		const bool bCanUpgrade = (L.SyncLevel < 5) && (Campaign->GetCredits() >= SyncCost);
		UERActionButton* UpgBtn = MakeResonantButton(
			FText::FromString(L.SyncLevel >= 5
				? TEXT("虹膜同步已满级")
				: FString::Printf(TEXT("升级同步 · %d 银河币"), SyncCost)),
			15, false, false,
			[this, LegionId]()
			{
				if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
				{
					if (Camp->UpgradeLegionSync(LegionId)) { RefreshAll(); }
				}
			});
		UpgBtn->SetButtonEnabled(bCanUpgrade);
		Row->AddChildToHorizontalBox(UpgBtn)->SetVerticalAlignment(VAlign_Center);

		RosterListBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 16.0f));
		if (i + 1 < Legions.Num())
		{
			RosterListBox->AddChildToVerticalBox(MakeHairlineH(0.12f));
		}
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

void UERGalaCommandPanel::StyleDarkInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword)
{
	if (!Box) { return; }
	const UERUITheme* T = ERUI::Theme();

	// 深墨半透输入框 + 纯白文字 + 聚焦转暖金发丝框
	FEditableTextBoxStyle Style = Box->GetWidgetStyle();
	Style.SetFont(ERUI::FontRegular(14, 20));
	Style.SetForegroundColor(FSlateColor(FLinearColor(0.99f, 0.99f, 0.98f, 1.0f)));
	Style.SetFocusedForegroundColor(FSlateColor(FLinearColor(0.99f, 0.99f, 0.98f, 1.0f)));
	Style.SetBackgroundColor(FSlateColor(FLinearColor(0.014f, 0.016f, 0.022f, 0.94f)));
	Style.SetBackgroundImageNormal(ERUI::RoundedBrush(0.0f, FLinearColor(0.014f, 0.016f, 0.022f, 0.92f), 1.0f, FLinearColor(0.99f, 0.99f, 0.98f, 0.24f)));
	Style.SetBackgroundImageHovered(ERUI::RoundedBrush(0.0f, FLinearColor(0.020f, 0.022f, 0.030f, 0.96f), 1.0f, FLinearColor(0.99f, 0.99f, 0.98f, 0.48f)));
	Style.SetBackgroundImageFocused(ERUI::RoundedBrush(0.0f, FLinearColor(0.022f, 0.025f, 0.034f, 0.98f), 1.2f, T->Accent));
	Style.SetPadding(FMargin(12.0f, 9.0f));
	Box->SetWidgetStyle(Style);
	Box->SetHintText(Hint);
	Box->SetIsPassword(bIsPassword);
}

void UERGalaCommandPanel::SyncSettingsInputsFromService()
{
	if (!SettingsEndpointInput) { return; }

	UWorld* World = GetWorld();
	UBattleAICommandService* Service = World ? UBattleAICommandService::Get(World) : nullptr;
	if (!Service)
	{
		if (SettingsStatusText)
		{
			SettingsStatusText->SetText(INVTEXT("AI 指挥模块未编译启用 (ER_WITH_AI_COMMAND=0)"));
			SettingsStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.99f, 0.99f, 0.98f, 0.65f)));
		}
		return;
	}

	const FAICommandConfig& Cfg = Service->GetConfig();
	SettingsEndpointInput->SetText(FText::FromString(Cfg.EndpointUrl));
	if (SettingsModelInput) { SettingsModelInput->SetText(FText::FromString(Cfg.ModelName)); }
	if (SettingsApiKeyInput) { SettingsApiKeyInput->SetText(FText::FromString(Cfg.ApiKey)); }

	if (SettingsStatusText)
	{
		const bool bReady = Service->HasApiKey();
		SettingsStatusText->SetText(FText::FromString(
			bReady ? TEXT("密钥已就绪 · 战场中可接入 AI") : TEXT("未配置密钥 · AI 接入不可用")));
		SettingsStatusText->SetColorAndOpacity(FSlateColor(
			bReady ? FLinearColor(0.42f, 0.92f, 0.56f, 0.96f) : FLinearColor(0.99f, 0.72f, 0.24f, 0.96f)));
	}
}

void UERGalaCommandPanel::OnSaveAIConfigClicked()
{
	UWorld* World = GetWorld();
	UBattleAICommandService* Service = World ? UBattleAICommandService::Get(World) : nullptr;
	if (!Service || !SettingsEndpointInput || !SettingsModelInput || !SettingsApiKeyInput)
	{
		return;
	}

	// 保留现有附加提示词/工具轮次等不在此面板编辑的字段
	FAICommandConfig NewCfg = Service->GetConfig();
	NewCfg.EndpointUrl = SettingsEndpointInput->GetText().ToString();
	NewCfg.ModelName = SettingsModelInput->GetText().ToString();
	NewCfg.ApiKey = SettingsApiKeyInput->GetText().ToString();
	Service->SetConfig(NewCfg, true);

	if (SettingsStatusText)
	{
		const bool bReady = Service->HasApiKey();
		SettingsStatusText->SetText(FText::FromString(
			bReady ? TEXT("配置已保存 · 密钥已就绪") : TEXT("配置已保存 · 警告：密钥为空，远程接口将鉴权失败")));
		SettingsStatusText->SetColorAndOpacity(FSlateColor(
			bReady ? FLinearColor(0.42f, 0.92f, 0.56f, 1.0f) : FLinearColor(0.99f, 0.72f, 0.24f, 1.0f)));
	}
}
