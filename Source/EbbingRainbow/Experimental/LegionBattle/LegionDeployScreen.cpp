// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionDeployScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "ContentStreaming.h"
#include "Core/ERLog.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "GrassInstancedStaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Misc/Parse.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUITheme.h"

namespace
{
	/** 大厅过渡屏实例登记: 跨关卡 travel 后若仍存活, 由交互式接驳屏在接管时移除。
	 *  (视口控件在打包版 travel 中存活是引擎行为, PIE 中则可能被销毁 —— 两条路径都要正确) */
	TWeakObjectPtr<ULegionDeployScreen> GTransitDeployScreen;

	// ---- 排版常量 (《控制:共振》式巨细对比: 星球名对小字约 10:1) ----
	/** 星球名字号 —— 全屏绝对主导 */
	constexpr float PlanetFontSize = 190.0f;
	/** 虹膜徽标高度 (px) */
	constexpr float LogoHeight = 100.0f;
	/** 连接文案字号 */
	constexpr float FlavorFontSize = 15.0f;
	/** 「接入虹膜」按钮字号 */
	constexpr float EnterFontSize = 17.0f;

	/** 白色虹膜徽标 (原 T_UI_Faction_Iris 为墨黑透明底字形, 黑幕上不可见) */
	const TCHAR* IrisWhiteLogoPath = TEXT("/Game/UI/Textures/T_UI_Faction_Iris_White.T_UI_Faction_Iris_White");

	/** 星球名 —— 当前全战役仅端点星一颗, 占位常量; 后续应由委托/星区数据驱动 */
	FText GetPlanetDisplayName()
	{
		return INVTEXT("端点星");
	}
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

void ULegionDeployScreen::SetupTransit()
{
	bTransit = true;
	GTransitDeployScreen = this;
}

void ULegionDeployScreen::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

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

	// ---- 中央纵列: 星球名 / 虹膜徽标 / 连接文案 / 接入按钮, 除此四项无一多余 ----
	UVerticalBox* CenterCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterCol"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(CenterCol);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D::ZeroVector);
		S->SetAutoSize(true);
	}

	// 1. 星球名 —— 超大粗体骨白字, 绝对视觉主导
	PlanetText = ERUI::MakeText(this, GetPlanetDisplayName(), EERTypeScale::Hero, T->Bone, false);
	PlanetText->SetFont(ERUI::FontBold((int32)PlanetFontSize, 0));
	PlanetText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(PlanetText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// 2. 虹膜徽标 (白色透明底, 高度驱动等比)
	if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, IrisWhiteLogoPath))
	{
		LogoImage = ERUI::MakeLogoImage(this, Tex, LogoHeight);
	}
	if (LogoImage)
	{
		if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(LogoImage))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 64.0f, 0.0f, 0.0f));
		}
	}
	else
	{
		UE_LOG(LogERLegion, Warning, TEXT("[Deploy] 白色虹膜徽标缺失: %s"), IrisWhiteLogoPath);
	}

	// 3. 连接文案 —— 小字, 与星球名形成夸张巨细对比
	FlavorText = ERUI::MakeText(this, INVTEXT("连接中……我们所做的一切都是有意义的，对吧？"),
		EERTypeScale::Caption, T->BoneSecondary, false);
	FlavorText->SetFont(ERUI::FontRegular((int32)FlavorFontSize, 40));
	FlavorText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(FlavorText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 48.0f, 0.0f, 0.0f));
	}

	// 4.「接入虹膜」幽灵按钮 —— 发丝描边无底卡, 悬停反白; 接驳完成前 Hidden (保留占位不跳动)
	{
		EnterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnterButton"));
		FButtonStyle BtnStyle;
		BtnStyle.SetNormal(ERUI::TransparentBrush());
		BtnStyle.SetHovered(ERUI::TransparentBrush());
		BtnStyle.SetPressed(ERUI::TransparentBrush());
		BtnStyle.SetDisabled(ERUI::TransparentBrush());
		BtnStyle.SetNormalPadding(FMargin(0.0f));
		BtnStyle.SetPressedPadding(FMargin(0.0f));
		EnterButton->SetStyle(BtnStyle);
		EnterButton->SetCursor(EMouseCursor::Hand);
		EnterButton->OnClicked.AddDynamic(this, &ULegionDeployScreen::HandleEnterClicked);

		EnterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EnterBorder"));
		EnterBorder->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor::Transparent, 1.0f,
			FLinearColor(T->Bone.R, T->Bone.G, T->Bone.B, 0.55f)));
		EnterBorder->SetBrushColor(FLinearColor::White);
		EnterBorder->SetPadding(FMargin(36.0f, 13.0f));

		EnterText = ERUI::MakeText(this, INVTEXT("接入虹膜"), EERTypeScale::Body, T->Bone, false);
		EnterText->SetFont(ERUI::FontBold((int32)EnterFontSize, 140));
		EnterBorder->SetContent(EnterText);
		EnterButton->SetContent(EnterBorder);

		EnterButton->SetVisibility(ESlateVisibility::Hidden);
		if (UVerticalBoxSlot* S = CenterCol->AddChildToVerticalBox(EnterButton))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 88.0f, 0.0f, 0.0f));
		}
	}
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
		// 过渡模式: 无按钮无轮询, 纯静态视觉, 与关卡内接驳屏连续
		return;
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

