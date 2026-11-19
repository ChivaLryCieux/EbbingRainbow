// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERBattleHUD.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"

#include "Core/ERLog.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUnitToken.h"
#include "UI/ERSkillCard.h"
#include "UI/ERSkillTooltip.h"
#include "UI/ERHeroPlate.h"
#include "UI/ERActionButton.h"
#include "UI/ERInteractiveWidget.h"
#if ER_WITH_AI_COMMAND
#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#endif

#include "Battle/Core/BattleManager.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleSkillComponent.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

TSharedRef<SWidget> UERBattleHUD::RebuildWidget()
{
	// WBP 模板树的 RootWidget 为空时, 必须在 Slate 树实例化之前(而不是
	// NativeConstruct)把程序化布局挂上去, 否则整棵树渲染为空。
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UERBattleHUD::NativePreConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativePreConstruct();

	if (EndTurnButton && !EndTurnLabel.IsEmpty())
	{
		EndTurnButton->SetLabel(EndTurnLabel);
	}

	if (IsDesignTime())
	{
		BuildDesignPreview();
	}
}

void UERBattleHUD::NativeConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativeConstruct();

	CacheDesignerBaseTransforms();

	// 清理任何设计期占位子项, 交给运行时对象池管理
	InitiativeTokens.Reset();
	SquadTokens.Reset();
	Cards.Reset();
	if (InitiativeBox) { InitiativeBox->ClearChildren(); }
	if (SquadBox)      { SquadBox->ClearChildren(); }
	if (HandBox)       { HandBox->ClearChildren(); }

	if (NotifyText)    { NotifyText->SetVisibility(ESlateVisibility::Hidden); }
	if (SkillTooltip)  { SkillTooltip->SetVisibility(ESlateVisibility::Collapsed); }
	if (TooltipRoot)   { TooltipRoot->SetVisibility(ESlateVisibility::Collapsed); }
	if (ResultPanel)   { ResultPanel->SetVisibility(ESlateVisibility::Collapsed); }

	if (EndTurnButton)
	{
		if (!EndTurnLabel.IsEmpty())
		{
			EndTurnButton->SetLabel(EndTurnLabel);
		}
		EndTurnButton->OnActionClicked.AddUniqueDynamic(this, &UERBattleHUD::HandleEndTurnClicked);
	}
#if ER_WITH_AI_COMMAND
	if (!IsDesignTime())
	{
		UERAICommandPanel::InstallOnBattleHUD(this, RootCanvas, ERUI::Theme()->ScreenMargin);
	}
#endif
	if (ResultPanel)
	{
		ResultPanel->OnPrimaryClicked.AddUniqueDynamic(this, &UERBattleHUD::HandleResultPrimary);
	}

	EntranceT = bEnableEntranceAnimation ? 0.0f : 1.0f;
	if (Anim_Intro)
	{
		PlayAnimationForward(Anim_Intro);
	}
}

void UERBattleHUD::NativeDestruct()
{
	if (UBattleManager* Mgr = BattleManager.Get())
	{
		Mgr->OnTurnStarted.RemoveDynamic(this, &UERBattleHUD::HandleTurnStarted);
		Mgr->OnBattleFinished.RemoveDynamic(this, &UERBattleHUD::HandleBattleFinished);
	}
	Super::NativeDestruct();
}

void UERBattleHUD::CacheDesignerBaseTransforms()
{
	if (bBaseTransformsCached)
	{
		return;
	}
	bBaseTransformsCached = true;
	if (InitiativeBox) { BaseInitiativeTranslation = InitiativeBox->GetRenderTransform().Translation; }
	if (HandBox)       { BaseHandTranslation = HandBox->GetRenderTransform().Translation; }
	if (SquadBox)      { BaseSquadTranslation = SquadBox->GetRenderTransform().Translation; }
	if (HeroPlate)     { BaseHeroOpacity = HeroPlate->GetRenderOpacity(); }
	if (EndTurnButton) { BaseEndTurnOpacity = EndTurnButton->GetRenderOpacity(); }
}

// ============================================================
// 布局与设计期预览
// ============================================================

