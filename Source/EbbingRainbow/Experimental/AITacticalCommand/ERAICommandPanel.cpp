// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#include "Blueprint/WidgetTree.h"
#include "UI/ERActionButton.h"
#include "UI/ERUIStatics.h"

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
		ToggleBtn->Label = INVTEXT("AI 连接");
		ToggleBtn->bShowArrow = false;
		ToggleBtn->ContentPadding = FMargin(14.0f, 7.0f);
		UCanvasPanelSlot* BtnSlot = RootCanvas->AddChildToCanvas(ToggleBtn);
		BtnSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		BtnSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		BtnSlot->SetPosition(FVector2D(-ScreenMargin, 32.0f));
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
		PanelSlot->SetPosition(FVector2D(-ScreenMargin, 76.0f));
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

	if (SettingsToggleButton)
	{
		SettingsToggleButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleToggleSettingsClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleCloseClicked);
	}
	if (SaveConfigButton)
	{
		SaveConfigButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleSaveConfigClicked);
	}
	if (PresetDeepSeekBtn)
	{
		PresetDeepSeekBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetDeepSeekClicked);
	}
	if (PresetOpenAIBtn)
	{
		PresetOpenAIBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetOpenAIClicked);
	}
	if (PresetQwenBtn)
	{
		PresetQwenBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetQwenClicked);
	}
	if (PresetOllamaBtn)
	{
		PresetOllamaBtn->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandlePresetOllamaClicked);
	}
	if (SendButton)
	{
		SendButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleSendClicked);
	}
	if (NewContextButton)
	{
		NewContextButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleNewContextClicked);
	}
	if (ClearLogButton)
	{
		ClearLogButton->OnClicked.AddUniqueDynamic(this, &UERAICommandPanel::HandleClearLogClicked);
	}
	if (CommandInput)
	{
		CommandInput->OnTextCommitted.AddUniqueDynamic(this, &UERAICommandPanel::HandleCommandCommitted);
	}
}

void UERAICommandPanel::NativeDestruct()
{
	ActivePanels().RemoveAll([this](const TWeakObjectPtr<UERAICommandPanel>& P)
	{
		return !P.IsValid() || P.Get() == this;
	});

	if (UBattleAICommandService* Svc = Service.Get())
	{
		if (LogDelegateHandle.IsValid())
		{
			Svc->OnLogEntry.Remove(LogDelegateHandle);
		}
		if (StateDelegateHandle.IsValid())
		{
			Svc->OnStateChanged.Remove(StateDelegateHandle);
		}
	}
	LogDelegateHandle.Reset();
	StateDelegateHandle.Reset();
	Super::NativeDestruct();
}

