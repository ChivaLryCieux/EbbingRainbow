// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHallHUD.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Core/ERLog.h"
#include "Hall/HallHUD.h"
#include "Hall/HallPlayerController.h"
#include "UI/ERActionButton.h"
#include "UI/ERHallButton.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Run/RunSubsystem.h"

TSharedRef<SWidget> UERHallHUD::RebuildWidget()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UERHallHUD::NativePreConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativePreConstruct();

	ApplyResonantLayoutStyle();
	if (IsDesignTime())
	{
		ApplyZenState(bPreviewZenMode);
	}
}

void UERHallHUD::NativeConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativeConstruct();

	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		Campaign->OnCampaignUpdated.AddUObject(this, &UERHallHUD::RefreshCampaignSaveNotice);
	}
	RefreshCampaignSaveNotice();

	// 标题主菜单 (按钮变量名保留旧蓝图兼容绑定; 文案/行为已是新游戏流程)
	if (PersonnelButton)
	{
		PersonnelButton->SetButtonTexts(INVTEXT("继续游戏"), FText::GetEmpty());
		PersonnelButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandlePersonnelClicked);
	}
	if (RoutesButton)
	{
		RoutesButton->SetButtonTexts(INVTEXT("开始新游戏"), FText::GetEmpty());
		RoutesButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleRoutesClicked);
	}
	if (SituationButton)
	{
		SituationButton->SetButtonTexts(INVTEXT("读取存档"), FText::GetEmpty());
		SituationButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleSituationClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->SetButtonTexts(INVTEXT("设置"), FText::GetEmpty());
		SettingsButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleSettingsClicked);
	}

	ApplyResonantLayoutStyle();

	if (!bBaseCached)
	{
		bBaseCached = true;
		if (NavBox)
		{
			BaseDockTranslation = NavBox->GetRenderTransform().Translation;
			BaseDockOpacity = NavBox->GetRenderOpacity();
		}
		if (MastheadContainer)
		{
			BaseMastheadTranslation = MastheadContainer->GetRenderTransform().Translation;
		}
	}

	RefreshTitleMenuState();

	const FString SloganTag = TEXT("虹膜基金会 · 银河军事指挥服务");
	const FString SloganSub = TEXT("我们面向全银河提供军事指挥服务");
	if (MastheadTagText)
	{
		MastheadTagText->SetText(FText::FromString(SloganTag));
	}
	if (MastheadMetaText)
	{
		MastheadMetaText->SetText(FText::FromString(SloganSub));
	}

	ApplyZenState(false);

	if (bEnableEntranceAnimation && !IsDesignTime())
	{
		EntranceTrack.Trigger(FMath::Max(0.45f, EntranceDuration));
		MastheadTagCipher.Start(SloganTag, 0.42f, 0.08f);
		MastheadMetaCipher.FinishImmediately();
		if (PersonnelButton) { PersonnelButton->TriggerEntrancePulse(0.26f); }
		if (RoutesButton)    { RoutesButton->TriggerEntrancePulse(0.36f); }
		if (SituationButton) { SituationButton->TriggerEntrancePulse(0.46f); }
		if (SettingsButton)  { SettingsButton->TriggerEntrancePulse(0.56f); }
		ApplyEntrancePose();
	}
	else
	{
		EntranceTrack.FinishImmediately();
		ApplyEntrancePose();
	}

	if (Anim_Intro)
	{
		PlayAnimationForward(Anim_Intro);
	}
}

