// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERActionButton.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

TArray<TWeakObjectPtr<UERAICommandPanel>>& UERAICommandPanel::ActivePanels()
{
	static TArray<TWeakObjectPtr<UERAICommandPanel>> Panels;
	return Panels;
}

bool UERAICommandPanel::IsAnyTextInputFocused()
{
	ActivePanels().RemoveAll([](const TWeakObjectPtr<UERAICommandPanel>& P) { return !P.IsValid(); });
	for (const TWeakObjectPtr<UERAICommandPanel>& P : ActivePanels())
	{
		if (P.IsValid() && P->IsTextInputFocused())
		{
			return true;
		}
	}
	return false;
}

UERAICommandPanel* UERAICommandPanel::InstallOnBattleHUD(UUserWidget* OwnerHUD, UCanvasPanel* RootCanvas, float ScreenMargin)
{
#if ER_WITH_AI_COMMAND
	if (!OwnerHUD || !OwnerHUD->WidgetTree || !RootCanvas)
	{
		return nullptr;
	}

	UWidgetTree* Tree = OwnerHUD->WidgetTree;
	UERActionButton* ToggleBtn = Cast<UERActionButton>(Tree->FindWidget(TEXT("AIConnectButton")));
	if (!ToggleBtn)
	{
		ToggleBtn = Tree->ConstructWidget<UERActionButton>(UERActionButton::StaticClass(), TEXT("AIConnectButton"));
		ToggleBtn->Label = INVTEXT("战术链路");
		ToggleBtn->bShowArrow = false;
		ToggleBtn->ContentPadding = FMargin(16.0f, 8.0f);
		UCanvasPanelSlot* BtnSlot = RootCanvas->AddChildToCanvas(ToggleBtn);
		BtnSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		BtnSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		BtnSlot->SetPosition(FVector2D(-ScreenMargin, 28.0f));
		BtnSlot->SetAutoSize(true);
		BtnSlot->SetZOrder(20);
	}

	UERAICommandPanel* Panel = Cast<UERAICommandPanel>(Tree->FindWidget(TEXT("AICommandPanel")));
	if (!Panel)
	{
		Panel = Tree->ConstructWidget<UERAICommandPanel>(UERAICommandPanel::StaticClass(), TEXT("AICommandPanel"));
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		PanelSlot->SetPosition(FVector2D(-ScreenMargin, 82.0f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetZOrder(30);
	}

	Panel->LinkedToggleButton = ToggleBtn;
	ToggleBtn->OnActionClicked.AddUniqueDynamic(Panel, &UERAICommandPanel::HandleToggleButtonClicked);
	Panel->OnCloseRequested.AddUniqueDynamic(Panel, &UERAICommandPanel::HandleToggleButtonClicked);

	if (UWorld* World = OwnerHUD->GetWorld())
	{
		if (UBattleAICommandService* Svc = UBattleAICommandService::Get(World))
		{
			Panel->BindService(Svc);
		}
	}
	return Panel;
#else
	return nullptr;
#endif
}

TSharedRef<SWidget> UERAICommandPanel::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UERAICommandPanel::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		BuildLayout();
	}
	Super::NativeConstruct();
	ActivePanels().AddUnique(this);

	if (SettingsToggleButton) { SettingsToggleButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleToggleSettingsClicked); }
	if (CloseButton)          { CloseButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleCloseClicked); }
	if (SaveConfigButton)     { SaveConfigButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleSaveConfigClicked); }
	if (PresetDeepSeekBtn)    { PresetDeepSeekBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetDeepSeekClicked); }
	if (PresetOpenAIBtn)      { PresetOpenAIBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetOpenAIClicked); }
	if (PresetQwenBtn)        { PresetQwenBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetQwenClicked); }
	if (PresetOllamaBtn)      { PresetOllamaBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetOllamaClicked); }
	if (SendButton)           { SendButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleSendClicked); }
	if (NewContextButton)     { NewContextButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleNewContextClicked); }
	if (ClearLogButton)       { ClearLogButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleClearLogClicked); }
	if (CommandInput)         { CommandInput->OnTextCommitted.AddUniqueDynamic(this, &UERAICommandPanel::HandleCommandCommitted); }
}

