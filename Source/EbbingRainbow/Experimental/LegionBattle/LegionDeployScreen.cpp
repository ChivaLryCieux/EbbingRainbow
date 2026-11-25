// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionDeployScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "ContentStreaming.h"
#include "Core/ERLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "GrassInstancedStaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Misc/Parse.h"
#include "UI/ERActionButton.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUITheme.h"
#include "UI/Primitives/ERTechPrimitives.h"

namespace
{
	/** 大厅过渡屏实例登记: 跨关卡 travel 后若仍存活, 由交互式接驳屏在接管时移除。
	 *  (视口控件在打包版 travel 中存活是引擎行为, PIE 中则可能被销毁 —— 两条路径都要正确) */
	TWeakObjectPtr<ULegionDeployScreen> GTransitDeployScreen;
}

TSharedRef<SWidget> ULegionDeployScreen::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	return Super::RebuildWidget();
}

void ULegionDeployScreen::SetupInteractive()
{
	bTransit = false;

	// 接管大厅过渡屏: 若其在 travel 后仍滞留视口 (z-order 更高, 会盖住本屏), 立即移除
	if (GTransitDeployScreen.IsValid() && GTransitDeployScreen.Get() != this)
	{
		GTransitDeployScreen->RemoveFromParent();
	}
	GTransitDeployScreen.Reset();
}

void ULegionDeployScreen::SetupTransit(const FText& InTitle, const FText& InSub)
{
	bTransit = true;
	PendingTitle = InTitle;
	PendingSub = InSub;
	if (TitleText) { TitleText->SetText(InTitle); }
	if (SubText) { SubText->SetText(InSub); }
	GTransitDeployScreen = this;
}

void ULegionDeployScreen::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

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

	// ---- 全屏极深纯墨底幕 (Visible: 吃掉一切落点输入) ----
	{
		UImage* Scrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Scrim"));
		Scrim->SetBrush(ERUI::WhiteBrush());
		Scrim->SetColorAndOpacity(FLinearColor(T->PureInk.R, T->PureInk.G, T->PureInk.B, 1.0f));
		Scrim->SetVisibility(ESlateVisibility::Visible);
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Scrim);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// ---- 顶部通栏虹彩窄条 ----
	{
		UERPrismBar* Prism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("TopPrism"));
		Prism->bVertical = false;
		Prism->BarSize = FVector2D(4000.0f, 3.0f);
		Anchor(Prism, Root, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D::ZeroVector);
	}

	// ---- 中央纵列 ----
	UVerticalBox* CenterCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterCol"));
	Anchor(CenterCol, Root, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	// 标题行: 虹彩竖条 + 微标/标题/子标
	{
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));

		UERPrismBar* VPrism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("VPrism"));
		VPrism->bVertical = true;
		VPrism->BarSize = FVector2D(5.0f, 96.0f);
		TitleRow->AddChildToHorizontalBox(VPrism)->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));

		UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));

		UTextBlock* Micro = ERUI::MakeText(this, INVTEXT("虹膜神经接驳"), EERTypeScale::Micro, T->BoneTertiary, false);
		Micro->SetFont(ERUI::FontBold(13, 320));
		TitleCol->AddChildToVerticalBox(Micro);

		TitleText = ERUI::MakeText(this, INVTEXT("出击部署"), EERTypeScale::Display, T->Bone, false);
		TitleText->SetFont(ERUI::FontBold(46, 60));
		TitleCol->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

		SubText = ERUI::MakeText(this, INVTEXT("虹膜基金会  ·  银河军事指挥"), EERTypeScale::Caption, T->BoneSecondary, false);
		SubText->SetFont(ERUI::FontRegular(16, 40));
		TitleCol->AddChildToVerticalBox(SubText)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

		TitleRow->AddChildToHorizontalBox(TitleCol);
		if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(TitleRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
	}

	// 阶段 + 明细
	StageText = ERUI::MakeText(this, INVTEXT("正在建立前线神经链路"), EERTypeScale::Title, T->Bone, false);
	StageText->SetFont(ERUI::FontBold(20, 100));
	StageText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(StageText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 42.0f, 0.0f, 0.0f));
	}

	DetailText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary, false);
	DetailText->SetFont(ERUI::FontBold(13, 120));
	DetailText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(DetailText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}

	// 进度细条 (520x3, 骨白鬼影轨道 + 折射暖金填充)
	{
		USizeBox* TrackSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TrackSize"));
		TrackSize->SetMinDesiredWidth(520.0f);
		TrackSize->SetMaxDesiredHeight(3.0f);

		ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress"));
		FProgressBarStyle PBS;
		FSlateBrush BgBrush = ERUI::WhiteBrush();
		BgBrush.TintColor = FSlateColor(FLinearColor(T->Bone.R, T->Bone.G, T->Bone.B, 0.14f));
		PBS.SetBackgroundImage(BgBrush);
		FSlateBrush FillBrush = ERUI::WhiteBrush();
		FillBrush.TintColor = FSlateColor(T->Accent);
		PBS.SetFillImage(FillBrush);
		ProgressBar->SetWidgetStyle(PBS);
		ProgressBar->SetPercent(0.0f);
		TrackSize->AddChild(ProgressBar);

		if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(TrackSize))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
		}
	}

	// 「进入战场」按钮 (接驳模式专用; 过渡模式隐藏)
	EnterButton = UERActionButton::Make(this, INVTEXT("进入战场"), 26, true);
	EnterButton->SetMinSize(420.0f, 0.0f);
	EnterButton->SetButtonEnabled(false);
	EnterButton->SetNativeClick([this]() { HandleEnterClicked(); });
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(EnterButton))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 34.0f, 0.0f, 0.0f));
	}

	HintText = ERUI::MakeText(this, INVTEXT("接驳完成后点击进入战场接管前线指挥"), EERTypeScale::Micro, T->BoneTertiary, false);
	HintText->SetFont(ERUI::FontBold(13, 140));
	HintText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(HintText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
	}

	ProgressSpring.SnapTo(0.06f);
}

void ULegionDeployScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}

	if (bTransit)
	{
		// 过渡模式: 无按钮无轮询, 纯静态视觉 (文案经 Pending 缓存落地, 见 SetupTransit)
		if (!PendingTitle.IsEmpty() && TitleText) { TitleText->SetText(PendingTitle); }
		if (!PendingSub.IsEmpty() && SubText) { SubText->SetText(PendingSub); }
		if (EnterButton) { EnterButton->SetVisibility(ESlateVisibility::Collapsed); }
		if (HintText) { HintText->SetText(INVTEXT("神经链路建立中  ·  正在抵达战区")); }
		if (DetailText) { DetailText->SetText(FText::GetEmpty()); }
		if (ProgressBar) { ProgressBar->SetPercent(0.35f); }
		return;
	}

	// 接驳模式: 标题取战场实况, 开草加速
	if (UWorld* World = GetWorld())
	{
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(World->GetAuthGameMode()))
		{
			if (TitleText) { TitleText->SetText(FText::FromString(GM->GetBattleTitleText())); }
			if (SubText) { SubText->SetText(FText::FromString(GM->GetBattleSubText())); }
		}
	}
	ApplyGrassBoost();
}

void ULegionDeployScreen::NativeDestruct()
{
	RestoreGrassBoost();
	Super::NativeDestruct();
}

void ULegionDeployScreen::ApplyGrassBoost()
{
	if (bGrassBoosted || bTransit) { return; }

	// 近处优先生成 + 4 倍创建配额: PrioritizeGrassCreation 让 UpdateGrass 按相机距离排序
	// (可见泡内的草皮最先完成, 正好命中接驳就绪判定的统计范围) 并同步等待每帧任务,
	// MaxCreatePerFrame/MaxAsyncTasks 自动乘 GrassCreationPrioritizedMultipler(4) —
	// 4×4=16 路并行正好铺满物理核, 不再手工调高 cvar 基数
	if (UWorld* World = GetWorld())
	{
		if (ULandscapeSubsystem* LandscapeSys = World->GetSubsystem<ULandscapeSubsystem>())
		{
			LandscapeSys->PrioritizeGrassCreation(true);
			bGrassPrioritized = true;
		}
	}
	CVarMaxCreate = IConsoleManager::Get().FindConsoleVariable(TEXT("grass.MaxCreatePerFrame"));
	if (CVarMaxCreate)
	{
		SavedMaxCreate = CVarMaxCreate->GetInt();
		CVarMaxCreate->Set(4, ECVF_SetByCode);
	}
	bGrassBoosted = true;
}