void UERHallHUD::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();

	if (!RootCanvas)
	{
		if (UCanvasPanel* Existing = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			RootCanvas = Existing;
		}
		else if (!WidgetTree->RootWidget)
		{
			RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			WidgetTree->RootWidget = RootCanvas;
		}
	}
	if (!RootCanvas)
	{
		return;
	}
	RootCanvas->SetVisibility(ESlateVisibility::Visible);

	UCanvasPanel* DashCanvas = Cast<UCanvasPanel>(DashboardContainer);
	if (!DashCanvas)
	{
		DashCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DashboardContainer"));
		DashCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* DS = RootCanvas->AddChildToCanvas(DashCanvas);
		DS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DS->SetOffsets(FMargin(0.0f));
		DashboardContainer = DashCanvas;
	}

	// 1. 全屏四角静谧光学标尺
	if (!CornerReticle)
	{
		CornerReticle = WidgetTree->ConstructWidget<UERCornerReticle>(UERCornerReticle::StaticClass(), TEXT("CornerReticle"));
		CornerReticle->TopRightCode = TEXT("");
		CornerReticle->BottomRightCode = TEXT("");
		CornerReticle->ReticleColor = FLinearColor(0.95f, 0.95f, 0.92f, 0.46f);
		CornerReticle->ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);
		CornerReticle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* RS = DashCanvas->AddChildToCanvas(CornerReticle);
		RS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		RS->SetOffsets(FMargin(0.0f));
	}

	// 2. 左上《控制:共振》式巨型粗体标题碑 (黑曜石切角背板确保在明亮金色圣殿上锐利夺目)
	if (!MastheadContainer)
	{
		UOverlay* MastOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MastheadContainer"));
		MastOv->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		MastheadContainer = MastOv;

		MastheadFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("MastheadFrame"));
		MastheadFrame->Chamfer = FVector4(0.0f, 0.0f, 18.0f, 0.0f);
		MastheadFrame->FillColor = FLinearColor(0.006f, 0.007f, 0.010f, 0.88f);
		MastheadFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.16f);
		MastheadFrame->bDrawCornerBrackets = true;
		MastheadFrame->BracketLength = 10.0f;
		MastheadFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.68f);
		MastheadFrame->AccentEdge = EERAccentEdge::Left;
		MastheadFrame->bUsePrismAccent = true;
		MastheadFrame->AccentThickness = 4.0f;
		MastheadFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = MastOv->AddChildToOverlay(MastheadFrame);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* MCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MastheadCol"));
		MCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = MastOv->AddChildToOverlay(MCol);
			S->SetPadding(FMargin(24.0f, 16.0f, 34.0f, 16.0f));
		}

		MastheadTagText = ERUI::MakeText(this, INVTEXT("虹膜基金会 · 银河军事指挥服务"), EERTypeScale::Micro, T->BoneSecondary, false);
		MCol->AddChildToVerticalBox(MastheadTagText);

		MastheadTitleText = ERUI::MakeText(this, INVTEXT("虹不我与"), EERTypeScale::Hero, T->Bone);
		MCol->AddChildToVerticalBox(MastheadTitleText);

		MastheadDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("MastheadDivider"));
		MastheadDivider->SetVisibility(ESlateVisibility::Collapsed);
		MCol->AddChildToVerticalBox(MastheadDivider);

		MastheadMetaText = ERUI::MakeText(this, INVTEXT("我们面向全银河提供军事指挥服务"), EERTypeScale::Micro, T->BoneSecondary, false);
		MCol->AddChildToVerticalBox(MastheadMetaText);

		UCanvasPanelSlot* MS = DashCanvas->AddChildToCanvas(MastOv);
		MS->SetAnchors(FAnchors(0.0f, 0.0f));
		MS->SetPosition(FVector2D(72.0f, 58.0f));
		MS->SetAutoSize(true);
	}

	// 3. 左下纯白排印纵向导航: 继续游戏 / 开始新游戏 / 读取存档 / 设置
	//    (旧蓝图设计器里的底部双按钮 DockBox 仅作兼容绑定, 折叠隐藏)
	if (DockBox)
	{
		DockBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!NavBox)
	{
		NavBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NavBox"));
		NavBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* DockSlot = DashCanvas->AddChildToCanvas(NavBox);
		DockSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		DockSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		DockSlot->SetPosition(FVector2D(64.0f, -82.0f));
		DockSlot->SetAutoSize(true);

		auto MakeNavButton = [&](const TCHAR* Name, const FText& Code) -> UERHallButton*
		{
			UERHallButton* B = WidgetTree->ConstructWidget<UERHallButton>(UERHallButton::StaticClass(), Name);
			B->IndexCode = Code;
			B->AccentColor = T->Accent;
			B->bPrimary = true;
			return B;
		};

		PersonnelButton = MakeNavButton(TEXT("NavPersonnelButton"), INVTEXT("01"));
		RoutesButton    = MakeNavButton(TEXT("NavRoutesButton"), INVTEXT("02"));
		SituationButton = MakeNavButton(TEXT("NavSituationButton"), INVTEXT("03"));
		SettingsButton  = MakeNavButton(TEXT("NavSettingsButton"), INVTEXT("04"));

		UERHallButton* NavButtons[] = { PersonnelButton, RoutesButton, SituationButton, SettingsButton };
		for (UERHallButton* Btn : NavButtons)
		{
			NavBox->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}

		MenuStatusText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, false);
		MenuStatusText->SetAutoWrapText(true);
		NavBox->AddChildToVerticalBox(MenuStatusText)->SetPadding(FMargin(14.0f, 6.0f, 0.0f, 0.0f));
	}

	// 4. 观景模式提示条 (无底卡纯白字 + 错位黑影)
	if (!ZenHintBorder)
	{
		ZenHintBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ZenHintBorder"));
		ZenHintBorder->SetBrush(ERUI::TransparentBrush());
		ZenHintBorder->SetPadding(FMargin(0.0f));
		ZenHintBorder->SetVisibility(ESlateVisibility::Hidden);

		ZenHintText = ERUI::MakeText(this, ZenHintMessage, EERTypeScale::Micro, T->BoneSecondary, false);
		ZenHintBorder->SetContent(ZenHintText);

		UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(ZenHintBorder);
		HintSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		HintSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		HintSlot->SetPosition(FVector2D(0.0f, -32.0f));
		HintSlot->SetAutoSize(true);
	}

	// 右上银河战役存档提示 (无底卡纯白字 + 错位黑影, 正常时折叠)
	if (!CampaignSaveNoticeBorder)
	{
		CampaignSaveNoticeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CampaignSaveNoticeBorder"));
		CampaignSaveNoticeBorder->SetBrush(ERUI::TransparentBrush());
		CampaignSaveNoticeBorder->SetPadding(FMargin(0.0f));
		CampaignSaveNoticeBorder->SetVisibility(ESlateVisibility::Collapsed);

		CampaignSaveNoticeText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, false);
		CampaignSaveNoticeText->SetAutoWrapText(true);
		CampaignSaveNoticeBorder->SetContent(CampaignSaveNoticeText);

		UCanvasPanelSlot* NoticeSlot = RootCanvas->AddChildToCanvas(CampaignSaveNoticeBorder);
		NoticeSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		NoticeSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		NoticeSlot->SetPosition(FVector2D(-72.0f, 58.0f));
		NoticeSlot->SetAutoSize(true);
	}

	// 读取存档抽屉: 无底卡建筑竖线 + 纯白错位阴影列表
	if (!LoadMenuBorder)
	{
		LoadMenuBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LoadMenuBorder"));
		LoadMenuBorder->SetBrush(ERUI::TransparentBrush());
		LoadMenuBorder->SetPadding(FMargin(0.0f));
		LoadMenuBorder->SetVisibility(ESlateVisibility::Collapsed);

		UHorizontalBox* LoadWrap = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LoadMenuWrap"));
		LoadMenuBorder->SetContent(LoadWrap);

		UImage* SepLine = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LoadMenuSepLine"));
		SepLine->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 1.0f)));
		SepLine->SetColorAndOpacity(FLinearColor(0.99f, 0.99f, 0.98f, 0.35f));
		{
			UHorizontalBoxSlot* SS = LoadWrap->AddChildToHorizontalBox(SepLine);
			SS->SetVerticalAlignment(VAlign_Fill);
			SS->SetPadding(FMargin(0.0f, 4.0f, 20.0f, 4.0f));
		}

		UVerticalBox* LoadCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadMenuCol"));
		LoadWrap->AddChildToHorizontalBox(LoadCol);

		UTextBlock* LoadTitle = ERUI::MakeText(this, INVTEXT("选择战役检查点"), EERTypeScale::Title, T->Bone, false);
		ERUI::ApplyResonantText(LoadTitle, 17, true, 140, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), FVector2D(2.0f, 2.5f), 0.86f);
		LoadCol->AddChildToVerticalBox(LoadTitle)->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 10.0f));

		LoadSlotListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadSlotListBox"));
		LoadCol->AddChildToVerticalBox(LoadSlotListBox);

		LoadMenuStatusText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, false);
		ERUI::ApplyResonantText(LoadMenuStatusText, 13, false, 80, FLinearColor(0.99f, 0.99f, 0.98f, 0.78f), FVector2D(1.5f, 2.0f), 0.86f);
		LoadMenuStatusText->SetAutoWrapText(true);
		LoadCol->AddChildToVerticalBox(LoadMenuStatusText)->SetPadding(FMargin(4.0f, 6.0f, 0.0f, 8.0f));

		UERActionButton* BackBtn = UERActionButton::Make(this, INVTEXT("返回"), 17, false);
		BackBtn->SetResonantHudStyle(true, true);
		BackBtn->SetMinSize(140.0f, 0.0f);
		BackBtn->SetNativeClick([this]() { HideLoadMenu(); });
		LoadCol->AddChildToVerticalBox(BackBtn);

		UCanvasPanelSlot* LoadSlot = RootCanvas->AddChildToCanvas(LoadMenuBorder);
		LoadSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		LoadSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		LoadSlot->SetPosition(FVector2D(304.0f, -82.0f));
		LoadSlot->SetAutoSize(true);
	}

	// 第一人称准星与交互提示均挂在 RootCanvas, 不受标题菜单 Dashboard 隐藏影响
	if (!CrosshairDot)
	{
		CrosshairDot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CrosshairDot"));
		CrosshairDot->SetBrush(ERUI::RoundedBrush(2.0f, FLinearColor(0.96f, 0.97f, 1.0f, 0.80f), 0.0f, FLinearColor::Transparent));
		CrosshairDot->SetVisibility(ESlateVisibility::Hidden);
		UCanvasPanelSlot* CrossSlot = RootCanvas->AddChildToCanvas(CrosshairDot);
		CrossSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CrossSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CrossSlot->SetSize(FVector2D(4.0f, 4.0f));
	}

	if (!InteractionPromptBorder)
	{
		InteractionPromptBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InteractionPromptBorder"));
		InteractionPromptBorder->SetBrush(ERUI::TransparentBrush());
		InteractionPromptBorder->SetPadding(FMargin(0.0f));
		InteractionPromptBorder->SetVisibility(ESlateVisibility::Hidden);
		InteractionPromptText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->Bone, false);
		InteractionPromptBorder->SetContent(InteractionPromptText);

		UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(InteractionPromptBorder);
		PromptSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		PromptSlot->SetPosition(FVector2D(0.0f, -118.0f));
		PromptSlot->SetAutoSize(true);
	}

	ApplyResonantLayoutStyle();
}