void UERAICommandPanel::NativeDestruct()
{
	ActivePanels().RemoveAll([this](const TWeakObjectPtr<UERAICommandPanel>& P)
	{
		return !P.IsValid() || P.Get() == this;
	});

	if (UBattleAICommandService* Svc = Service.Get())
	{
		if (LogDelegateHandle.IsValid())   { Svc->OnLogEntry.Remove(LogDelegateHandle); }
		if (StateDelegateHandle.IsValid()) { Svc->OnStateChanged.Remove(StateDelegateHandle); }
	}
	LogDelegateHandle.Reset();
	StateDelegateHandle.Reset();
	Super::NativeDestruct();
}

void UERAICommandPanel::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(508.0f);
	PanelSize->SetMaxDesiredHeight(640.0f);
	WidgetTree->RootWidget = PanelSize;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetBrush(ERUI::TransparentBrush());
	RootBorder->SetBrushColor(FLinearColor::White);
	RootBorder->SetPadding(FMargin(0.0f));
	PanelSize->AddChild(RootBorder);

	UOverlay* FrameOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FrameOverlay"));
	RootBorder->AddChild(FrameOverlay);

	// 1. 黑曜石 45° 切角战术终端底板 + 左侧《虹不我与》窄光谱条 + 四角对准刻度
	PanelFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("PanelFrame"));
	PanelFrame->Chamfer = FVector4(0.0f, 16.0f, 0.0f, 16.0f);
	PanelFrame->FillColor = FLinearColor(0.006f, 0.007f, 0.010f, 0.96f);
	PanelFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.22f);
	PanelFrame->BorderThickness = 1.0f;
	PanelFrame->bDrawCornerBrackets = true;
	PanelFrame->BracketLength = 12.0f;
	PanelFrame->BracketThickness = 1.5f;
	PanelFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.78f);
	PanelFrame->AccentEdge = EERAccentEdge::Left;
	PanelFrame->bUsePrismAccent = true;
	PanelFrame->AccentThickness = 3.5f;
	PanelFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = FrameOverlay->AddChildToOverlay(PanelFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* MainCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainCol"));
	UOverlaySlot* MainSlot = FrameOverlay->AddChildToOverlay(MainCol);
	MainSlot->SetPadding(FMargin(22.0f, 18.0f, 20.0f, 18.0f));

	// ---- 1. Control: 共振式巨细对比终端顶栏 ----
	{
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Header"));
		MainCol->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));
		UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(TitleCol);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		HeaderCaptionText = ERUI::MakeText(this, INVTEXT("战术神经链路"), EERTypeScale::Micro, T->BoneTertiary, true);
		HeaderCaptionText->SetFont(ERUI::FontRegular(8, 260));
		TitleCol->AddChildToVerticalBox(HeaderCaptionText);

		UHorizontalBox* TitleLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleLine"));
		TitleCol->AddChildToVerticalBox(TitleLine)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));

		UTextBlock* Title = ERUI::MakeText(this, INVTEXT("战术链路"), EERTypeScale::Title, T->Bone);
		Title->SetFont(ERUI::FontBold(22, 60));
		TitleLine->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);

		// 反白状态标签块
		StatusBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusBadgeBorder"));
		StatusBadgeBorder->SetBrush(ERUI::RoundedBrush(0.0f, T->Accent));
		StatusBadgeBorder->SetBrushColor(FLinearColor::White);
		StatusBadgeBorder->SetPadding(FMargin(8.0f, 2.0f));
		StatusBadgeText = ERUI::MakeText(this, INVTEXT("未配置密钥"), EERTypeScale::Micro, T->PureInk, true);
		StatusBadgeText->SetFont(ERUI::FontBold(9, 140));
		StatusBadgeBorder->SetContent(StatusBadgeText);
		UHorizontalBoxSlot* BadgeSlot = TitleLine->AddChildToHorizontalBox(StatusBadgeBorder);
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
		BadgeSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));

		SettingsToggleButton = MakeTechButton(TEXT("API"), false,
			FLinearColor(0.018f, 0.020f, 0.026f, 0.92f), FLinearColor(0.92f, 0.92f, 0.90f, 0.28f), T->Bone, &SettingsToggleText);
		UHorizontalBoxSlot* SetSlot = Header->AddChildToHorizontalBox(SettingsToggleButton);
		SetSlot->SetVerticalAlignment(VAlign_Center);
		SetSlot->SetPadding(FMargin(8.0f, 0.0f, 6.0f, 0.0f));

		CloseButton = MakeTechButton(TEXT("关闭"), false,
			FLinearColor(0.018f, 0.020f, 0.026f, 0.92f), FLinearColor(0.92f, 0.92f, 0.90f, 0.32f), T->Bone);
		Header->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);
	}

	// 动态建筑分割线
	HeaderDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("HeaderDivider"));
	HeaderDivider->AnchorLength = 32.0f;
	HeaderDivider->AnchorColor = T->Accent;
	HeaderDivider->LineColor = T->BoneGhost;
	HeaderDivider->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MainCol->AddChildToVerticalBox(HeaderDivider)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// ---- 2. 可折叠 API 配置抽屉 (炭黑子腔体) ----
	{
		SettingsDrawer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsDrawer"));
		SettingsDrawer->SetVisibility(ESlateVisibility::Collapsed);

		UBorder* DrawerBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DrawerBg"));
		DrawerBg->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.014f, 0.016f, 0.021f, 0.96f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.16f)));
		DrawerBg->SetBrushColor(FLinearColor::White);
		DrawerBg->SetPadding(FMargin(12.0f, 10.0f));
		SettingsDrawer->AddChildToVerticalBox(DrawerBg);

		UVerticalBox* DrawerInner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DrawerInner"));
		DrawerBg->SetContent(DrawerInner);

		UHorizontalBox* PresetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PresetRow"));
		DrawerInner->AddChildToVerticalBox(PresetRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UTextBlock* PresetCap = ERUI::MakeText(this, INVTEXT("预设"), EERTypeScale::Micro, T->BoneTertiary, true);
		PresetCap->SetFont(ERUI::FontRegular(8, 200));
		PresetRow->AddChildToHorizontalBox(PresetCap)->SetVerticalAlignment(VAlign_Center);

		auto AddPreset = [this, PresetRow, T](const FString& Label) -> UButton*
		{
			UButton* Btn = MakeTechButton(Label, false,
				FLinearColor(0.008f, 0.009f, 0.012f, 0.95f), FLinearColor(0.92f, 0.92f, 0.90f, 0.22f), T->BoneSecondary);
			UHorizontalBoxSlot* S = PresetRow->AddChildToHorizontalBox(Btn);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
			return Btn;
		};
		PresetDeepSeekBtn = AddPreset(TEXT("DEEPSEEK"));
		PresetOpenAIBtn   = AddPreset(TEXT("OPENAI"));
		PresetQwenBtn     = AddPreset(TEXT("QWEN"));
		PresetOllamaBtn   = AddPreset(TEXT("OLLAMA"));

		auto AddConfigRow = [this, DrawerInner, T](const FString& LabelStr, TObjectPtr<UEditableTextBox>& OutInput, const FText& Hint, bool bPassword)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			DrawerInner->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelBox->SetWidthOverride(56.0f);
			UTextBlock* Lbl = ERUI::MakeText(this, FText::FromString(LabelStr), EERTypeScale::Micro, T->BoneSecondary, true);
			Lbl->SetFont(ERUI::FontBold(9, 160));
			LabelBox->AddChild(Lbl);
			Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

			UEditableTextBox* InputWidget = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			StyleInputBox(InputWidget, Hint, bPassword);
			OutInput = InputWidget;
			UHorizontalBoxSlot* InSlot = Row->AddChildToHorizontalBox(InputWidget);
			InSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			InSlot->SetVerticalAlignment(VAlign_Center);
		};

		AddConfigRow(TEXT("接口地址"),   EndpointInput, INVTEXT("https://api.deepseek.com/v1/chat/completions"), false);
		AddConfigRow(TEXT("模型"), ModelInput,    INVTEXT("deepseek-chat"), false);
		AddConfigRow(TEXT("密钥"),   ApiKeyInput,   INVTEXT("sk-... (仅保存在本地 Saved/ER_AIConfig.json)"), true);

		UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		DrawerInner->AddChildToVerticalBox(SaveRow)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		SaveRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))
			->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		SaveConfigButton = MakeTechButton(TEXT("保存配置 // SAVE"), true, T->Bone, T->Bone, T->PureInk);
		SaveRow->AddChildToHorizontalBox(SaveConfigButton);

		MainCol->AddChildToVerticalBox(SettingsDrawer)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	// ---- 3. 战场实时单位遥测条 ----
	{
		UBorder* IndexBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("IndexBg"));
		IndexBg->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.014f, 0.016f, 0.021f, 0.94f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.14f)));
		IndexBg->SetBrushColor(FLinearColor::White);
		IndexBg->SetPadding(FMargin(10.0f, 8.0f));
		MainCol->AddChildToVerticalBox(IndexBg)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		UHorizontalBox* IdxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		IndexBg->SetContent(IdxRow);

		UImage* CyanBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		CyanBar->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 26.0f)));
		CyanBar->SetColorAndOpacity(T->AllyMark);
		UHorizontalBoxSlot* BSlot = IdxRow->AddChildToHorizontalBox(CyanBar);
		BSlot->SetVerticalAlignment(VAlign_Fill);
		BSlot->SetPadding(FMargin(0.0f, 1.0f, 10.0f, 1.0f));

		UVerticalBox* IdxCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		IdxRow->AddChildToHorizontalBox(IdxCol)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UTextBlock* IdxHeader = ERUI::MakeText(this, INVTEXT("单位索引"), EERTypeScale::Micro, T->BoneTertiary, true);
		IdxHeader->SetFont(ERUI::FontRegular(8, 220));
		IdxCol->AddChildToVerticalBox(IdxHeader);

		UnitIndexText = ERUI::MakeText(this, INVTEXT("等待战场就绪..."), EERTypeScale::Caption, T->Bone);
		UnitIndexText->SetFont(ERUI::FontRegular(11, 20));
		UnitIndexText->SetAutoWrapText(true);
		IdxCol->AddChildToVerticalBox(UnitIndexText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
	}

	// ---- 4. 结构化战报日志流 ----
	{
		UBorder* FeedBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FeedBg"));
		FeedBg->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.004f, 0.005f, 0.007f, 0.85f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.12f)));
		FeedBg->SetBrushColor(FLinearColor::White);
		FeedBg->SetPadding(FMargin(10.0f, 8.0f));

		USizeBox* LogHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LogHeight"));
		LogHeight->SetHeightOverride(240.0f);
		LogHeight->SetContent(FeedBg);
		UVerticalBoxSlot* LogSlot = MainCol->AddChildToVerticalBox(LogHeight);
		LogSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LogSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		LogScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LogScrollBox"));
		FeedBg->SetContent(LogScrollBox);
	}

	// ---- 5. 底部指令控制台 (反白石膏主执行 CTA) ----
	{
		UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InputRow"));
		MainCol->AddChildToVerticalBox(InputRow);

		CommandInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("CommandInput"));
		StyleInputBox(CommandInput, INVTEXT("输入战术指令，如：让当前单位攻击 E1"), false);
		UHorizontalBoxSlot* InSlot = InputRow->AddChildToHorizontalBox(CommandInput);
		InSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InSlot->SetVerticalAlignment(VAlign_Center);
		InSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		SendButton = MakeTechButton(TEXT("执行"), true, T->Bone, T->Bone, T->PureInk, &SendButtonText, &SendButtonBg);
		UHorizontalBoxSlot* SendSlot = InputRow->AddChildToHorizontalBox(SendButton);
		SendSlot->SetVerticalAlignment(VAlign_Center);
		SendSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

		NewContextButton = MakeTechButton(TEXT("新对话"), false,
			FLinearColor(0.015f, 0.017f, 0.022f, 0.94f), FLinearColor(0.92f, 0.92f, 0.90f, 0.24f), T->BoneSecondary);
		UHorizontalBoxSlot* CtxSlot = InputRow->AddChildToHorizontalBox(NewContextButton);
		CtxSlot->SetVerticalAlignment(VAlign_Center);
		CtxSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

		ClearLogButton = MakeTechButton(TEXT("清屏"), false,
			FLinearColor(0.015f, 0.017f, 0.022f, 0.94f), FLinearColor(0.92f, 0.92f, 0.90f, 0.18f), T->BoneTertiary);
		InputRow->AddChildToHorizontalBox(ClearLogButton)->SetVerticalAlignment(VAlign_Center);
	}

	AppendLogLine(EERAICommandLogKind::System, TEXT("战术链路已就绪。支持多轮上下文追问，点击「新对话」可重置记忆。"));
}

