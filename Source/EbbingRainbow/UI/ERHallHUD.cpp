// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERHallHUD.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Core/ERLog.h"
#include "Hall/HallHUD.h"
#include "UI/ERHallButton.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

	if (ZenHintText && !ZenHintMessage.IsEmpty())
	{
		ZenHintText->SetText(ZenHintMessage);
	}
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

	// 两个入口统一「白底墨字彩条」, 按钮上只有中文 (无英文副标)
	if (PersonnelButton)
	{
		PersonnelButton->SetButtonTexts(INVTEXT("兵团整备"), FText::GetEmpty());
		PersonnelButton->SetAccentAndPrimary(ERUI::Theme()->Accent, true);
		PersonnelButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandlePersonnelClicked);
	}
	if (RoutesButton)
	{
		RoutesButton->SetButtonTexts(INVTEXT("接取委托"), FText::GetEmpty());
		RoutesButton->SetAccentAndPrimary(ERUI::Theme()->Accent, true);
		RoutesButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleRoutesClicked);
	}
	if (SituationButton)
	{
		SituationButton->SetButtonTexts(INVTEXT("银河局势"), FText::GetEmpty());
		SituationButton->SetAccentAndPrimary(ERUI::Theme()->Accent, true);
		SituationButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleSituationClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->SetButtonTexts(INVTEXT("设置"), FText::GetEmpty());
		SettingsButton->SetAccentAndPrimary(ERUI::Theme()->Accent, true);
		SettingsButton->OnButtonClicked.AddUniqueDynamic(this, &UERHallHUD::HandleSettingsClicked);
	}
	if (ZenHintText && !ZenHintMessage.IsEmpty())
	{
		ZenHintText->SetText(ZenHintMessage);
	}

	const FString SloganTag = TEXT("虹膜基金会  ·  银河军事指挥服务");
	const FString SloganSub = TEXT("「 我们面向全银河提供军事指挥服务 」");
	if (MastheadTagText)
	{
		MastheadTagText->SetText(FText::FromString(SloganTag));
	}
	if (MastheadMetaText)
	{
		MastheadMetaText->SetText(FText::FromString(SloganSub));
		MastheadMetaText->SetFont(ERUI::FontBold(12, 120));
		MastheadMetaText->SetColorAndOpacity(FSlateColor(ERUI::Theme()->Accent));
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

		MastheadTagText = ERUI::MakeText(this, INVTEXT("虹膜基金会  ·  银河军事指挥服务"), EERTypeScale::Micro, T->BoneSecondary, false);
		MastheadTagText->SetFont(ERUI::FontBold(13, 120));
		MCol->AddChildToVerticalBox(MastheadTagText);

		MastheadTitleText = ERUI::MakeText(this, INVTEXT("虹不我与"), EERTypeScale::Display, T->Bone);
		MastheadTitleText->SetFont(ERUI::FontBold(48, 80));
		MCol->AddChildToVerticalBox(MastheadTitleText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));

		MastheadDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("MastheadDivider"));
		MastheadDivider->AnchorLength = 28.0f;
		MastheadDivider->AnchorColor = T->Accent;
		MastheadDivider->LineColor = T->BoneGhost;
		MastheadDivider->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		MCol->AddChildToVerticalBox(MastheadDivider)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));

		MastheadMetaText = ERUI::MakeText(this, INVTEXT("「 我们面向全银河提供军事指挥服务 」"), EERTypeScale::Micro, T->BoneTertiary, false);
		MastheadMetaText->SetFont(ERUI::FontBold(14, 120));
		MCol->AddChildToVerticalBox(MastheadMetaText);

		UCanvasPanelSlot* MS = DashCanvas->AddChildToCanvas(MastOv);
		MS->SetAnchors(FAnchors(0.0f, 0.0f));
		MS->SetPosition(FVector2D(46.0f, 44.0f));
		MS->SetAutoSize(true);
	}

	// 3. 左部标题下方纵向导航: [01 兵团整备] [02 银河委托] [03 银河局势] [04 设置]
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
		DockSlot->SetPosition(FVector2D(46.0f, -56.0f));
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
			NavBox->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
		}
	}

	// 4. 观景模式提示条 (锐利直角黑曜石条)
	if (!ZenHintBorder)
	{
		ZenHintBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ZenHintBorder"));
		ZenHintBorder->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.009f, 0.012f, 0.86f), 1.0f, T->BoneGhost));
		ZenHintBorder->SetBrushColor(FLinearColor::White);
		ZenHintBorder->SetPadding(FMargin(24.0f, 10.0f));
		ZenHintBorder->SetVisibility(ESlateVisibility::Hidden);

		ZenHintText = ERUI::MakeText(this, ZenHintMessage, EERTypeScale::Micro, T->BoneSecondary, true);
		ZenHintText->SetFont(ERUI::FontRegular(9, 200));
		ZenHintBorder->SetContent(ZenHintText);

		UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(ZenHintBorder);
		HintSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		HintSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		HintSlot->SetPosition(FVector2D(0.0f, -32.0f));
		HintSlot->SetAutoSize(true);
	}

	// 右上银河战役存档提示 (正常时折叠)
	if (!CampaignSaveNoticeBorder)
	{
		CampaignSaveNoticeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CampaignSaveNoticeBorder"));
		CampaignSaveNoticeBorder->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.009f, 0.012f, 0.86f), 1.0f, T->BoneGhost));
		CampaignSaveNoticeBorder->SetBrushColor(FLinearColor::White);
		CampaignSaveNoticeBorder->SetPadding(FMargin(18.0f, 9.0f));
		CampaignSaveNoticeBorder->SetVisibility(ESlateVisibility::Collapsed);

		CampaignSaveNoticeText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, false);
		CampaignSaveNoticeText->SetAutoWrapText(true);
		CampaignSaveNoticeBorder->SetContent(CampaignSaveNoticeText);

		UCanvasPanelSlot* NoticeSlot = RootCanvas->AddChildToCanvas(CampaignSaveNoticeBorder);
		NoticeSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		NoticeSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		NoticeSlot->SetPosition(FVector2D(-46.0f, 44.0f));
		NoticeSlot->SetAutoSize(true);
	}
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
		DashboardContainer->SetVisibility(bInZen ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
	else if (NavBox)
	{
		NavBox->SetVisibility(bInZen ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
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
	UE_LOG(LogERHall, Log, TEXT("[Hall] Legion Roster clicked -> Galactic Panel (Tab 1)."));
	BP_OnPersonnelClicked();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(PC->GetHUD()))
		{
			HallHUD->OpenGalacticPanel(1);
		}
		else
		{
			UE_LOG(LogERHall, Warning, TEXT("[Hall] Galactic panel skipped: HUD is not AHallHUD."));
		}
	}
}

void UERHallHUD::HandleRoutesClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Galactic Contracts clicked -> Galactic Panel (Tab 0)."));
	BP_OnRoutesClicked();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(PC->GetHUD()))
		{
			HallHUD->OpenGalacticPanel(0);
			return;
		}
	}
	LaunchRouteMap();
}

void UERHallHUD::HandleSituationClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Galaxy Situation clicked -> Galactic Panel (Tab 2)."));

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(PC->GetHUD()))
		{
			HallHUD->OpenGalacticPanel(2);
		}
	}
}

void UERHallHUD::HandleSettingsClicked()
{
	UE_LOG(LogERHall, Log, TEXT("[Hall] Settings clicked -> Galactic Panel (Tab 3)."));

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AHallHUD* HallHUD = Cast<AHallHUD>(PC->GetHUD()))
		{
			HallHUD->OpenGalacticPanel(3);
		}
	}
}