void UERBattleHUD::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();
	const float M = T->ScreenMargin;

	if (!RootCanvas)
	{
		if (UCanvasPanel* ExistingCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			RootCanvas = ExistingCanvas;
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
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	auto AddAnchored = [this](UWidget* W, FVector2D Anchor, FVector2D Align, FVector2D Pos)
	{
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		S->SetAutoSize(true);
		return S;
	};

	if (!TopScrim)
	{
		TopScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TopScrim"));
		TopScrim->SetBrush(ERUI::ScrimBrush());
		TopScrim->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.85f));
		TopScrim->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(TopScrim);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 132.0f));
	}
	if (!BottomScrim)
	{
		BottomScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BottomScrim"));
		BottomScrim->SetBrush(ERUI::ScrimBrush());
		BottomScrim->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.9f));
		BottomScrim->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		BottomScrim->SetRenderScale(FVector2D(1.0f, -1.0f));
		BottomScrim->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(BottomScrim);
		S->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f, -176.0f, 0.0f, 0.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
	}

	if (!InitiativeBox)
	{
		UVerticalBox* Strip = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InitiativeStrip"));
		Strip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		InitiativeRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InitiativeRule"));
		InitiativeRule->SetBrush(ERUI::WhiteBrush(FVector2D(400.0f, T->RuleThickness)));
		InitiativeRule->SetColorAndOpacity(T->BoneGhost);
		InitiativeRule->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UVerticalBoxSlot* RuleSlot = Strip->AddChildToVerticalBox(InitiativeRule);
		RuleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		RuleSlot->SetHorizontalAlignment(HAlign_Center);

		InitiativeBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InitiativeBox"));
		InitiativeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UVerticalBoxSlot* BxSlot = Strip->AddChildToVerticalBox(InitiativeBox);
		BxSlot->SetHorizontalAlignment(HAlign_Center);

		AddAnchored(Strip, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 30.0f));
	}

	if (!NotifyText)
	{
		NotifyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->Accent);
		AddAnchored(NotifyText, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 96.0f));
	}

	if (!SquadBox)
	{
		SquadBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SquadBox"));
		SquadBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddAnchored(SquadBox, FVector2D(0.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(M, 0.0f));
	}

	if (!HeroPlate)
	{
		HeroPlate = WidgetTree->ConstructWidget<UERHeroPlate>(UERHeroPlate::StaticClass(), TEXT("HeroPlate"));
		AddAnchored(HeroPlate, FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(M, -44.0f));
	}
	HeroPlate->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (!HandBox)
	{
		HandBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HandBox"));
		HandBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddAnchored(HandBox, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -40.0f));
	}

	if (!SkillTooltip && !TooltipRoot)
	{
		SkillTooltip = WidgetTree->ConstructWidget<UERSkillTooltip>(UERSkillTooltip::StaticClass(), TEXT("SkillTooltip"));
		AddAnchored(SkillTooltip, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -180.0f));
	}

	if (!EndTurnButton)
	{
		EndTurnButton = WidgetTree->ConstructWidget<UERActionButton>(UERActionButton::StaticClass(), TEXT("EndTurnButton"));
		AddAnchored(EndTurnButton, FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-M, -52.0f));
	}

	if (!ResultPanel)
	{
		ResultPanel = WidgetTree->ConstructWidget<UERBattleResult>(UERBattleResult::StaticClass(), TEXT("ResultPanel"));
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ResultPanel);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}
}