UButton* UERAICommandPanel::MakeTechButton(const FString& Label, bool bInvertedPrimary, const FLinearColor& FillTint,
	const FLinearColor& StrokeTint, const FLinearColor& TextColor, TObjectPtr<UTextBlock>* OutLabel, TObjectPtr<UBorder>* OutBg)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style;
	const FSlateBrush Tr = ERUI::TransparentBrush();
	Style.SetNormal(Tr); Style.SetHovered(Tr); Style.SetPressed(Tr); Style.SetDisabled(Tr);
	Style.SetNormalPadding(FMargin(0.0f)); Style.SetPressedPadding(FMargin(0.0f));
	Btn->SetStyle(Style);
	Btn->SetCursor(EMouseCursor::Hand);

	// 统一「白底墨字彩条」: 骨白底 + 墨黑描边 + 左侧点睛条 (调用方传入的暗色配色不再采用)
	UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bg->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.93f, 0.93f, 0.91f, 0.96f), 1.0f, FLinearColor(0.02f, 0.02f, 0.03f, 0.45f)));
	Bg->SetBrushColor(FLinearColor::White);
	Bg->SetPadding(FMargin(16.0f, 8.0f, 14.0f, 8.0f));
	Bg->SetHorizontalAlignment(HAlign_Center);
	Bg->SetVerticalAlignment(VAlign_Center);
	Btn->SetContent(Bg);
	if (OutBg) { *OutBg = Bg; }

	UTextBlock* LabelWidget = ERUI::MakeText(this, FText::FromString(Label), EERTypeScale::Micro, ERUI::InkText(), false);
	LabelWidget->SetFont(ERUI::FontBold(16, 60));
	if (OutLabel) { *OutLabel = LabelWidget; }
	Bg->SetContent(LabelWidget);
	return Btn;
}