void UERAICommandPanel::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(450.0f);
	PanelSize->SetMaxDesiredHeight(560.0f);
	WidgetTree->RootWidget = PanelSize;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetBrush(ERUI::WhiteBrush());
	RootBorder->SetBrushColor(FLinearColor(0.008f, 0.009f, 0.012f, 0.96f));
	RootBorder->SetPadding(FMargin(0.0f));
	PanelSize->AddChild(RootBorder);

	UOverlay* FrameOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FrameOverlay"));
	RootBorder->AddChild(FrameOverlay);

	UImage* OuterLine = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("OuterLine"));
	OuterLine->SetBrush(ERUI::HairlineBoxBrush(T->BoneGhost));
	OuterLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlaySlot* LineSlot = FrameOverlay->AddChildToOverlay(OuterLine);
	LineSlot->SetHorizontalAlignment(HAlign_Fill);
	LineSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* MainCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainCol"));
	UOverlaySlot* MainSlot = FrameOverlay->AddChildToOverlay(MainCol);
	MainSlot->SetPadding(FMargin(14.0f, 12.0f));

	// ---- 1. Header ----
	{
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Header"));
		MainCol->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UImage* AccentTick = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AccentTick"));
		AccentTick->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 16.0f)));
		AccentTick->SetColorAndOpacity(T->Accent);
		UHorizontalBoxSlot* TickSlot = Header->AddChildToHorizontalBox(AccentTick);
		TickSlot->SetVerticalAlignment(VAlign_Center);
		TickSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		UTextBlock* Title = ERUI::MakeText(this, INVTEXT("AI 战术链路"), EERTypeScale::Body, T->Bone);
		UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));

		StatusBadgeText = ERUI::MakeText(this, INVTEXT("未配置 Key"), EERTypeScale::Micro, T->Accent);
		UHorizontalBoxSlot* StatusSlot = Header->AddChildToHorizontalBox(StatusBadgeText);
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		SettingsToggleButton = MakeMiniButton(TEXT("API 设置"), T->BoneGhost, T->BoneSecondary, &SettingsToggleText);
		UHorizontalBoxSlot* SetSlot = Header->AddChildToHorizontalBox(SettingsToggleButton);
		SetSlot->SetVerticalAlignment(VAlign_Center);
		SetSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

		CloseButton = MakeMiniButton(TEXT("×"), T->BoneGhost, T->BoneSecondary);
		Header->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);
	}

	// ---- 2. Collapsible API Config Drawer ----
	{
		SettingsDrawer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsDrawer"));
		SettingsDrawer->SetVisibility(ESlateVisibility::Collapsed);
		MainCol->AddChildToVerticalBox(SettingsDrawer)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UHorizontalBox* PresetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PresetRow"));
		SettingsDrawer->AddChildToVerticalBox(PresetRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		UTextBlock* PresetCap = ERUI::MakeText(this, INVTEXT("预设:"), EERTypeScale::Micro, T->BoneTertiary);
		PresetRow->AddChildToHorizontalBox(PresetCap)->SetVerticalAlignment(VAlign_Center);

		auto AddPreset = [this, PresetRow, T](const FString& Label) -> UButton*
		{
			UButton* Btn = MakeMiniButton(Label, T->BoneGhost, T->BoneSecondary);
			UHorizontalBoxSlot* S = PresetRow->AddChildToHorizontalBox(Btn);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
			return Btn;
		};
		PresetDeepSeekBtn = AddPreset(TEXT("DeepSeek"));
		PresetOpenAIBtn = AddPreset(TEXT("OpenAI"));
		PresetQwenBtn = AddPreset(TEXT("千问"));
		PresetOllamaBtn = AddPreset(TEXT("Ollama"));

		auto AddConfigRow = [this, T](const FString& LabelStr, TObjectPtr<UEditableTextBox>& OutInput, const FText& Hint, bool bPassword)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			SettingsDrawer->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelBox->SetWidthOverride(48.0f);
			UTextBlock* Lbl = ERUI::MakeText(this, FText::FromString(LabelStr), EERTypeScale::Micro, T->BoneSecondary, true);
			LabelBox->AddChild(Lbl);
			Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

			UEditableTextBox* InputWidget = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			StyleInputBox(InputWidget, Hint, bPassword);
			OutInput = InputWidget;
			UHorizontalBoxSlot* InSlot = Row->AddChildToHorizontalBox(InputWidget);
			InSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			InSlot->SetVerticalAlignment(VAlign_Center);
		};

		AddConfigRow(TEXT("URL"), EndpointInput, INVTEXT("https://api.deepseek.com/v1/chat/completions"), false);
		AddConfigRow(TEXT("Model"), ModelInput, INVTEXT("deepseek-chat"), false);
		AddConfigRow(TEXT("Key"), ApiKeyInput, INVTEXT("sk-... (仅保存在本地 Saved/ER_AIConfig.json)"), true);

		UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		SettingsDrawer->AddChildToVerticalBox(SaveRow)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 4.0f));
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		SaveRow->AddChildToHorizontalBox(Sp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		SaveConfigButton = MakeMiniButton(TEXT("保存配置"), T->Accent, T->Accent);
		SaveRow->AddChildToHorizontalBox(SaveConfigButton);
	}

	// ---- 3. Live Unit Index Bar ----
	{
		UBorder* IndexBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("IndexBg"));
		IndexBg->SetBrush(ERUI::WhiteBrush());
		IndexBg->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.04f));
		IndexBg->SetPadding(FMargin(8.0f, 6.0f));
		MainCol->AddChildToVerticalBox(IndexBg)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UnitIndexText = ERUI::MakeText(this, INVTEXT("等待战场就绪..."), EERTypeScale::Caption, T->BoneSecondary);
		UnitIndexText->SetAutoWrapText(true);
		IndexBg->AddChild(UnitIndexText);
	}

	// ---- 4. Log Feed ----
	{
		USizeBox* LogHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LogHeight"));
		LogHeight->SetHeightOverride(220.0f);
		UVerticalBoxSlot* LogSlot = MainCol->AddChildToVerticalBox(LogHeight);
		LogSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LogSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		LogScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LogScrollBox"));
		LogHeight->AddChild(LogScrollBox);
	}

	// ---- 5. Command Input Row ----
	{
		UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InputRow"));
		MainCol->AddChildToVerticalBox(InputRow);

		CommandInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("CommandInput"));
		StyleInputBox(CommandInput, INVTEXT("输入指令，如：让当前棋子去攻击序号为1的敌人"), false);
		UHorizontalBoxSlot* InSlot = InputRow->AddChildToHorizontalBox(CommandInput);
		InSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InSlot->SetVerticalAlignment(VAlign_Center);
		InSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

		SendButton = MakeMiniButton(TEXT("执行"), T->Accent, T->Accent, &SendButtonText);
		UHorizontalBoxSlot* SendSlot = InputRow->AddChildToHorizontalBox(SendButton);
		SendSlot->SetVerticalAlignment(VAlign_Center);
		SendSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));

		NewContextButton = MakeMiniButton(TEXT("新对话"), T->BoneGhost, T->BoneSecondary);
		UHorizontalBoxSlot* CtxSlot = InputRow->AddChildToHorizontalBox(NewContextButton);
		CtxSlot->SetVerticalAlignment(VAlign_Center);
		CtxSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));

		ClearLogButton = MakeMiniButton(TEXT("清屏"), T->BoneGhost, T->BoneTertiary);
		InputRow->AddChildToHorizontalBox(ClearLogButton)->SetVerticalAlignment(VAlign_Center);
	}

	AppendLogLine(EERAICommandLogKind::System, TEXT("战术 AI 模块已就绪。支持多轮上下文追问，点击“新对话”可重置记忆。"));
}