void UERBattleHUD::BuildDesignPreview()
{
	if (!WidgetTree)
	{
		return;
	}
	const UERUITheme* T = ERUI::Theme();

	if (NotifyText)
	{
		NotifyText->SetText(PreviewNotificationText);
		NotifyText->SetVisibility(bPreviewNotification ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
	if (SkillTooltip)
	{
		SkillTooltip->SetVisibility(bPreviewTooltip ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TooltipRoot)
	{
		if (TooltipTitle) { TooltipTitle->SetText(INVTEXT("相位斩击\n突进劈砍单体目标")); }
		if (TooltipMeta)  { TooltipMeta->SetText(INVTEXT("射 300 · 伤 35 · 2 AP")); }
		TooltipRoot->SetVisibility(bPreviewTooltip ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(bPreviewResultPanel ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!bPreviewPopulateContainers)
	{
		if (InitiativeBox) { InitiativeBox->ClearChildren(); }
		if (SquadBox)      { SquadBox->ClearChildren(); }
		if (HandBox)       { HandBox->ClearChildren(); }
		return;
	}

	// 先攻条预览
	if (InitiativeBox)
	{
		InitiativeBox->ClearChildren();
		const TSubclassOf<UERUnitToken> TokCls = InitiativeTokenClass
			? InitiativeTokenClass
			: TSubclassOf<UERUnitToken>(UERUnitToken::StaticClass());
		static const TCHAR* Glyphs[] = { TEXT("锋"), TEXT("敌"), TEXT("观测"), TEXT("卫"), TEXT("猎") };
		for (int32 i = 0; i < PreviewInitiativeCount; ++i)
		{
			UERUnitToken* Tok = WidgetTree->ConstructWidget<UERUnitToken>(TokCls);
			if (!Tok) { continue; }
			Tok->PreviewGlyph = FText::FromString(FString(Glyphs[i % 5]).Left(1));
			Tok->bPreviewIsPlayer = (i % 2 == 0);
			Tok->bPreviewActive = (i == 0);
			Tok->PreviewHpPercent = 1.0f - 0.12f * i;
			Tok->bAlwaysShowName = false;
			UHorizontalBoxSlot* S = InitiativeBox->AddChildToHorizontalBox(Tok);
			S->SetPadding(FMargin(0.0f, 0.0f, (i == PreviewInitiativeCount - 1) ? 0.0f : InitiativeTokenGap, 0.0f));
		}
	}

	// 左侧编队预览
	if (SquadBox)
	{
		SquadBox->ClearChildren();
		const TSubclassOf<UERUnitToken> SqCls = SquadTokenClass
			? SquadTokenClass
			: TSubclassOf<UERUnitToken>(UERUnitToken::StaticClass());
		static const TCHAR* Names[] = { TEXT("观测者"), TEXT("铁卫"), TEXT("棱镜") };
		for (int32 i = 0; i < PreviewSquadCount; ++i)
		{
			UERUnitToken* Tok = WidgetTree->ConstructWidget<UERUnitToken>(SqCls);
			if (!Tok) { continue; }
			Tok->PreviewName = FText::FromString(Names[i % 3]);
			Tok->PreviewGlyph = FText::FromString(FString(Names[i % 3]).Left(1));
			Tok->bPreviewIsPlayer = true;
			Tok->bPreviewActive = (i == 0);
			Tok->bPreviewFocused = (i == 0);
			Tok->PreviewHpPercent = (i == 1) ? 0.65f : 0.92f;
			UVerticalBoxSlot* S = SquadBox->AddChildToVerticalBox(Tok);
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, (i == PreviewSquadCount - 1) ? 0.0f : SquadTokenGap));
		}
	}

	// 底部手牌预览
	if (HandBox)
	{
		HandBox->ClearChildren();
		const TSubclassOf<UERSkillCard> CardCls = SkillCardClass
			? SkillCardClass
			: TSubclassOf<UERSkillCard>(UERSkillCard::StaticClass());
		static const TCHAR* CardNames[] = { TEXT("相位斩击"), TEXT("棱镜护盾"), TEXT("电弧跳跃"), TEXT("机动") };
		const FLinearColor CardColors[] = {
			T->Accent,
			T->AllyMark,
			FLinearColor(0.68f, 0.45f, 0.95f, 1.0f),
			MoveCardAccent
		};
		for (int32 i = 0; i < PreviewHandCount; ++i)
		{
			UERSkillCard* Card = WidgetTree->ConstructWidget<UERSkillCard>(CardCls);
			if (!Card) { continue; }
			const bool bMove = bShowMoveCard && (i == PreviewHandCount - 1);
			UTexture2D* Icon = bMove ? MoveCardIcon.Get()
				: (PreviewSkillIcons.IsValidIndex(i) ? PreviewSkillIcons[i].Get() : Card->PreviewIcon.Get());
			const FText Hk = bMove ? MoveCardHotkey : FText::AsNumber(i + 1);
			Card->PreviewIcon = Icon;
			Card->PreviewName = FText::FromString(CardNames[i % 4]);
			Card->PreviewHotkey = Hk;
			Card->PreviewCost = bMove ? 0 : (i % 2 + 1);
			Card->PreviewAccent = CardColors[i % 4];
			Card->Setup(bMove ? -1 : i, Card->PreviewName, FText::GetEmpty(),
				FText::GetEmpty(), Hk, Card->PreviewCost, bMove, CardColors[i % 4], Icon);
			UHorizontalBoxSlot* S = HandBox->AddChildToHorizontalBox(Card);
			S->SetPadding(FMargin(HandCardGap, 0.0f));
		}
	}
}

// ============================================================
// 初始化 / 每帧刷新
// ============================================================

void UERBattleHUD::InitializeBattleUI(UBattleManager* InManager, ABattlePlayerController* InPC)
{
	if (UBattleManager* Old = BattleManager.Get())
	{
		Old->OnTurnStarted.RemoveDynamic(this, &UERBattleHUD::HandleTurnStarted);
		Old->OnBattleFinished.RemoveDynamic(this, &UERBattleHUD::HandleBattleFinished);
	}
	BattleManager = InManager;
	PlayerController = InPC;
	if (UBattleManager* Mgr = BattleManager.Get())
	{
		Mgr->OnTurnStarted.AddDynamic(this, &UERBattleHUD::HandleTurnStarted);
		Mgr->OnBattleFinished.AddDynamic(this, &UERBattleHUD::HandleBattleFinished);
	}
#if ER_WITH_AI_COMMAND
	UERAICommandPanel::InstallOnBattleHUD(this, RootCanvas, ERUI::Theme()->ScreenMargin);
#endif
	RefreshAll();
}

bool UERBattleHUD::IsMouseOverInteractiveUI() const
{
	if (ResultPanel && ResultPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		return true;
	}
	return UERInteractiveWidget::IsAnyHovered();
}

void UERBattleHUD::PostNotification(const FString& Message)
{
	if (NotifyText)
	{
		NotifyText->SetText(FText::FromString(Message));
		NotifyText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		NotifyText->SetRenderOpacity(1.0f);
		NotifyTimer = ERUI::Theme()->NotifyDuration;
	}
	if (Anim_Notify)
	{
		PlayAnimationForward(Anim_Notify);
	}
	BP_OnNotificationPosted(Message);
}

void UERBattleHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 入场: 相对 Designer 基准坐标轻微滑入, 不覆盖用户手调位置
	if (bEnableEntranceAnimation && EntranceT < 1.0f)
	{
		const float Dur = FMath::Max(0.05f, ERUI::Theme()->EntranceDuration);
		EntranceT = FMath::Min(EntranceT + InDeltaTime / Dur, 1.0f);
		const float C = 1.0f - EntranceT;
		const float E = 1.0f - C * C * C;
		if (InitiativeBox) { InitiativeBox->SetRenderTranslation(BaseInitiativeTranslation + InitiativeEntranceOffset * (1.0f - E)); }
		if (HandBox)       { HandBox->SetRenderTranslation(BaseHandTranslation + HandEntranceOffset * (1.0f - E)); }
		if (SquadBox)      { SquadBox->SetRenderTranslation(BaseSquadTranslation + SquadEntranceOffset * (1.0f - E)); }
		if (HeroPlate)     { HeroPlate->SetRenderOpacity(BaseHeroOpacity * E); }
		if (EndTurnButton) { EndTurnButton->SetRenderOpacity(BaseEndTurnOpacity * E); }
	}

	// 通知淡出
	if (NotifyTimer > 0.0f)
	{
		NotifyTimer -= InDeltaTime;
		if (NotifyTimer <= 0.0f && NotifyText)
		{
			NotifyText->SetVisibility(ESlateVisibility::Hidden);
		}
		else if (NotifyTimer < 0.4f && NotifyText)
		{
			NotifyText->SetRenderOpacity(NotifyTimer / 0.4f);
		}
	}

	UBattleManager* Mgr = BattleManager.Get();
	if (!Mgr)
	{
		return;
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	const EBattleState State = Mgr->GetBattleState();
	ABattlePlayerController* PC = PlayerController.Get();
	const EPlayerInteractionMode Mode = PC ? PC->GetInteractionMode() : EPlayerInteractionMode::Idle;

	// 先攻队列 (重建条件: 队列修订 / 单位修订 / 血量总和 / 活跃单位)
	const int32 QueueRev = Mgr->GetInitiativeRevision();
	const int32 UnitsRev = Mgr->GetUnitsRevision();
	int32 IntegritySum = 0;
	for (ABattleUnit* U : Mgr->GetInitiativeQueue())
	{
		if (U) { IntegritySum += FMath::RoundToInt(U->Integrity); }
	}
	if (QueueRev != CachedQueueRev || UnitsRev != CachedUnitsRev || IntegritySum != CachedIntegritySum || Active != CachedActiveUnit)
	{
		CachedQueueRev = QueueRev;
		CachedIntegritySum = IntegritySum;
		RebuildInitiative();
	}
	else
	{
		for (UERUnitToken* Tok : InitiativeTokens)
		{
			if (Tok) { Tok->RefreshFromUnit(); }
		}
		for (UERUnitToken* Tok : SquadTokens)
		{
			if (Tok) { Tok->RefreshFromUnit(); }
		}
	}
	if (UnitsRev != CachedUnitsRev)
	{
		CachedUnitsRev = UnitsRev;
		RebuildSquad();
	}

	// 编队聚焦态同步
	if (PC)
	{
		ABattleUnit* Focused = PC->GetFocusedUnit();
		for (UERUnitToken* Tok : SquadTokens)
		{
			if (Tok) { Tok->SetFocusedFlag(Tok->GetUnit() == Focused); }
		}
	}

	// 手牌 + 状态牌
	const float CurAP = Active ? (float)Active->AP : -1.0f;
	const int32 CurHP = Active ? FMath::RoundToInt(Active->Integrity) : -1;
	int32 SkillSig = 0;
	if (Active)
	{
		for (const FBattleSkillSpec& Spec : Active->GetSkillSpecs())
		{
			SkillSig = SkillSig * 31 + (Spec.CooldownRemaining * 7 + (Spec.bBlockedThisTurn ? 1 : 0) + (Spec.SkillData ? GetTypeHash(Spec.SkillData->SkillId) : 0));
		}
	}
	if (Active != CachedActiveUnit || FMath::Abs(CurAP - CachedAP) > 0.01f || CurHP != CachedHP
		|| Mode != CachedMode || SkillSig != CachedSkillSignature)
	{
		CachedActiveUnit = Active;
		CachedAP = CurAP;
		CachedHP = CurHP;
		CachedMode = Mode;
		CachedSkillSignature = SkillSig;
		RefreshHeroPlate(Active);
		RefreshHand(Active);
	}

	if (State != CachedState)
	{
		CachedState = State;
		RefreshResult(State);
	}
	RefreshEndTurn(Active, State);
}

void UERBattleHUD::RefreshAll()
{
	CachedQueueRev = -1;
	CachedUnitsRev = -1;
	CachedIntegritySum = INT32_MIN;
	CachedAP = -1.0f;
	CachedHP = -1;
	CachedSkillSignature = -1;
	CachedMode = EPlayerInteractionMode::TargetingSkill;
	CachedActiveUnit = nullptr;
}

// ============================================================
// 先攻条 / 编队
// ============================================================

void UERBattleHUD::RebuildInitiative()
{
	if (!InitiativeBox)
	{
		return;
	}
	UBattleManager* Mgr = BattleManager.Get();
	if (!Mgr)
	{
		return;
	}

	const TArray<ABattleUnit*>& Queue = Mgr->GetInitiativeQueue();
	ABattleUnit* Active = Mgr->GetActiveUnit();

	const TSubclassOf<UERUnitToken> TokCls = InitiativeTokenClass
		? InitiativeTokenClass
		: TSubclassOf<UERUnitToken>(UERUnitToken::StaticClass());
	while (InitiativeTokens.Num() < Queue.Num())
	{
		UERUnitToken* Tok = WidgetTree->ConstructWidget<UERUnitToken>(TokCls);
		Tok->OnTokenClicked.AddDynamic(this, &UERBattleHUD::HandleTokenClicked);
		InitiativeTokens.Add(Tok);
		UHorizontalBoxSlot* S = InitiativeBox->AddChildToHorizontalBox(Tok);
		S->SetPadding(FMargin(0.0f, 0.0f, InitiativeTokenGap, 0.0f));
	}
	while (InitiativeTokens.Num() > Queue.Num())
	{
		InitiativeBox->RemoveChild(InitiativeTokens.Last());
		InitiativeTokens.Pop();
	}

	for (int32 i = 0; i < Queue.Num(); ++i)
	{
		ABattleUnit* U = Queue[i];
		UERUnitToken* Tok = InitiativeTokens[i];
		if (!U || !Tok)
		{
			continue;
		}
		Tok->Setup(U, false);
		Tok->SetActiveFlag(U == Active);
	}
}

void UERBattleHUD::RebuildSquad()
{
	if (!SquadBox)
	{
		return;
	}
	UBattleManager* Mgr = BattleManager.Get();
	if (!Mgr)
	{
		return;
	}

	TArray<ABattleUnit*> Players;
	for (ABattleUnit* U : Mgr->GetAllUnits())
	{
		if (U && U->Team == EBattleTeam::Player)
		{
			Players.Add(U);
		}
	}

	const TSubclassOf<UERUnitToken> TokCls = SquadTokenClass
		? SquadTokenClass
		: TSubclassOf<UERUnitToken>(UERUnitToken::StaticClass());
	while (SquadTokens.Num() < Players.Num())
	{
		UERUnitToken* Tok = WidgetTree->ConstructWidget<UERUnitToken>(TokCls);
		Tok->OnTokenClicked.AddDynamic(this, &UERBattleHUD::HandleTokenClicked);
		SquadTokens.Add(Tok);
		UVerticalBoxSlot* S = SquadBox->AddChildToVerticalBox(Tok);
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, SquadTokenGap));
	}
	while (SquadTokens.Num() > Players.Num())
	{
		SquadBox->RemoveChild(SquadTokens.Last());
		SquadTokens.Pop();
	}

	ABattleUnit* Active = Mgr->GetActiveUnit();
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		SquadTokens[i]->Setup(Players[i], true);
		SquadTokens[i]->SetActiveFlag(Players[i] == Active);
	}
}