void UERAICommandPanel::StyleInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword)
{
	if (!Box) return;
	const UERUITheme* T = ERUI::Theme();

	FEditableTextBoxStyle Style = Box->GetWidgetStyle();
	Style.SetFont(ERUI::FontRegular(11, 20));
	Style.SetForegroundColor(FSlateColor(FLinearColor(0.96f, 0.96f, 0.94f, 1.0f)));
	Style.SetFocusedForegroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 0.98f, 1.0f)));
	Style.SetBackgroundColor(FSlateColor(FLinearColor::White));
	Style.SetBackgroundImageNormal(ERUI::RoundedBrush(0.0f, FLinearColor(0.012f, 0.014f, 0.019f, 0.98f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.28f)));
	Style.SetBackgroundImageHovered(ERUI::RoundedBrush(0.0f, FLinearColor(0.018f, 0.021f, 0.028f, 1.00f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.56f)));
	Style.SetBackgroundImageFocused(ERUI::RoundedBrush(0.0f, FLinearColor(0.018f, 0.021f, 0.028f, 1.00f), 1.2f, T->Accent));
	Style.SetPadding(FMargin(10.0f, 6.0f));
	Box->SetWidgetStyle(Style);
	Box->SetHintText(Hint);
	Box->SetIsPassword(bIsPassword);
}

