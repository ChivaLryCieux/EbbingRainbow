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
		INVTEXT("虹膜基金会  ·  银河委托与战局终端"),
		INVTEXT("虹膜基金会  ·  虹膜兵团整备终端"),
		INVTEXT("虹膜基金会  ·  银河局势终端"),
		INVTEXT("虹膜基金会  ·  终端设置"),
	};
	if (PanelTitleText)
	{
		PanelTitleText->SetText(Titles[ActiveTab]);
	}

	UWidget* Pages[] = { ContractsPage, RosterPage, SituationPage, SettingsPage };
	for (int32 i = 0; i < 4; ++i)
	{
		if (Pages[i])
		{
			Pages[i]->SetVisibility(i == ActiveTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (TabButtons.IsValidIndex(i) && TabButtons[i])
		{
			TabButtons[i]->SetUrgent(i == ActiveTab);
		}
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
		CreditsText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, false);
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

		// 控制台终端关闭后回到第一人称大厅 (标题菜单中的设置入口同样适用)
		UERActionButton* CloseBtn = MakeBoneButton(INVTEXT("断开连接"), 26, true, [this]() { ClosePanel(); });
		CloseBtn->SetMinSize(220.0f, 0.0f);
		HeaderRow->AddChildToHorizontalBox(CloseBtn)->SetVerticalAlignment(VAlign_Center);

		MainCol->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	// 战役存档状态提示 (无异常时折叠)
	CampaignSaveNotice = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, false);
	CampaignSaveNotice->SetAutoWrapText(true);
	CampaignSaveNotice->SetVisibility(ESlateVisibility::Collapsed);
	MainCol->AddChildToVerticalBox(CampaignSaveNotice)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	// 分割线
	UERTechDivider* Div = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass());
	Div->AnchorLength = 42.0f;
	Div->AnchorColor = T->Accent;
	Div->LineColor = T->BoneGhost;
	MainCol->AddChildToVerticalBox(Div)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// 控制台是游戏流程的唯一入口, 因此终端必须自带页签导航 (旧版依赖大厅直发按钮)。
	{
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TerminalTabRow"));
		TerminalTabRow = TabRow;
		static const FText TabLabels[4] = {
			INVTEXT("接取委托"), INVTEXT("兵团整备"), INVTEXT("银河局势"), INVTEXT("终端设置")
		};
		TabButtons.Reset();
		for (int32 Tab = 0; Tab < 4; ++Tab)
		{
			const int32 TabIndex = Tab;
			UERActionButton* TabBtn = MakeBoneButton(TabLabels[Tab], 17, false,
				[this, TabIndex]() { SetActiveTab(TabIndex); });
			TabBtn->SetMinSize(150.0f, 0.0f);
			TabButtons.Add(TabBtn);
			TabRow->AddChildToHorizontalBox(TabBtn)->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		}
		MainCol->AddChildToVerticalBox(TabRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	// =========================================================================
	// 内容区: Overlay 页签
	// =========================================================================
	UOverlay* ContentOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ContentOverlay"));
	{
		UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(ContentOv);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// =========================================================================
	// TAB 0: 银河委托 (仅合同卡片, 从左至右三张排布)
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
		{
			UHorizontalBoxSlot* S = ContractHeader->AddChildToHorizontalBox(ERUI::MakeSectionBar(this, INVTEXT("待接取军事委托"), 22));
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}
		RefreshContractsButton = MakeBoneButton(INVTEXT("刷新加密频道"), 21, false, [this]() { OnRefreshContractsClicked(); });
		ContractHeader->AddChildToHorizontalBox(RefreshContractsButton)->SetVerticalAlignment(VAlign_Center);
		Page0->AddChildToVerticalBox(ContractHeader)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		ContractListBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		{
			UVerticalBoxSlot* S = Page0->AddChildToVerticalBox(ContractListBox);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// =========================================================================
	// TAB 2: 银河局势 (三方势力态势 + 重点星区归属 + 上一战况)
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

		// 左列: 单幅黑灰控制率环图
		UScrollBox* LeftScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* LeftCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftScroll->AddChild(LeftCol);
		{
			UHorizontalBoxSlot* S = Page2->AddChildToHorizontalBox(LeftScroll);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
		}

		LeftCol->AddChildToVerticalBox(ERUI::MakeSectionBar(this, INVTEXT("银河总体控制率"), 22, ERUI::InkText(), false))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		ERUI::FBoneCard Chart = ERUI::MakeBoneCard(this, ERUI::InkText(), false, FMargin(18.0f, 12.0f));
		ControlShareCard = Chart.Root;
		ControlShareCard->SetVisibility(ESlateVisibility::Collapsed);
		ControlSharePie = WidgetTree->ConstructWidget<UERControlSharePie>(UERControlSharePie::StaticClass());
		ControlSharePie->SetVisibility(ESlateVisibility::HitTestInvisible);
		Chart.Content->AddChildToVerticalBox(ControlSharePie)->SetHorizontalAlignment(HAlign_Fill);
		UTextBlock* ChartNote = ERUI::MakeText(this, INVTEXT("扇区面积对应控制率，不按重点星区数量计算"), EERTypeScale::Micro, ERUI::InkText(0.65f), false);
		ChartNote->SetFont(ERUI::FontRegular(14, 20));
		ChartNote->SetJustification(ETextJustify::Center);
		ChartNote->SetAutoWrapText(true);
		Chart.Content->AddChildToVerticalBox(ChartNote)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		LeftCol->AddChildToVerticalBox(Chart.Root);

		// 右列: 势力简报 + 星区控制表 + 上局战报
		UScrollBox* RightScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightScroll->AddChild(RightCol);
		{
			UHorizontalBoxSlot* S = Page2->AddChildToHorizontalBox(RightScroll);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		RightCol->AddChildToVerticalBox(ERUI::MakeSectionBar(this, INVTEXT("势力简报"), 22, ERUI::InkText(), false))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		FactionListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightCol->AddChildToVerticalBox(FactionListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		RightCol->AddChildToVerticalBox(ERUI::MakeSectionBar(this, INVTEXT("重点星区归属"), 22))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		SectorListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightCol->AddChildToVerticalBox(SectorListBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

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
		RightCol->AddChildToVerticalBox(LastOutcomeBox);
	}

	// =========================================================================
	// TAB 3: 设置 (AI 战术指挥接入配置)
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

		Page3->AddChildToVerticalBox(ERUI::MakeSectionBar(this, INVTEXT("终端设置"), 22))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		CampaignSaveRetryButton = MakeBoneButton(INVTEXT("重试战役存档"), 18, false, [this]() {
			if (UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this))
			{
				Camp->RetryCampaignSave();
				RefreshAll();
			}
		});
		CampaignSaveRetryButton->SetVisibility(ESlateVisibility::Collapsed);
		Page3->AddChildToVerticalBox(CampaignSaveRetryButton)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		// ---- AI 战术指挥接入卡: 协议预设 + 接口/模型/密钥 + 保存 ----
		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, T->Accent, false, FMargin(24.0f, 18.0f));
		UVerticalBox* Inner = Card.Content;

		UTextBlock* AITitle = ERUI::MakeText(this, INVTEXT("AI 战术指挥接入"), EERTypeScale::Title, ERUI::InkText(), false);
		AITitle->SetFont(ERUI::FontBold(20, 40));
		Inner->AddChildToVerticalBox(AITitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		UTextBlock* AIDesc = ERUI::MakeText(this,
			INVTEXT("配置大模型接口后, 战场上可通过右上「接入 AI」以自然语言指挥全军。接口地址同时决定通信协议: 以 /messages 结尾走 Anthropic 协议, 其余走 OpenAI 兼容协议。"),
			EERTypeScale::Micro, ERUI::InkText(0.55f), false);
		AIDesc->SetFont(ERUI::FontRegular(13, 20));
		AIDesc->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(AIDesc)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// 输入行: 标签固定宽 + 输入框通栏
		auto AddInputRow = [this, Inner](const FText& Label, TObjectPtr<UEditableTextBox>& OutInput, const FText& Hint, bool bPassword)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelBox->SetWidthOverride(90.0f);
			UTextBlock* Lbl = ERUI::MakeText(this, Label, EERTypeScale::Micro, ERUI::InkText(0.72f), false);
			Lbl->SetFont(ERUI::FontBold(14, 60));
			LabelBox->AddChild(Lbl);
			Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

			UEditableTextBox* InputWidget = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			StyleBoneInputBox(InputWidget, Hint, bPassword);
			OutInput = InputWidget;
			{
				UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InputWidget);
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			Inner->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		};

		AddInputRow(INVTEXT("接口地址"), SettingsEndpointInput, INVTEXT("https://api.deepseek.com/v1/chat/completions"), false);
		AddInputRow(INVTEXT("模型名称"), SettingsModelInput,    INVTEXT("deepseek-chat"), false);
		AddInputRow(INVTEXT("API Key"), SettingsApiKeyInput,   INVTEXT("sk-... (仅保存于本地 Saved/ER_AIConfig.json)"), true);

		UTextBlock* KeyNote = ERUI::MakeText(this,
			INVTEXT("密钥仅保存在本机 Saved/ER_AIConfig.json, 不上传任何其他服务器, 也不进入版本库。"),
			EERTypeScale::Micro, ERUI::InkText(0.45f), false);
		KeyNote->SetFont(ERUI::FontRegular(12, 20));
		KeyNote->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(KeyNote)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// 保存行: 按钮 + 状态反馈
		{
			UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UERActionButton* SaveBtn = MakeBoneButton(INVTEXT("保存配置"), 18, true,
				[this]() { OnSaveAIConfigClicked(); });
			SaveRow->AddChildToHorizontalBox(SaveBtn)->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));

			SettingsStatusText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::InkText(0.55f), false);
			SettingsStatusText->SetFont(ERUI::FontRegular(13, 20));
			SettingsStatusText->SetAutoWrapText(true);
			{
				UHorizontalBoxSlot* S = SaveRow->AddChildToHorizontalBox(SettingsStatusText);
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			Inner->AddChildToVerticalBox(SaveRow);
		}
		Page3->AddChildToVerticalBox(Card.Root)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// ---- 画面与音频 (后续周期开放) ----
		ERUI::FBoneCard StubCard = ERUI::MakeBoneCard(this, T->BoneGhost, false, FMargin(24.0f, 14.0f));
		UTextBlock* Placeholder = ERUI::MakeText(this,
			INVTEXT("画面、音频与指挥终端偏好将在后续周期开放。"),
			EERTypeScale::Body, ERUI::InkText(0.5f), false);
		Placeholder->SetFont(ERUI::FontRegular(14, 20));
		StubCard.Content->AddChildToVerticalBox(Placeholder);
		Page3->AddChildToVerticalBox(StubCard.Root);
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

		ReinforceAllButton = MakeBoneButton(INVTEXT("一键满编补员"), 24, false, [this]() { OnReinforceAllClicked(); });
		TopBar->AddChildToHorizontalBox(ReinforceAllButton)->SetVerticalAlignment(VAlign_Center);
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
	const UERUITheme* T = ERUI::Theme();

	const TArray<FFactionStatus>& Factions = Campaign->GetFactions();
	static const FColor GrayRanks[] = { FColor(26, 26, 26), FColor(84, 84, 84), FColor(151, 151, 151) };
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
		Slices.Add({ Share, FLinearColor::FromSRGBColor(GrayRanks[FMath::Min(Rank, static_cast<int32>(UE_ARRAY_COUNT(GrayRanks)) - 1)]) });
	}

	FactionListBox->ClearChildren();
	for (int32 Index = 0; Index < Factions.Num(); ++Index)
	{
		const FFactionStatus& F = Factions[Index];
		const FLinearColor SliceFill = Slices[Index].Fill;
		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, SliceFill, false, FMargin(14.0f, 8.0f));

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
			S->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 势力 Logo (纯 logo 无底片, 纵横比自适应)
		if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetFactionLogoAssetPath(F.Faction)))
		{
			if (UImage* LogoImg = ERUI::MakeLogoImage(this, Tex, 30.0f))
			{
				UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(LogoImg);
				LS->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				LS->SetVerticalAlignment(VAlign_Center);
			}
		}
		UTextBlock* NameTxt = ERUI::MakeText(this, FText::FromString(F.DisplayName), EERTypeScale::Caption, ERUI::InkText(), false);
		NameTxt->SetFont(ERUI::FontBold(21, 60));
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameTxt);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* PctTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("控制率 %.1f%%  ·  声望 %+d"), F.GalacticControl, F.Reputation)),
			EERTypeScale::Micro, ERUI::InkText(0.8f), false);
		PctTxt->SetFont(ERUI::FontBold(16, 40));
		Row->AddChildToHorizontalBox(PctTxt);
		Card.Content->AddChildToVerticalBox(Row);

		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(F.DoctrineBrief), EERTypeScale::Micro, ERUI::InkText(0.62f), false);
		Brief->SetFont(ERUI::FontRegular(14, 20));
		Brief->SetAutoWrapText(true);
		Card.Content->AddChildToVerticalBox(Brief)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

		FactionListBox->AddChildToVerticalBox(Card.Root)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
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
				TEXT("上一战况 [%s]: %s\n经费 +%d 银河币 · 我方伤亡 %d · %s控制率 %+.1f%%"),
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
		if (UOverlaySlot* CS = Cast<UOverlaySlot>(Card.Content->Slot))
		{
			CS->SetVerticalAlignment(VAlign_Fill);
		}
		UVerticalBox* Inner = Card.Content;

		const FGalacticSector* Sec = Campaign->FindSector(C.TargetSectorId);
		const FString SecName = Sec ? Sec->Name : TEXT("未知星区");

		// ---- 卡片正文: 七要素垂直排列, 每行独占一个横带, 行间留白 ----

		// 1. 交战双方势力 Logo 英雄区: 去除占位技能图标, 雇主/敌对大 logo 同高居中,
		//    合计宽度 ~470px (1080p 下卡片宽 588px 的 80%), 高度按双方纵横比联立求解
		{
			UTexture2D* EmpTex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetFactionLogoAssetPath(C.Employer));
			UTexture2D* EneTex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetFactionLogoAssetPath(C.EnemyFaction));
			if (EmpTex && EneTex)
			{
				const float A1 = (float)EmpTex->GetSizeX() / FMath::Max(1.0f, (float)EmpTex->GetSizeY());
				const float A2 = (float)EneTex->GetSizeX() / FMath::Max(1.0f, (float)EneTex->GetSizeY());
				const float Gap = 28.0f;
				const float Height = 446.0f / (A1 + A2);

				UHorizontalBox* HeroRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UImage* EmpImg = ERUI::MakeLogoImage(this, EmpTex, Height))
				{
					UHorizontalBoxSlot* S = HeroRow->AddChildToHorizontalBox(EmpImg);
					S->SetPadding(FMargin(0.0f, 0.0f, Gap, 0.0f));
					S->SetVerticalAlignment(VAlign_Center);
				}
				if (UImage* EneImg = ERUI::MakeLogoImage(this, EneTex, Height))
				{
					UHorizontalBoxSlot* S = HeroRow->AddChildToHorizontalBox(EneImg);
					S->SetVerticalAlignment(VAlign_Center);
				}
				UVerticalBoxSlot* HS = Inner->AddChildToVerticalBox(HeroRow);
				HS->SetHorizontalAlignment(HAlign_Center);
				HS->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 18.0f));
			}
		}

		// 2. 任务名字 (独占整宽, 不与 Logo/星级抢横向空间)
		UTextBlock* Title = ERUI::MakeText(this, FText::FromString(C.Title), EERTypeScale::Title, ERUI::InkText(), false);
		Title->SetFont(ERUI::FontBold(24, 40));
		Title->SetAutoWrapText(true);
		Inner->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		// 3. 任务地点: 标签浅墨 + 星区名重墨
		{
			UHorizontalBox* LocRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* LocLabel = ERUI::MakeText(this, INVTEXT("任务地点"), EERTypeScale::Micro, ERUI::InkText(0.48f), false);
			LocLabel->SetFont(ERUI::FontBold(13, 80));
			LocRow->AddChildToHorizontalBox(LocLabel)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			UTextBlock* LocValue = ERUI::MakeText(this, FText::FromString(SecName), EERTypeScale::Caption, ERUI::InkText(0.92f), false);
			LocValue->SetFont(ERUI::FontBold(16, 50));
			LocRow->AddChildToHorizontalBox(LocValue);
			Inner->AddChildToVerticalBox(LocRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		// 雇主 → 敌对 (tertiary 情报行, 纯文本: 大 logo 已在顶部承担视觉身份)
		UTextBlock* Meta = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("雇主 %s  ·  敌对 %s  ·  %s"),
				*ERGalacticMeta::GetFactionName(C.Employer),
				*ERGalacticMeta::GetFactionName(C.EnemyFaction),
				*ERGalacticMeta::GetTacticalProfileName(C.TacticalProfile))),
			EERTypeScale::Micro, ERUI::InkText(0.55f), false);
		Meta->SetFont(ERUI::FontRegular(13, 30));
		Inner->AddChildToVerticalBox(Meta)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		// 4. 难度星级: 最低一星, 标签浅墨 + 琥珀星同行
		{
			UHorizontalBox* StarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* StarLabel = ERUI::MakeText(this, INVTEXT("任务难度"), EERTypeScale::Micro, ERUI::InkText(0.48f), false);
			StarLabel->SetFont(ERUI::FontBold(13, 80));
			{
				UHorizontalBoxSlot* S = StarRow->AddChildToHorizontalBox(StarLabel);
				S->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
				S->SetVerticalAlignment(VAlign_Center);
			}
			FString Stars;
			const int32 Rating = FMath::Clamp(C.DangerRating, 1, 5);
			for (int32 s = 0; s < 5; ++s) { Stars += (s < Rating) ? TEXT("★") : TEXT("☆"); }
			UTextBlock* DangerTxt = ERUI::MakeText(this, FText::FromString(Stars),
				EERTypeScale::Caption, FLinearColor(0.58f, 0.31f, 0.0f, 1.0f), false);
			DangerTxt->SetFont(ERUI::FontBold(17, 40));
			StarRow->AddChildToHorizontalBox(DangerTxt);
			Inner->AddChildToVerticalBox(StarRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}

		// 5. 任务简述: 占据弹性空间, 把报酬与按钮压到底部
		UTextBlock* Brief = ERUI::MakeText(this, FText::FromString(C.Briefing), EERTypeScale::Body, ERUI::InkText(0.66f), false);
		Brief->SetFont(ERUI::FontRegular(15, 20));
		Brief->SetAutoWrapText(true);
		{
			UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(Brief);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}

		// ---- 任务报酬 (要素 6): 标签/酬金/控制权/敌情各自成行 ----
		UTextBlock* RewardLabel = ERUI::MakeText(this, INVTEXT("任务报酬"), EERTypeScale::Micro, ERUI::InkText(0.48f), false);
		RewardLabel->SetFont(ERUI::FontBold(13, 80));
		Inner->AddChildToVerticalBox(RewardLabel)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		UTextBlock* RewardValue = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("+%d 银河币"), C.RewardCredits)),
			EERTypeScale::Caption, ERUI::InkText(), false);
		RewardValue->SetFont(ERUI::FontBold(21, 30));
		Inner->AddChildToVerticalBox(RewardValue)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));

		UTextBlock* ControlTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("控制权 %+.1f%%"), C.ControlShift)),
			EERTypeScale::Micro, ERUI::InkText(0.78f), false);
		ControlTxt->SetFont(ERUI::FontBold(14, 30));
		Inner->AddChildToVerticalBox(ControlTxt)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		UTextBlock* Intel = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("敌情 %d 波次 · %d 名作战单位"),
				C.EnemyWaves.Num(), C.GetTotalEnemySoldiers())),
			EERTypeScale::Micro, ERUI::InkText(0.55f), false);
		Intel->SetFont(ERUI::FontRegular(13, 30));
		Inner->AddChildToVerticalBox(Intel)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// ---- 接取按钮 (要素 7): 通栏主按钮 ----
		const int32 ContractId = C.ContractId;
		UERActionButton* DeployBtn = MakeBoneButton(INVTEXT("接取委托 · 出击部署"), 22, true,
			[this, ContractId]()
			{
				UGalacticCampaignSubsystem* Camp = UGalacticCampaignSubsystem::Get(this);
				if (!Camp || bDeployInTransit) { return; }
				bDeployInTransit = true;

				// 先盖上静态「神经接驳」过渡屏, 随后直接调 OpenLevel —
				// SetClientTravel 本就延迟到下一帧引擎 tick 才处理 (UEngine::TickWorldTravel),
				// 本帧过渡屏照常光栅化, 关卡加载卡顿发生在接驳屏上,
				// 与 Lvl_Legion 内的交互式接驳屏视觉连续 (草皮构筑期间玩家不会盯着黑屏)。
				// 切勿经 FTSTicker 转一手: 核心 ticker 回调脱离 PIE 世界上下文 (GPlayInEditorID==-1),
				// 此时 GetCurrentPlayWorld() 返回空, 部署静默失败 —— 已踩过 (2026-11)
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
					// 部署被拒绝 (合同失效/无存活士兵等): 撤销过渡屏并解锁按钮
					bDeployInTransit = false;
					if (Screen) { Screen->RemoveFromParent(); }
					RefreshAll();
				}
			});
		{
			UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(DeployBtn);
			S->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UHorizontalBoxSlot* S = ContractListBox->AddChildToHorizontalBox(Card.Root))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
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
			FText::FromString(FString::Printf(TEXT("招募%s · %d 银河币"), *ERGalacticMeta::GetClassName(Cls), Cost)),
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

		// 部队占位 Logo
		{
			UBorder* LogoBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			LogoBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.012f, 0.013f, 0.018f, 0.94f), 1.0f, FLinearColor(0.02f, 0.02f, 0.03f, 0.5f)));
			LogoBox->SetBrushColor(FLinearColor::White);
			LogoBox->SetPadding(FMargin(3.0f));

			UImage* LogoImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush IconBrush;
			IconBrush.ImageSize = FVector2D(46.0f, 46.0f);
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
				S->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
			}
		}

		UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		InfoCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InfoCol);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* NameTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("%s   ·   %s   ·   虹膜同步 %d 级"),
				*L.Callsign, *ERGalacticMeta::GetClassName(L.Archetype), L.SyncLevel)),
			EERTypeScale::Title, ERUI::InkText(), false);
		NameTxt->SetFont(ERUI::FontBold(23, 40));
		InfoCol->AddChildToVerticalBox(NameTxt);

		UTextBlock* RoleTxt = ERUI::MakeText(this,
			FText::FromString(FString::Printf(TEXT("定位: %s   |   战历: %d 场"),
				*ERGalacticMeta::GetClassRoleDesc(L.Archetype),
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
				? FString::Printf(TEXT("补充兵员 · %d 银河币"), ReinforceCost)
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
				: FString::Printf(TEXT("升级同步 · %d 银河币"), SyncCost)),
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

void UERGalaCommandPanel::StyleBoneInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword)
{
	if (!Box) { return; }
	const UERUITheme* T = ERUI::Theme();

	// 白底墨字主题: 浅骨底 + 墨色文字, 聚焦时描边转虹彩
	FEditableTextBoxStyle Style = Box->GetWidgetStyle();
	Style.SetFont(ERUI::FontRegular(14, 20));
	Style.SetForegroundColor(FSlateColor(ERUI::InkText()));
	Style.SetFocusedForegroundColor(FSlateColor(ERUI::InkText()));
	Style.SetBackgroundColor(FSlateColor(FLinearColor::White));
	Style.SetBackgroundImageNormal(ERUI::RoundedBrush(0.0f, FLinearColor(0.93f, 0.925f, 0.90f, 1.0f), 1.0f, ERUI::InkText(0.22f)));
	Style.SetBackgroundImageHovered(ERUI::RoundedBrush(0.0f, FLinearColor(0.95f, 0.945f, 0.92f, 1.0f), 1.0f, ERUI::InkText(0.45f)));
	Style.SetBackgroundImageFocused(ERUI::RoundedBrush(0.0f, FLinearColor(0.97f, 0.965f, 0.94f, 1.0f), 1.2f, T->Accent));
	Style.SetPadding(FMargin(10.0f, 7.0f));
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
			SettingsStatusText->SetColorAndOpacity(FSlateColor(ERUI::InkText(0.55f)));
		}
		return;
	}

	const FAICommandConfig& Cfg = Service->GetConfig();
	SettingsEndpointInput->SetText(FText::FromString(Cfg.EndpointUrl));
	if (SettingsModelInput) { SettingsModelInput->SetText(FText::FromString(Cfg.ModelName)); }
	if (SettingsApiKeyInput) { SettingsApiKeyInput->SetText(FText::FromString(Cfg.ApiKey)); }

	if (SettingsStatusText)
	{
		SettingsStatusText->SetText(FText::FromString(
			Service->HasApiKey() ? TEXT("密钥已就绪 · 战场中可接入 AI") : TEXT("未配置密钥 · AI 接入不可用")));
		SettingsStatusText->SetColorAndOpacity(FSlateColor(ERUI::InkText(0.55f)));
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
			bReady ? TEXT("配置已保存 · 密钥已就绪") : TEXT("配置已保存 · 警告: 密钥为空, 远程接口将鉴权失败")));
		SettingsStatusText->SetColorAndOpacity(FSlateColor(
			bReady ? FLinearColor(0.15f, 0.45f, 0.2f, 1.0f) : FLinearColor(0.6f, 0.3f, 0.05f, 1.0f)));
	}
}