// ============================================================
// 状态牌 / 手牌 / 结束回合
// ============================================================

void UERBattleHUD::RefreshHeroPlate(ABattleUnit* Unit)
{
	if (HeroPlate)
	{
		HeroPlate->BindUnit(Unit);
	}
}

void UERBattleHUD::RefreshHand(ABattleUnit* Unit)
{
	if (!HandBox)
	{
		return;
	}
	ABattlePlayerController* PC = PlayerController.Get();
	const EPlayerInteractionMode Mode = PC ? PC->GetInteractionMode() : EPlayerInteractionMode::Idle;
	const int32 SelectedSkill = PC ? PC->GetSelectedSkillIndex() : -1;

	const bool bShowHand = IsValid(Unit) && Unit->IsAlive() && Unit->Team == EBattleTeam::Player;
	static const TArray<FBattleSkillSpec> EmptySkills;
	const TArray<FBattleSkillSpec>& Skills = bShowHand ? Unit->GetSkillSpecs() : EmptySkills;
	const int32 Wanted = bShowHand ? (Skills.Num() + (bShowMoveCard ? 1 : 0)) : 0;

	const TSubclassOf<UERSkillCard> CardCls = SkillCardClass
		? SkillCardClass
		: TSubclassOf<UERSkillCard>(UERSkillCard::StaticClass());
	while (Cards.Num() < Wanted)
	{
		UERSkillCard* Card = WidgetTree->ConstructWidget<UERSkillCard>(CardCls);
		Card->OnCardClicked.AddDynamic(this, &UERBattleHUD::HandleCardClicked);
		Card->OnCardHover.AddDynamic(this, &UERBattleHUD::HandleCardHover);
		Cards.Add(Card);
		UHorizontalBoxSlot* S = HandBox->AddChildToHorizontalBox(Card);
		S->SetPadding(FMargin(HandCardGap, 0.0f));
	}
	while (Cards.Num() > Wanted)
	{
		HandBox->RemoveChild(Cards.Last());
		Cards.Pop();
	}

	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		const FBattleSkillSpec& Spec = Skills[i];
		const UBattleSkillData* Skill = Spec.SkillData;
		if (!IsValid(Skill))
		{
			continue;
		}
		UERSkillCard* Card = Cards[i];

		FString RangeStr;
		if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly && Skill->AoE.IsArea())
		{
			RangeStr = FString::Printf(TEXT("自周 %.0f"), Skill->AoE.Radius);
		}
		else if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
		{
			RangeStr = TEXT("自身");
		}
		else if (Skill->AoE.IsArea())
		{
			RangeStr = FString::Printf(TEXT("射 %.0f · 域 %.0f"), Skill->Range, Skill->AoE.Radius);
		}
		else
		{
			RangeStr = FString::Printf(TEXT("射 %.0f"), Skill->Range);
		}
		if (Skill->Chain.IsChain())
		{
			RangeStr += FString::Printf(TEXT(" · 链%d"), Skill->Chain.MaxJumps);
		}
		FString MetaStr = FString::Printf(TEXT("%s · 伤 %.0f · %d AP"), *RangeStr,
			Skill->GetEstimatedTotalDamage(), Skill->APCost);
		if (Skill->HitCount > 1)
		{
			MetaStr += FString::Printf(TEXT(" x%d"), Skill->HitCount);
		}
		if (Spec.IsOnCooldown())
		{
			MetaStr += TEXT("  ·  ") + Spec.GetCooldownLabel();
		}

		FString TipStr = FString::Printf(TEXT("%s\n%s"), *Skill->DisplayName.ToString(), *Skill->Description.ToString());

		const FText CooldownBadge = Spec.CooldownRemaining > 0 || Spec.bBlockedThisTurn
			? FText::FromString(FString::Printf(TEXT("%d"), FMath::Max(1, Spec.CooldownRemaining)))
			: FText::GetEmpty();

		Card->Setup(i, Skill->DisplayName, FText::FromString(TipStr), CooldownBadge,
			FText::FromString(FString::Printf(TEXT("%d"), i + 1)),
			Skill->APCost, false, Skill->ThemeColor, Skill->Icon);
		Card->SetCardDescription(Skill->Description);
		Card->SetCardMeta(FText::FromString(MetaStr));
		Card->SetAffordable((Unit->AP >= Skill->APCost) && !Spec.IsOnCooldown());
		Card->SetSelectedMode(Mode == EPlayerInteractionMode::TargetingSkill && SelectedSkill == i);
	}

	// 移动卡 (末位)
	if (Wanted > 0 && bShowMoveCard)
	{
		UERSkillCard* MoveCard = Cards.Last();
		const FText MoveDesc = FText::FromString(FString::Printf(TEXT("消耗 AP 沿地面寻路 (1 AP = %.0f cm)"), Unit->GetCmPerAP()));
		const FText MoveTip = FText::FromString(FString::Printf(TEXT("%s\n%s"), *MoveCardName.ToString(), *MoveDesc.ToString()));
		MoveCard->Setup(-1, MoveCardName, MoveTip, FText::GetEmpty(), MoveCardHotkey, 0, true, MoveCardAccent, MoveCardIcon);
		MoveCard->SetCardDescription(MoveDesc);
		MoveCard->SetCardMeta(FText::FromString(FString::Printf(TEXT("1 AP = %.0f cm"), Unit->GetCmPerAP())));
		MoveCard->SetAffordable(Unit->AP >= 1);
		MoveCard->SetSelectedMode(Mode == EPlayerInteractionMode::Moving);
	}
}