void UERAICommandPanel::BindService(UBattleAICommandService* InService)
{
	if (Service.Get() == InService) { return; }

	if (UBattleAICommandService* Old = Service.Get())
	{
		if (LogDelegateHandle.IsValid())   { Old->OnLogEntry.Remove(LogDelegateHandle); }
		if (StateDelegateHandle.IsValid()) { Old->OnStateChanged.Remove(StateDelegateHandle); }
	}
	LogDelegateHandle.Reset();
	StateDelegateHandle.Reset();

	Service = InService;
	if (InService)
	{
		LogDelegateHandle = InService->OnLogEntry.AddUObject(this, &UERAICommandPanel::HandleServiceLog);
		StateDelegateHandle = InService->OnStateChanged.AddUObject(this, &UERAICommandPanel::HandleServiceStateChanged);
		SyncConfigFieldsFromService();
		UpdateBusyUI(InService->IsBusy(), InService->GetStatusLabel());
		RefreshUnitIndexBar();
	}
}

void UERAICommandPanel::OnPanelOpened()
{
	SyncConfigFieldsFromService();
	RefreshUnitIndexBar();

	OpenTrack.Trigger(0.46f);
	CaptionCipher.Start(TEXT("战术神经链路"), 0.36f, 0.04f);

	if (Service.IsValid() && !Service->HasApiKey() && !bSettingsExpanded)
	{
		bSettingsExpanded = true;
		if (SettingsDrawer)
		{
			SettingsDrawer->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void UERAICommandPanel::SyncConfigFieldsFromService()
{
	const UBattleAICommandService* Svc = Service.Get();
	if (!Svc) return;
	const FAICommandConfig& Cfg = Svc->GetConfig();
	if (EndpointInput) { EndpointInput->SetText(FText::FromString(Cfg.EndpointUrl)); }
	if (ModelInput)    { ModelInput->SetText(FText::FromString(Cfg.ModelName)); }
	if (ApiKeyInput)   { ApiKeyInput->SetText(FText::FromString(Cfg.ApiKey)); }
}

void UERAICommandPanel::RefreshUnitIndexBar()
{
	if (!UnitIndexText) return;
	if (const UBattleAICommandService* Svc = Service.Get())
	{
		UnitIndexText->SetText(FText::FromString(Svc->GetUnitIndexSummary()));
	}
}

void UERAICommandPanel::UpdateBusyUI(bool bBusy, const FString& StatusLabel)
{
	const UERUITheme* T = ERUI::Theme();
	const bool bReady = Service.IsValid() && Service->HasApiKey();

	if (StatusBadgeBorder)
	{
		const FLinearColor PillCol = bBusy ? T->Accent : (bReady ? T->AllyMark : FLinearColor(0.92f, 0.92f, 0.90f, 0.24f));
		StatusBadgeBorder->SetBrush(ERUI::RoundedBrush(0.0f, PillCol));
		StatusBadgeBorder->SetBrushColor(FLinearColor::White);
	}
	if (StatusBadgeText)
	{
		StatusBadgeText->SetText(FText::FromString(StatusLabel));
		StatusBadgeText->SetColorAndOpacity(FSlateColor((bBusy || bReady) ? T->PureInk : T->Bone));
	}
	if (SendButtonBg)
	{
		SendButtonBg->SetBrush(ERUI::RoundedBrush(0.0f, bBusy ? T->DangerColor : T->Bone));
		SendButtonBg->SetBrushColor(FLinearColor::White);
	}
	if (SendButtonText)
	{
		SendButtonText->SetText(FText::FromString(bBusy ? TEXT("中止") : TEXT("执行")));
		SendButtonText->SetColorAndOpacity(FSlateColor(bBusy ? T->Bone : T->PureInk));
	}
	if (UERActionButton* ToggleBtn = LinkedToggleButton.Get())
	{
		ToggleBtn->SetUrgent(bBusy);
		if (bBusy)
		{
			ToggleBtn->SetLabel(INVTEXT("推演中"));
		}
		else
		{
			ToggleBtn->SetLabel(bReady ? INVTEXT("战术链路 · 就绪") : INVTEXT("战术链路"));
		}
	}
}

void UERAICommandPanel::AppendLogLine(EERAICommandLogKind Kind, const FString& Text)
{
	if (!LogScrollBox) return;
	const UERUITheme* T = ERUI::Theme();

	FString Tag;
	FLinearColor TagBgCol = FLinearColor(0.92f, 0.92f, 0.90f, 0.16f);
	FLinearColor TagTxtCol = T->Bone;
	FLinearColor BodyCol = T->BoneSecondary;

	switch (Kind)
	{
	case EERAICommandLogKind::User:
		Tag = TEXT("CMD");
		TagBgCol = T->Bone;
		TagTxtCol = T->PureInk;
		BodyCol = T->Bone;
		break;
	case EERAICommandLogKind::ToolCall:
		Tag = TEXT("工具");
		TagBgCol = T->AllyMark;
		TagTxtCol = T->PureInk;
		BodyCol = T->Bone;
		break;
	case EERAICommandLogKind::ToolResult:
		Tag = TEXT("RET");
		TagBgCol = FLinearColor(0.92f, 0.92f, 0.90f, 0.14f);
		TagTxtCol = T->BoneSecondary;
		BodyCol = T->BoneTertiary;
		break;
	case EERAICommandLogKind::Assistant:
		Tag = TEXT("战术链路");
		TagBgCol = T->Accent;
		TagTxtCol = T->PureInk;
		BodyCol = T->Bone;
		break;
	case EERAICommandLogKind::Error:
		Tag = TEXT("ERR");
		TagBgCol = T->DangerColor;
		TagTxtCol = T->Bone;
		BodyCol = T->DangerColor;
		break;
	default:
		Tag = TEXT("SYS");
		TagBgCol = FLinearColor(0.92f, 0.92f, 0.90f, 0.14f);
		TagTxtCol = T->BoneSecondary;
		BodyCol = T->BoneSecondary;
		break;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UBorder* TagBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	TagBox->SetBrush(ERUI::RoundedBrush(0.0f, TagBgCol));
	TagBox->SetBrushColor(FLinearColor::White);
	TagBox->SetPadding(FMargin(6.0f, 2.0f));
	UTextBlock* TagText = ERUI::MakeText(this, FText::FromString(Tag), EERTypeScale::Micro, TagTxtCol, true);
	TagText->SetFont(ERUI::FontBold(8, 120));
	TagBox->SetContent(TagText);
	UHorizontalBoxSlot* TSlot = Row->AddChildToHorizontalBox(TagBox);
	TSlot->SetVerticalAlignment(VAlign_Top);
	TSlot->SetPadding(FMargin(0.0f, 1.0f, 8.0f, 0.0f));

	UTextBlock* Entry = ERUI::MakeText(this, FText::FromString(Text), EERTypeScale::Caption, BodyCol);
	Entry->SetFont(ERUI::FontRegular(11, 20));
	Entry->SetAutoWrapText(true);
	UHorizontalBoxSlot* ESlot = Row->AddChildToHorizontalBox(Entry);
	ESlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ESlot->SetVerticalAlignment(VAlign_Center);

	UScrollBoxSlot* S = Cast<UScrollBoxSlot>(LogScrollBox->AddChild(Row));
	if (S)
	{
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}
	LogScrollBox->ScrollToEnd();
}

void UERAICommandPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (OpenTrack.Tick(InDeltaTime))
	{
		const float E = OpenTrack.Expo(0.00f, 0.36f);
		const float DivE = OpenTrack.Expo(0.08f, 0.36f);
		SetRenderOpacity(E);
		SetRenderTranslation(FVector2D(18.0f * (1.0f - E), 0.0f));
		if (PanelFrame)    { PanelFrame->SetMotionState(FMath::Lerp(0.45f, 1.0f, E), 0.0f, E < 0.99f ? E : 1.0f); }
		if (HeaderDivider) { HeaderDivider->SetRevealAlpha(DivE); }
	}

	FString Decoded;
	if (CaptionCipher.Tick(InDeltaTime, Decoded) && HeaderCaptionText)
	{
		HeaderCaptionText->SetText(FText::FromString(Decoded));
	}

	if (GetVisibility() == ESlateVisibility::Visible)
	{
		IndexRefreshAccum += InDeltaTime;
		if (IndexRefreshAccum >= 0.35f)
		{
			IndexRefreshAccum = 0.0f;
			RefreshUnitIndexBar();
		}
	}
}

bool UERAICommandPanel::IsTextInputFocused() const
{
	if (GetVisibility() != ESlateVisibility::Visible) { return false; }
	return (CommandInput && CommandInput->HasKeyboardFocus())
		|| (EndpointInput && EndpointInput->HasKeyboardFocus())
		|| (ModelInput && ModelInput->HasKeyboardFocus())
		|| (ApiKeyInput && ApiKeyInput->HasKeyboardFocus());
}

bool UERAICommandPanel::IsInteractiveHovered() const
{
	if (GetVisibility() != ESlateVisibility::Visible) { return false; }
	if (IsTextInputFocused()) { return true; }
	if (FSlateApplication::IsInitialized())
	{
		const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
		if (GetCachedGeometry().IsUnderLocation(CursorPos)) { return true; }
	}
	return RootBorder && RootBorder->IsHovered();
}

UWidget* UERAICommandPanel::GetHoverCheckWidget() const
{
	return RootBorder.Get();
}

void UERAICommandPanel::HandleToggleButtonClicked()
{
	const bool bOpening = (GetVisibility() != ESlateVisibility::Visible);
	SetVisibility(bOpening ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bOpening)
	{
		OnPanelOpened();
	}
}

void UERAICommandPanel::HandleToggleSettingsClicked()
{
	bSettingsExpanded = !bSettingsExpanded;
	if (SettingsDrawer)
	{
		SettingsDrawer->SetVisibility(bSettingsExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UERAICommandPanel::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UERAICommandPanel::HandleSaveConfigClicked()
{
	UBattleAICommandService* Svc = Service.Get();
	if (!Svc) return;

	FAICommandConfig NewCfg = Svc->GetConfig();
	if (EndpointInput) { NewCfg.EndpointUrl = EndpointInput->GetText().ToString(); }
	if (ModelInput)    { NewCfg.ModelName   = ModelInput->GetText().ToString(); }
	if (ApiKeyInput)   { NewCfg.ApiKey      = ApiKeyInput->GetText().ToString(); }
	Svc->SetConfig(NewCfg, true);

	AppendLogLine(EERAICommandLogKind::System, TEXT("API 配置已保存至本地 Saved/ER_AIConfig.json。"));

	if (Svc->HasApiKey())
	{
		bSettingsExpanded = false;
		if (SettingsDrawer) { SettingsDrawer->SetVisibility(ESlateVisibility::Collapsed); }
	}
}

void UERAICommandPanel::HandlePresetDeepSeekClicked()
{
	if (UBattleAICommandService* Svc = Service.Get()) { Svc->ApplyPreset(TEXT("DeepSeek")); SyncConfigFieldsFromService(); }
}

void UERAICommandPanel::HandlePresetOpenAIClicked()
{
	if (UBattleAICommandService* Svc = Service.Get()) { Svc->ApplyPreset(TEXT("OpenAI")); SyncConfigFieldsFromService(); }
}

void UERAICommandPanel::HandlePresetQwenClicked()
{
	if (UBattleAICommandService* Svc = Service.Get()) { Svc->ApplyPreset(TEXT("Qwen")); SyncConfigFieldsFromService(); }
}

void UERAICommandPanel::HandlePresetOllamaClicked()
{
	if (UBattleAICommandService* Svc = Service.Get()) { Svc->ApplyPreset(TEXT("Ollama")); SyncConfigFieldsFromService(); }
}

void UERAICommandPanel::HandleSendClicked()
{
	UBattleAICommandService* Svc = Service.Get();
	if (!Svc) return;

	if (Svc->IsBusy())
	{
		Svc->CancelActiveCommand();
		return;
	}

	if (CommandInput)
	{
		const FString Text = CommandInput->GetText().ToString();
		if (Svc->SendUserCommand(Text))
		{
			CommandInput->SetText(FText::GetEmpty());
		}
	}
}

void UERAICommandPanel::HandleNewContextClicked()
{
	if (UBattleAICommandService* Svc = Service.Get()) { Svc->ClearConversationContext(); }
}

void UERAICommandPanel::HandleClearLogClicked()
{
	if (LogScrollBox) { LogScrollBox->ClearChildren(); }
}

void UERAICommandPanel::HandleCommandCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter) { HandleSendClicked(); }
}

void UERAICommandPanel::HandleServiceLog(EERAICommandLogKind Kind, const FString& Text)
{
	AppendLogLine(Kind, Text);
}

void UERAICommandPanel::HandleServiceStateChanged(bool bBusy, const FString& StatusLabel)
{
	UpdateBusyUI(bBusy, StatusLabel);
	RefreshUnitIndexBar();
}