UButton* UERAICommandPanel::MakeMiniButton(const FString& Label, const FLinearColor& StrokeTint, const FLinearColor& TextColor, TObjectPtr<UTextBlock>* OutLabel)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style;
	Style.SetNormal(ERUI::HairlineBoxBrush(StrokeTint));
	Style.SetHovered(ERUI::HairlineBoxBrush(ERUI::Theme()->Bone));
	Style.SetPressed(ERUI::HairlineBoxBrush(ERUI::Theme()->Accent));
	Style.SetNormalPadding(FMargin(8.0f, 4.0f));
	Style.SetPressedPadding(FMargin(8.0f, 4.0f));
	Btn->SetStyle(Style);

	UTextBlock* LabelWidget = ERUI::MakeText(this, FText::FromString(Label), EERTypeScale::Micro, TextColor);
	if (OutLabel)
	{
		*OutLabel = LabelWidget;
	}
	Btn->SetContent(LabelWidget);
	return Btn;
}

void UERAICommandPanel::StyleInputBox(UEditableTextBox* Box, const FText& Hint, bool bIsPassword)
{
	if (!Box) return;
	const UERUITheme* T = ERUI::Theme();

	FEditableTextBoxStyle Style = Box->GetWidgetStyle();
	Style.SetFont(ERUI::Font(EERTypeScale::Caption));
	Style.SetForegroundColor(FSlateColor(T->Bone));
	Style.SetFocusedForegroundColor(FSlateColor(T->Bone));
	Style.SetBackgroundColor(FSlateColor(FLinearColor::White));
	Style.SetBackgroundImageNormal(ERUI::HairlineBoxBrush(T->BoneGhost));
	Style.SetBackgroundImageHovered(ERUI::HairlineBoxBrush(T->BoneSecondary));
	Style.SetBackgroundImageFocused(ERUI::HairlineBoxBrush(T->Accent));
	Style.SetPadding(FMargin(8.0f, 5.0f));
	Box->SetWidgetStyle(Style);
	Box->SetHintText(Hint);
	Box->SetIsPassword(bIsPassword);
}