void ULegionDeployScreen::RestoreGrassBoost()
{
	if (!bGrassBoosted) { return; }
	if (CVarMaxCreate) { CVarMaxCreate->Set(SavedMaxCreate, ECVF_SetByCode); }
	if (bGrassPrioritized)
	{
		if (UWorld* World = GetWorld())
		{
			if (ULandscapeSubsystem* LandscapeSys = World->GetSubsystem<ULandscapeSubsystem>())
			{
				LandscapeSys->PrioritizeGrassCreation(false);
			}
		}
		bGrassPrioritized = false;
	}
	bGrassBoosted = false;
}

void ULegionDeployScreen::SetProgressTarget(float Target)
{
	ProgressSpring.SetTarget(FMath::Clamp(Target, 0.0f, 1.0f));
}

int32 ULegionDeployScreen::CountGrassInstances(bool& bOutAnyLandscape) const
{
	bOutAnyLandscape = false;
	int32 Total = 0;
	UWorld* World = GetWorld();
	if (!World) { return 0; }

	// 只统计「可见泡」内的植被: 草种 EndCull 最远 70m, 超过 ~80m 的草皮反正不可见,
	// 其生成属于流送系统的后台工作, 不该拖住接驳就绪判定 —— 实测全图植被实例 34 万+ 且线性增长不收敛,
	// 若按全图统计, 就绪永远等不到; 玩家进场后推进前线时, 新进入 70m 泡的草皮增量流送即可
	FVector CamPos = FVector::ZeroVector;
	bool bHasCam = false;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			CamPos = PC->PlayerCameraManager->GetCameraLocation();
			bHasCam = true;
		}
	}
	const float VisibleRadiusSq = 8000.0f * 8000.0f;

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		bOutAnyLandscape = true;
		TArray<UGrassInstancedStaticMeshComponent*> GrassComps;
		It->GetComponents<UGrassInstancedStaticMeshComponent>(GrassComps);
		for (const UGrassInstancedStaticMeshComponent* G : GrassComps)
		{
			if (!G) { continue; }
			if (bHasCam && FVector::DistSquared(G->Bounds.Origin, CamPos) > VisibleRadiusSq) { continue; }
			Total += G->GetNumRenderInstances();
		}
	}
	return Total;
}

void ULegionDeployScreen::PollReadiness()
{
	// 1. 渲染资产流送 (纹理/网格): 待流送清零即定论 (只进不退)
	const int32 Wanting = IStreamingManager::Get().GetNumWantingResources();
	PeakWantingResources = FMath::Max(PeakWantingResources, Wanting);
	if (Wanting <= 0) { bStreamDone = true; }

	// 2. 植被构筑: 实例总数连续 4 次轮询 (≈0.6s) 不再增长即定论;
	//    计数恒零时要求 6s 暖机底线, 防止生成尚未启动就被误判完成
	bool bAnyLandscape = false;
	const int32 Grass = CountGrassInstances(bAnyLandscape);
	if (Grass == LastGrassInstances) { StablePolls++; }
	else { StablePolls = 0; LastGrassInstances = Grass; }
	if (Grass > 0) { LastGrassSeen = Grass; }

	bool bGrassDone;
	if (!bAnyLandscape) { bGrassDone = true; }
	else if (Grass > 0) { bGrassDone = (StablePolls >= 4); }
	else { bGrassDone = (StablePolls >= 4) && (LiveTime >= 6.0f); }

	const bool bTimeout = (LiveTime >= ForceReadySeconds);
	bReady = (LiveTime >= MinDwellSeconds) && ((bStreamDone && bGrassDone) || bTimeout);
	if (bTimeout && !bTimeoutLogged)
	{
		bTimeoutLogged = true;
		UE_LOG(LogERLegion, Warning, TEXT("[Deploy] Readiness force-fired after %.0fs (stream=%d grass=%d stable=%d)"),
			ForceReadySeconds, (int32)bStreamDone, Grass, StablePolls);
	}

	// 阶段文案与进度
	if (!bStreamDone)
	{
		StageText->SetText(INVTEXT("战场资产流送"));
		DetailText->SetText(FText::FromString(FString::Printf(TEXT("剩余流送资源 %d 项"), Wanting)));
		SetProgressTarget(0.30f);
	}
	else if (!bGrassDone)
	{
		StageText->SetText(INVTEXT("地表植被构筑"));
		DetailText->SetText(FText::FromString(FString::Printf(TEXT("植被实例 %d"), Grass)));
		SetProgressTarget(0.62f);
	}
	else
	{
		StageText->SetText(INVTEXT("接驳完成"));
		DetailText->SetText(INVTEXT("战场神经同步就绪"));
		SetProgressTarget(1.0f);
	}

	if (bReady && !bReadyApplied)
	{
		bReadyApplied = true;
		if (EnterButton)
		{
			EnterButton->SetButtonEnabled(true);
			EnterButton->SetUrgent(true);
		}
		if (HintText) { HintText->SetText(INVTEXT("接驳完成  ·  点击进入战场")); }
		UE_LOG(LogERLegion, Log, TEXT("[Deploy] Link ready in %.1fs (grass instances=%d)"), LiveTime, Grass);
	}

	// 生长曲线遥测 (~2s 一行): 离线验收时据此判断植被构筑是收尾平台期还是无限 churn
	TelemetryTimer += 0.15f;
	if (TelemetryTimer >= 2.0f)
	{
		TelemetryTimer = 0.0f;
		UE_LOG(LogERLegion, Log, TEXT("[Deploy] t=%.1fs wanting=%d grass=%d stable=%d stream=%d"),
			LiveTime, Wanting, Grass, StablePolls, (int32)bStreamDone);
	}
}