void ULegionDeployScreen::RevealEnterButton()
{
	if (bEnterRevealed || !EnterButton) { return; }
	bEnterRevealed = true;
	EnterRevealAlpha = 0.0f;
	EnterButton->SetVisibility(ESlateVisibility::Visible);
	EnterButton->SetRenderOpacity(0.0f);
}

void ULegionDeployScreen::TickEnterButtonVisual()
{
	if (!EnterButton || !EnterBorder || !EnterText) { return; }

	const bool bHovered = EnterButton->IsHovered();
	const bool bPressed = EnterButton->IsPressed();
	if (bHovered == bBtnHovered && bPressed == bBtnPressed) { return; }
	bBtnHovered = bHovered;
	bBtnPressed = bPressed;

	const UERUITheme* T = ERUI::Theme();
	if (bHovered)
	{
		// 悬停反白: 骨白填满 + 墨字 (按压略沉)
		const float Dim = bPressed ? 0.82f : 1.0f;
		EnterBorder->SetBrush(ERUI::RoundedBrush(0.0f,
			FLinearColor(T->Bone.R * Dim, T->Bone.G * Dim, T->Bone.B * Dim, 1.0f), 1.0f, T->Bone));
		EnterText->SetColorAndOpacity(FSlateColor(T->PureInk));
	}
	else
	{
		EnterBorder->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor::Transparent, 1.0f,
			FLinearColor(T->Bone.R, T->Bone.G, T->Bone.B, 0.55f)));
		EnterText->SetColorAndOpacity(FSlateColor(T->Bone));
	}
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

	if (bReady && !bReadyApplied)
	{
		bReadyApplied = true;
		RevealEnterButton();
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
	// 按钮在亮起前处于 Hidden 态, 玩家无法提前点击; bReady 不再重复校验,
	// 以便自动化链路 (GameMode 已自行开战) 能经此路径强制撤屏
	if (bDismissing || bTransit) { return; }
	bDismissing = true;
	if (EnterButton) { EnterButton->SetVisibility(ESlateVisibility::Hidden); }

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

	// 「接入虹膜」淡入 (~0.3s)
	if (bEnterRevealed && EnterRevealAlpha < 1.0f)
	{
		EnterRevealAlpha = FMath::Min(1.0f, EnterRevealAlpha + InDeltaTime / 0.3f);
		if (EnterButton) { EnterButton->SetRenderOpacity(EnterRevealAlpha); }
	}
	TickEnterButtonVisual();

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

	PollAccum += InDeltaTime;
	if (PollAccum >= 0.15f)
	{
		PollAccum = 0.0f;
		PollReadiness();
	}

	// -DeployShot 离线验收链路: 就绪后稍作停留 (供 LegionDeployReady 截图), 自动点击「接入虹膜」;
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