void UERHallHUD::ApplyResonantLayoutStyle()
{
	if (CornerReticle)   { CornerReticle->SetVisibility(ESlateVisibility::Collapsed); }
	if (MastheadFrame)   { MastheadFrame->SetVisibility(ESlateVisibility::Collapsed); }
	if (MastheadDivider) { MastheadDivider->SetVisibility(ESlateVisibility::Collapsed); }
	if (DockBox)         { DockBox->SetVisibility(ESlateVisibility::Collapsed); }

	if (MastheadContainer)
	{
		if (UCanvasPanelSlot* MS = Cast<UCanvasPanelSlot>(MastheadContainer->Slot))
		{
			MS->SetAnchors(FAnchors(0.0f, 0.0f));
			MS->SetAlignment(FVector2D(0.0f, 0.0f));
			MS->SetPosition(FVector2D(72.0f, 58.0f));
			MS->SetAutoSize(true);
		}
		if (MastheadTitleText)
		{
			if (UWidget* ParentCol = MastheadTitleText->GetParent())
			{
				if (UOverlaySlot* ColSlot = Cast<UOverlaySlot>(ParentCol->Slot))
				{
					ColSlot->SetPadding(FMargin(0.0f));
				}
			}
		}
	}

	if (MastheadTagText)
	{
		ERUI::ApplyResonantText(MastheadTagText, 13, false, 260,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.80f), FVector2D(1.5f, 2.0f), 0.84f);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(MastheadTagText->Slot))
		{
			S->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 6.0f));
		}
	}
	if (MastheadTitleText)
	{
		MastheadTitleText->SetText(INVTEXT("虹不我与"));
		ERUI::ApplyResonantText(MastheadTitleText, 124, true, 160,
			FLinearColor(0.99f, 0.99f, 0.98f, 1.00f), FVector2D(5.0f, 7.0f), 0.80f);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(MastheadTitleText->Slot))
		{
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
	}
	if (MastheadMetaText)
	{
		ERUI::ApplyResonantText(MastheadMetaText, 15, false, 220,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.88f), FVector2D(1.5f, 2.5f), 0.86f);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(MastheadMetaText->Slot))
		{
			S->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	if (NavBox)
	{
		if (UCanvasPanelSlot* DockSlot = Cast<UCanvasPanelSlot>(NavBox->Slot))
		{
			DockSlot->SetAnchors(FAnchors(0.0f, 1.0f));
			DockSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			DockSlot->SetPosition(FVector2D(64.0f, -82.0f));
			DockSlot->SetAutoSize(true);
		}
	}
	if (MenuStatusText)
	{
		ERUI::ApplyResonantText(MenuStatusText, 13, false, 100,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.80f), FVector2D(1.5f, 2.0f), 0.86f);
	}

	if (ZenHintBorder)
	{
		ZenHintBorder->SetBrush(ERUI::TransparentBrush());
		ZenHintBorder->SetPadding(FMargin(0.0f));
	}
	if (ZenHintText)
	{
		FText HintText = ZenHintMessage;
		if (HintText.ToString().Contains(TEXT("ZEN MODE")))
		{
			HintText = INVTEXT("观景模式 · 点击任意位置或按 ESC 返回");
		}
		if (!HintText.IsEmpty())
		{
			ZenHintText->SetText(HintText);
		}
		ERUI::ApplyResonantText(ZenHintText, 13, false, 180,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.82f), FVector2D(1.5f, 2.0f), 0.86f);
	}

	if (CampaignSaveNoticeBorder)
	{
		CampaignSaveNoticeBorder->SetBrush(ERUI::TransparentBrush());
		CampaignSaveNoticeBorder->SetPadding(FMargin(0.0f));
	}
	if (CampaignSaveNoticeText)
	{
		ERUI::ApplyResonantText(CampaignSaveNoticeText, 13, false, 100,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.86f), FVector2D(1.5f, 2.0f), 0.86f);
	}

	if (InteractionPromptBorder)
	{
		InteractionPromptBorder->SetBrush(ERUI::TransparentBrush());
		InteractionPromptBorder->SetPadding(FMargin(0.0f));
	}
	if (InteractionPromptText)
	{
		ERUI::ApplyResonantText(InteractionPromptText, 19, true, 160,
			FLinearColor(0.99f, 0.99f, 0.98f, 0.98f), FVector2D(2.0f, 3.0f), 0.90f);
	}
}