void UERBattleHUD::RefreshEndTurn(ABattleUnit* Unit, EBattleState State)
{
	if (!EndTurnButton)
	{
		return;
	}
	const bool bOver = (State == EBattleState::BattleOver);
	const bool bMyTurn = !bOver && IsValid(Unit) && Unit->IsAlive() && Unit->Team == EBattleTeam::Player;
	EndTurnButton->SetButtonEnabled(bMyTurn);

	bool bUrgent = false;
	if (bMyTurn)
	{
		bUrgent = (Unit->AP <= 0);
		if (!bUrgent)
		{
			bool bAnyAffordable = bShowMoveCard && (Unit->AP >= 1);
			for (const FBattleSkillSpec& Spec : Unit->GetSkillSpecs())
			{
				if (Spec.SkillData && Unit->AP >= Spec.SkillData->APCost && !Spec.IsOnCooldown())
				{
					bAnyAffordable = true;
					break;
				}
			}
			bUrgent = !bAnyAffordable;
		}
	}
	EndTurnButton->SetUrgent(bUrgent);
}

void UERBattleHUD::RefreshResult(EBattleState State)
{
	if (!ResultPanel)
	{
		return;
	}
	if (State != EBattleState::BattleOver)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UBattleManager* Mgr = BattleManager.Get();
	ABattleGameMode* GM = GetBattleGameMode();

	bool bPlayerWon = true;
	if (GM && GM->IsBattleResolved())
	{
		bPlayerWon = GM->DidPlayerWin();
	}
	else if (Mgr)
	{
		for (ABattleUnit* U : Mgr->GetAllUnits())
		{
			if (U && U->IsAlive() && U->Team == EBattleTeam::Enemy)
			{
				bPlayerWon = false;
				break;
			}
		}
	}

	int32 AlivePlayers = 0, TotalPlayers = 0;
	float CurInt = 0.0f, MaxInt = 0.0f;
	if (Mgr)
	{
		for (ABattleUnit* U : Mgr->GetAllUnits())
		{
			if (U && U->Team == EBattleTeam::Player)
			{
				++TotalPlayers;
				if (U->IsAlive()) { ++AlivePlayers; }
				CurInt += FMath::Max(0.0f, U->Integrity);
				MaxInt += U->Stats.MaxIntegrity;
			}
		}
	}

	const UERUITheme* T = ERUI::Theme();
	TArray<FERResultRow> Rows;
	if (Mgr)
	{
		FERResultRow R1;
		R1.Label = FText::FromString(TEXT("回合"));
		R1.Value = FText::AsNumber(Mgr->GetRoundIndex());
		R1.ValueTint = T->Bone;
		Rows.Add(R1);
	}
	FERResultRow R2;
	R2.Label = FText::FromString(TEXT("存活"));
	R2.Value = FText::FromString(FString::Printf(TEXT("%d / %d"), AlivePlayers, TotalPlayers));
	R2.ValueTint = (AlivePlayers == TotalPlayers) ? T->Bone : T->Accent;
	Rows.Add(R2);

	FERResultRow R3;
	R3.Label = FText::FromString(TEXT("完整度"));
	R3.Value = FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(CurInt), FMath::RoundToInt(MaxInt)));
	R3.ValueTint = T->BoneSecondary;
	Rows.Add(R3);

	FText Subtitle;
	if (bPlayerWon)
	{
		Subtitle = (GM && GM->IsRunBattle())
			? FText::FromString(FString::Printf(TEXT("敌方全灭 · 节点 %d"), GM->GetPendingNodeId()))
			: FText::FromString(TEXT("敌方全灭 · 推演完成"));
	}
	else
	{
		Subtitle = FText::FromString(TEXT("我方全灭 · 远征结束"));
	}

	const FText BtnText = (GM && GM->ShouldReturnToRoute())
		? ReturnToRouteLabel
		: ReturnToHallLabel;

	ResultPanel->SetResult(bPlayerWon, bPlayerWon ? VictoryTitle : DefeatTitle, Subtitle, Rows, BtnText);
}