void UERAICommandPanel::BindService(UBattleAICommandService* InService)
{
	if (Service.Get() == InService)
	{
		return;
	}

	if (UBattleAICommandService* Old = Service.Get())
	{
		if (LogDelegateHandle.IsValid()) Old->OnLogEntry.Remove(LogDelegateHandle);
		if (StateDelegateHandle.IsValid()) Old->OnStateChanged.Remove(StateDelegateHandle);
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
	if (EndpointInput) EndpointInput->SetText(FText::FromString(Cfg.EndpointUrl));
	if (ModelInput) ModelInput->SetText(FText::FromString(Cfg.ModelName));
	if (ApiKeyInput) ApiKeyInput->SetText(FText::FromString(Cfg.ApiKey));
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
	if (StatusBadgeText)
	{
		StatusBadgeText->SetText(FText::FromString(StatusLabel));
		const bool bReady = Service.IsValid() && Service->HasApiKey();
		StatusBadgeText->SetColorAndOpacity(FSlateColor(bBusy ? T->Accent : (bReady ? T->AllyMark : T->BoneTertiary)));
	}
	if (SendButtonText)
	{
		SendButtonText->SetText(FText::FromString(bBusy ? TEXT("中止") : TEXT("执行")));
	}
	if (UERActionButton* ToggleBtn = LinkedToggleButton.Get())
	{
		ToggleBtn->SetUrgent(bBusy);
		if (bBusy)
		{
			ToggleBtn->SetLabel(INVTEXT("AI · 运行中"));
		}
		else
		{
			const bool bReady = Service.IsValid() && Service->HasApiKey();
			ToggleBtn->SetLabel(bReady ? INVTEXT("AI 连接 · 就绪") : INVTEXT("AI 连接"));
		}
	}
}

void UERAICommandPanel::AppendLogLine(EERAICommandLogKind Kind, const FString& Text)
{
	if (!LogScrollBox) return;
	const UERUITheme* T = ERUI::Theme();

	FString Prefix;
	FLinearColor Color = T->BoneSecondary;
	switch (Kind)
	{
	case EERAICommandLogKind::User:
		Prefix = TEXT("[指令] ");
		Color = T->Bone;
		break;
	case EERAICommandLogKind::ToolCall:
		Prefix = TEXT("[调用] ");
		Color = T->AllyMark;
		break;
	case EERAICommandLogKind::ToolResult:
		Prefix = TEXT("[回执] ");
		Color = T->BoneTertiary;
		break;
	case EERAICommandLogKind::Assistant:
		Prefix = TEXT("[AI] ");
		Color = T->Accent;
		break;
	case EERAICommandLogKind::Error:
		Prefix = TEXT("[错误] ");
		Color = T->DangerColor;
		break;
	default:
		Prefix = TEXT("[系统] ");
		Color = T->BoneSecondary;
		break;
	}

	UTextBlock* Entry = ERUI::MakeText(this, FText::FromString(Prefix + Text), EERTypeScale::Caption, Color);
	Entry->SetAutoWrapText(true);
	UScrollBoxSlot* S = Cast<UScrollBoxSlot>(LogScrollBox->AddChild(Entry));
	if (S)
	{
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}
	LogScrollBox->ScrollToEnd();
}

void UERAICommandPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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
	if (GetVisibility() != ESlateVisibility::Visible)
	{
		return false;
	}
	return (CommandInput && CommandInput->HasKeyboardFocus())
		|| (EndpointInput && EndpointInput->HasKeyboardFocus())
		|| (ModelInput && ModelInput->HasKeyboardFocus())
		|| (ApiKeyInput && ApiKeyInput->HasKeyboardFocus());
}

bool UERAICommandPanel::IsInteractiveHovered() const
{
	if (GetVisibility() != ESlateVisibility::Visible)
	{
		return false;
	}
	if (IsTextInputFocused())
	{
		return true;
	}
	if (FSlateApplication::IsInitialized())
	{
		const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
		if (GetCachedGeometry().IsUnderLocation(CursorPos))
		{
			return true;
		}
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
	if (EndpointInput) NewCfg.EndpointUrl = EndpointInput->GetText().ToString();
	if (ModelInput) NewCfg.ModelName = ModelInput->GetText().ToString();
	if (ApiKeyInput) NewCfg.ApiKey = ApiKeyInput->GetText().ToString();
	Svc->SetConfig(NewCfg, true);

	AppendLogLine(EERAICommandLogKind::System, TEXT("API 配置已保存至本地 Saved/ER_AIConfig.json。"));

	if (Svc->HasApiKey())
	{
		bSettingsExpanded = false;
		if (SettingsDrawer)
		{
			SettingsDrawer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UERAICommandPanel::HandlePresetDeepSeekClicked()
{
	if (UBattleAICommandService* Svc = Service.Get())
	{
		Svc->ApplyPreset(TEXT("DeepSeek"));
		SyncConfigFieldsFromService();
	}
}

void UERAICommandPanel::HandlePresetOpenAIClicked()
{
	if (UBattleAICommandService* Svc = Service.Get())
	{
		Svc->ApplyPreset(TEXT("OpenAI"));
		SyncConfigFieldsFromService();
	}
}

void UERAICommandPanel::HandlePresetQwenClicked()
{
	if (UBattleAICommandService* Svc = Service.Get())
	{
		Svc->ApplyPreset(TEXT("Qwen"));
		SyncConfigFieldsFromService();
	}
}

void UERAICommandPanel::HandlePresetOllamaClicked()
{
	if (UBattleAICommandService* Svc = Service.Get())
	{
		Svc->ApplyPreset(TEXT("Ollama"));
		SyncConfigFieldsFromService();
	}
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
	if (UBattleAICommandService* Svc = Service.Get())
	{
		Svc->ClearConversationContext();
	}
}

void UERAICommandPanel::HandleClearLogClicked()
{
	if (LogScrollBox)
	{
		LogScrollBox->ClearChildren();
	}
}

void UERAICommandPanel::HandleCommandCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleSendClicked();
	}
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