void ULegionDeployScreen::HandleEnterClicked()
{
	// 按钮在就绪前处于禁用态, 玩家无法提前点击; bReady 不再重复校验,
	// 以便自动化链路 (GameMode 已自行开战) 能经此路径强制撤屏
	if (bDismissing || bTransit) { return; }
	bDismissing = true;
	if (EnterButton) { EnterButton->SetButtonEnabled(false); }

	// 世界立即激活: 淡出期间玩家已能看到兵团待命
	if (UWorld* World = GetWorld())
	{
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(World->GetAuthGameMode()))
		{
			GM->BeginBattle();
		}
		if (ALegionPlayerController* LPC = Cast<ALegionPlayerController>(World->GetFirstPlayerController()))
		{
			LPC->SetDeploymentInputLocked(false);
		}
	}
	RestoreGrassBoost();
}

void ULegionDeployScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 过渡模式兜底: 正常路径下本屏会被交互式接驳屏接管移除 (或随 PIE travel 销毁);
	// 若 travel 失败/异常滞留, 超时自行退场, 避免永久盖住大厅 —— 超时期间若仍在阻塞加载,
	// 画面本就冻结, 移除不会产生任何视觉变化
	if (bTransit)
	{
		LiveTime += InDeltaTime;
		if (LiveTime >= 12.0f) { RemoveFromParent(); }
		return;
	}

	// 淡出收尾
	if (bDismissing)
	{
		FadeAlpha -= InDeltaTime / 0.45f;
		if (FadeAlpha <= 0.0f)
		{
			RemoveFromParent();
			return;
		}
		SetRenderOpacity(FadeAlpha);
		return;
	}

	LiveTime += InDeltaTime;

	// 自动化链路兜底: GameMode 若已直接开战 (理论上不会走到, 双保险), 立即撤屏
	if (UWorld* World = GetWorld())
	{
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(World->GetAuthGameMode()))
		{
			if (GM->IsBattleStarted())
			{
				HandleEnterClicked();
				return;
			}
		}
	}

	ProgressSpring.Tick(InDeltaTime);
	if (ProgressBar) { ProgressBar->SetPercent(ProgressSpring.Value); }

	PollAccum += InDeltaTime;
	if (PollAccum >= 0.15f)
	{
		PollAccum = 0.0f;
		PollReadiness();
	}

	// -DeployShot 离线验收链路: 就绪后稍作停留 (供 LegionDeployReady 截图), 自动点击「进入战场」;
	// LiveTime>=5.5 与就绪+1.2s 取较晚者, 保证 4.5s 的就绪态截图完成前屏幕不会提前退场
	if (bReady && FParse::Param(FCommandLine::Get(), TEXT("-DeployShot")))
	{
		ReadyAutoEnterTimer += InDeltaTime;
		if (ReadyAutoEnterTimer >= 1.2f && LiveTime >= 5.5f)
		{
			HandleEnterClicked();
		}
	}
}