// ============================================================
// 回调
// ============================================================

void UERBattleHUD::HandleTokenClicked(ABattleUnit* Unit)
{
	ABattlePlayerController* PC = PlayerController.Get();
	if (!PC || !Unit || !Unit->IsAlive())
	{
		return;
	}
	PC->FocusOnUnit(Unit);
}

void UERBattleHUD::HandleCardClicked(int32 Index, bool bIsMove)
{
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		if (bIsMove)
		{
			PC->ToggleMoveMode();
		}
		else
		{
			PC->StartSkillTargeting(Index);
		}
	}
}

void UERBattleHUD::HandleCardHover(UERSkillCard* Card, bool bHovered)
{
	if (SkillTooltip)
	{
		if (bHovered && Card)
		{
			SkillTooltip->SetTooltipContent(Card->GetCardName(), Card->GetCardDescription(), Card->GetCardMeta(), Card->GetCardAccent());
			SkillTooltip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			SkillTooltip->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (TooltipRoot)
	{
		if (bHovered && Card)
		{
			if (TooltipTitle) { TooltipTitle->SetText(Card->GetCardTooltip()); }
			if (TooltipMeta)  { TooltipMeta->SetText(Card->GetCardMeta()); }
			TooltipRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			TooltipRoot->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UERBattleHUD::HandleEndTurnClicked()
{
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		PC->RequestEndTurn();
	}
}

void UERBattleHUD::HandleResultPrimary()
{
	if (ABattleGameMode* GM = GetBattleGameMode())
	{
		GM->ReturnFromBattle();
		return;
	}
	UWorld* World = PlayerController.IsValid() ? PlayerController->GetWorld() : nullptr;
	if (World)
	{
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")), true,
			TEXT("game=/Script/EbbingRainbow.HallGameMode"));
	}
}

void UERBattleHUD::HandleTurnStarted(ABattleUnit* Unit)
{
	RefreshAll();
	BP_OnTurnChanged(Unit);
}

void UERBattleHUD::HandleBattleFinished(bool bPlayerWon)
{
	RefreshResult(EBattleState::BattleOver);
}

ABattleGameMode* UERBattleHUD::GetBattleGameMode() const
{
	if (ABattlePlayerController* PC = PlayerController.Get())
	{
		if (UWorld* World = PC->GetWorld())
		{
			return Cast<ABattleGameMode>(World->GetAuthGameMode());
		}
	}
	if (UBattleManager* Mgr = BattleManager.Get())
	{
		if (UWorld* World = Mgr->GetWorld())
		{
			return Cast<ABattleGameMode>(World->GetAuthGameMode());
		}
	}
	return nullptr;
}