void UERHallHUD::ShowTitleMenu()
{
	bFirstPersonMode = false;
	HideLoadMenu();
	if (DashboardContainer)
	{
		DashboardContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (CrosshairDot) { CrosshairDot->SetVisibility(ESlateVisibility::Hidden); }
	SetInteractionPromptVisible(false);
	RefreshTitleMenuState();
}

void UERHallHUD::SetTitleMenuHidden(bool bHidden)
{
	if (bHidden)
	{
		HideLoadMenu();
	}
	if (DashboardContainer)
	{
		DashboardContainer->SetVisibility(
			bHidden ? ESlateVisibility::Hidden : (bFirstPersonMode ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible));
	}
}

void UERHallHUD::EnterFirstPersonMode()
{
	bFirstPersonMode = true;
	HideLoadMenu();
	if (DashboardContainer)
	{
		DashboardContainer->SetVisibility(ESlateVisibility::Hidden);
	}
	if (CrosshairDot) { CrosshairDot->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (ZenHintBorder) { ZenHintBorder->SetVisibility(ESlateVisibility::Hidden); }
}

void UERHallHUD::SetInteractionPromptVisible(bool bVisible, const FText& PromptText)
{
	if (InteractionPromptText)
	{
		InteractionPromptText->SetText(PromptText);
	}
	if (InteractionPromptBorder)
	{
		InteractionPromptBorder->SetVisibility(bVisible && bFirstPersonMode && !PromptText.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UERHallHUD::SetMenuStatus(const FText& Message, bool bError)
{
	if (!MenuStatusText) { return; }
	MenuStatusText->SetText(Message);
	MenuStatusText->SetColorAndOpacity(FSlateColor(bError
		? FLinearColor(1.0f, 0.34f, 0.24f, 0.95f)
		: ERUI::Theme()->BoneSecondary));
}

void UERHallHUD::RefreshTitleMenuState()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	const bool bCanContinue = Campaign && Campaign->CanContinueCampaign();
	if (PersonnelButton)
	{
		PersonnelButton->SetIsEnabled(bCanContinue);
		PersonnelButton->SetRenderOpacity(1.0f);
	}
	if (MenuStatusText)
	{
		const FText NoSaveText = INVTEXT("未检测到可用战役存档");
		if (!bCanContinue)
		{
			SetMenuStatus(NoSaveText, true);
		}
		else if (MenuStatusText->GetText().EqualTo(NoSaveText))
		{
			SetMenuStatus(FText::GetEmpty());
		}
	}
}

void UERHallHUD::ShowLoadMenu()
{
	if (bFirstPersonMode) { return; }
	RebuildLoadSlotList();
	if (LoadMenuBorder)
	{
		LoadMenuBorder->SetVisibility(ESlateVisibility::Visible);
	}
}

void UERHallHUD::HideLoadMenu()
{
	if (LoadMenuBorder)
	{
		LoadMenuBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UERHallHUD::RebuildLoadSlotList()
{
	if (!LoadSlotListBox) { return; }
	LoadSlotListBox->ClearChildren();
	if (LoadMenuStatusText) { LoadMenuStatusText->SetText(FText::GetEmpty()); }

	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	TArray<FGalacticCampaignSaveSlotInfo> Infos = Campaign ? Campaign->GetSaveSlotInfos() : TArray<FGalacticCampaignSaveSlotInfo>();
	Infos.Sort([](const FGalacticCampaignSaveSlotInfo& A, const FGalacticCampaignSaveSlotInfo& B)
	{
		return A.Generation > B.Generation;
	});

	int32 ValidCount = 0;
	for (const FGalacticCampaignSaveSlotInfo& Info : Infos)
	{
		if (!Info.bValid) { continue; }
		++ValidCount;

		FString TimeText = TEXT("时间未知");
		if (Info.SavedAtUnixTime > 0)
		{
			TimeText = FDateTime::FromUnixTimestamp(Info.SavedAtUnixTime).ToString(TEXT("%Y-%m-%d %H:%M"));
		}
		const FString Label = FString::Printf(TEXT("检查点 %s · 第 %02d 周期 · %d 银河币 · 世代 %lld · %s"),
			Info.SlotIndex == 0 ? TEXT("A") : TEXT("B"), Info.CycleIndex, Info.Credits, Info.Generation, *TimeText);

		const int32 SlotIndex = Info.SlotIndex;
		UERActionButton* SlotButton = UERActionButton::Make(this, FText::FromString(Label), 16, true);
		SlotButton->SetResonantHudStyle(true, true);
		SlotButton->SetMinSize(560.0f, 0.0f);
		SlotButton->SetNativeClick([this, SlotIndex]() { HandleLoadSlotClicked(SlotIndex); });
		LoadSlotListBox->AddChildToVerticalBox(SlotButton)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	if (ValidCount == 0 && LoadMenuStatusText)
	{
		LoadMenuStatusText->SetText(INVTEXT("没有可读取的持久化战役检查点。PIE/命令行沙盘请直接开始新游戏。"));
	}
}

void UERHallHUD::HandleLoadSlotClicked(int32 SlotIndex)
{
	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		if (Campaign->LoadCampaignFromSlot(SlotIndex))
		{
			EnterFirstPersonFromMenu();
			return;
		}
	}
	SetMenuStatus(INVTEXT("读取该战役检查点失败"), true);
	RebuildLoadSlotList();
}

void UERHallHUD::EnterFirstPersonFromMenu()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallPlayerController* HallPC = Cast<AHallPlayerController>(PC))
		{
			HallPC->EnterFirstPerson();
			return;
		}
	}
	EnterFirstPersonMode();
}

void UERHallHUD::NativeDestruct()
{
	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		Campaign->OnCampaignUpdated.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UERHallHUD::RefreshCampaignSaveNotice()
{
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	const FText Notice = Campaign ? Campaign->GetSaveNotice() : FText::GetEmpty();
	if (CampaignSaveNoticeText)
	{
		CampaignSaveNoticeText->SetText(Notice);
	}
	if (CampaignSaveNoticeBorder)
	{
		CampaignSaveNoticeBorder->SetVisibility(Notice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UERHallHUD::ApplyEntrancePose()
{
	const float ReticleE = EntranceTrack.Expo(0.00f, 0.45f);
	const float MastE    = EntranceTrack.Expo(0.04f, 0.50f);
	const float DivE     = EntranceTrack.Expo(0.16f, 0.42f);
	const float DockE    = EntranceTrack.Quint(0.18f, 0.55f);

	if (CornerReticle)
	{
		CornerReticle->SetRevealAlpha(ReticleE);
	}
	if (MastheadContainer)
	{
		MastheadContainer->SetRenderOpacity(MastE);
		MastheadContainer->SetRenderTranslation(BaseMastheadTranslation + FVector2D(-18.0f * (1.0f - MastE), 0.0f));
	}
	if (MastheadFrame)
	{
		MastheadFrame->SetMotionState(MastE, 0.0f, MastE < 0.99f ? MastE : 1.0f);
	}
	if (MastheadDivider)
	{
		MastheadDivider->SetRevealAlpha(DivE);
	}
	if (NavBox)
	{
		NavBox->SetRenderOpacity(BaseDockOpacity * DockE);
		NavBox->SetRenderTranslation(BaseDockTranslation + FVector2D(0.0f, (1.0f - DockE) * EntranceOffsetY));
	}
}

void UERHallHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (EntranceTrack.Tick(InDeltaTime))
	{
		ApplyEntrancePose();
	}

	FString Decoded;
	if (MastheadTagCipher.Tick(InDeltaTime, Decoded) && MastheadTagText)
	{
		MastheadTagText->SetText(FText::FromString(Decoded));
	}
	if (MastheadMetaCipher.Tick(InDeltaTime, Decoded) && MastheadMetaText)
	{
		MastheadMetaText->SetText(FText::FromString(Decoded));
	}
}

FReply UERHallHUD::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bZenMode)
	{
		ToggleZenMode();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UERHallHUD::ToggleZenMode()
{
	SetZenMode(!bZenMode);
}

void UERHallHUD::SetZenMode(bool bInZenMode)
{
	bZenMode = bInZenMode;
	ApplyZenState(bZenMode);
	if (Anim_ZenEnter && !IsDesignTime())
	{
		PlayAnimation(Anim_ZenEnter, 0.0f, 1, bZenMode ? EUMGSequencePlayMode::Forward : EUMGSequencePlayMode::Reverse);
	}
	if (!IsDesignTime())
	{
		BP_OnZenModeChanged(bZenMode);
	}
}

void UERHallHUD::ApplyZenState(bool bInZen)
{
	if (DashboardContainer)
	{
		DashboardContainer->SetVisibility(bInZen || bFirstPersonMode ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
	else if (NavBox)
	{
		NavBox->SetVisibility(bInZen || bFirstPersonMode ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ZenHintBorder)
	{
		ZenHintBorder->SetVisibility(bInZen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
}

UWorld* UERHallHUD::ResolvePlayWorld() const
{
	if (UWorld* W = GetWorld())
	{
		return W;
	}
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				if (UWorld* Candidate = Context.World())
				{
					return Candidate;
				}
			}
		}
		return GEngine->GetCurrentPlayWorld();
	}
	return nullptr;
}

void UERHallHUD::LaunchRouteMap()
{
	UWorld* World = ResolvePlayWorld();
	if (!World)
	{
		UE_LOG(LogERHall, Error, TEXT("[Hall] LaunchRouteMap failed: could not resolve active UWorld!"));
		return;
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (URunSubsystem* Run = GameInstance->GetSubsystem<URunSubsystem>())
		{
			if (!Run->HasActiveRun())
			{
				Run->StartNewRandomRun();
			}
		}
	}

	UE_LOG(LogERHall, Log, TEXT("[Hall] Launching Route Map (%s)..."), *RouteLevelName.ToString());
	UGameplayStatics::OpenLevel(World, RouteLevelName, true, RouteLevelOptions);
}

void UERHallHUD::HandlePersonnelClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Continue Game clicked."));
	BP_OnPersonnelClicked();

	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		if (Campaign->ContinueLatestCampaign())
		{
			EnterFirstPersonFromMenu();
			return;
		}
	}
	SetMenuStatus(INVTEXT("无法读取最新战役存档"), true);
	RefreshTitleMenuState();
}

void UERHallHUD::HandleRoutesClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Start New Game clicked."));
	BP_OnRoutesClicked();

	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		if (Campaign->StartNewCampaign(2026))
		{
			EnterFirstPersonFromMenu();
			return;
		}
	}
	SetMenuStatus(INVTEXT("新战役初始化或存档写入失败"), true);
}

void UERHallHUD::HandleSituationClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Load Game clicked."));
	if (LoadMenuBorder && LoadMenuBorder->GetVisibility() == ESlateVisibility::Visible)
	{
		HideLoadMenu();
	}
	else
	{
		ShowLoadMenu();
	}
}

void UERHallHUD::HandleSettingsClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Settings clicked -> Galactic Panel (Tab 3)."));

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(PC->GetHUD()))
		{
			HallHUD->OpenGalacticPanel(3, true);
		}
	}
}
