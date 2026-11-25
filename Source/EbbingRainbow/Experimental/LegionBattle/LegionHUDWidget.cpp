// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ERLog.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"
#include "Kismet/GameplayStatics.h"
#include "UI/ERActionButton.h"
#include "UI/ERUIStatics.h"
#include "UI/ERUITheme.h"
#include "UI/Primitives/ERTechPrimitives.h"

TSharedRef<SWidget> ULegionHUDWidget::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}
	return Super::RebuildWidget();
}

void ULegionHUDWidget::Build()
{
	const UERUITheme* T = ERUI::Theme();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	RootPanel = Root;

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

	// ---- 左上: 虹彩竖条 + 委托/战场标题 ----
	{
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));

		UERPrismBar* Prism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("Prism"));
		Prism->bVertical = true;
		Prism->BarSize = FVector2D(4.0f, 42.0f);
		TitleRow->AddChildToHorizontalBox(Prism)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));

		UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));

		TitleText = ERUI::MakeText(this, INVTEXT("端点星"), EERTypeScale::Title, T->Bone);
		TitleText->SetFont(ERUI::FontBold(38, 90));
		TitleCol->AddChildToVerticalBox(TitleText);

		TitleRow->AddChildToHorizontalBox(TitleCol);
		Anchor(TitleRow, Root, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(T->ScreenMargin, 34.0f));
	}

	// ---- 左下: 双方编制存活 (白底墨字卡片, 同兵团牌风格) ----
	{
		UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));

		ERUI::FBoneCard AllyCard = ERUI::MakeBoneCard(this, T->AllyMark, false, FMargin(14.0f, 7.0f));
		StatusAllyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::InkText(), false);
		StatusAllyText->SetFont(ERUI::FontBold(18, 60));
		AllyCard.Content->AddChildToVerticalBox(StatusAllyText);
		StatusCol->AddChildToVerticalBox(AllyCard.Root);

		ERUI::FBoneCard EnemyCard = ERUI::MakeBoneCard(this, T->EnemyMark, false, FMargin(14.0f, 7.0f));
		StatusEnemyText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::InkText(), false);
		StatusEnemyText->SetFont(ERUI::FontBold(18, 60));
		EnemyCard.Content->AddChildToVerticalBox(StatusEnemyText);
		StatusCol->AddChildToVerticalBox(EnemyCard.Root)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

		Anchor(StatusCol, Root, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f),
			FVector2D(T->ScreenMargin, -44.0f));
	}

	// ---- 左侧竖排: 部队卡片栏 (仅显示 部队名称 / Logo / 兵员数量 三要素) ----
	// 锚定在小地图正下方: 小地图为 Canvas 绘制, 1080p 基准占 Y 108~312 (占视口高 0.29),
	// 两者随视口高度同比例缩放, 故用比例锚点 0.31 保证任意分辨率不重叠
	{
		LegionBar = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LegionBar"));
		Anchor(LegionBar, Root, FAnchors(0.0f, 0.31f), FVector2D(0.0f, 0.0f),
			FVector2D(T->ScreenMargin, 0.0f));
	}

	// ---- 中央结算面板 ----
	{
		ResultBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultBox"));
		ResultBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.010f, 0.014f, 0.94f), 1.5f, T->Accent));
		ResultBox->SetBrushColor(FLinearColor::White);
		ResultBox->SetPadding(FMargin(56.0f, 38.0f));
		ResultBox->SetVisibility(ESlateVisibility::Collapsed);

		UVerticalBox* ResCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ResultBox->SetContent(ResCol);

		ResultText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Hero, T->Bone);
		ResultText->SetFont(ERUI::FontBold(84, 240));
		ResultText->SetJustification(ETextJustify::Center);
		ResCol->AddChildToVerticalBox(ResultText);

		ResultSummaryText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->BoneSecondary);
		ResultSummaryText->SetFont(ERUI::FontRegular(14, 60));
		ResultSummaryText->SetJustification(ETextJustify::Center);
		ResCol->AddChildToVerticalBox(ResultSummaryText)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 22.0f));

		ReturnHallButton = UERActionButton::Make(this, INVTEXT("返回虹膜基金会大厅"), 30, true);
		ReturnHallButton->SetMinSize(520.0f, 0.0f);
		ReturnHallButton->OnActionClicked.AddDynamic(this, &ULegionHUDWidget::OnReturnHallClicked);
		ResCol->AddChildToVerticalBox(ReturnHallButton);

		ResultSubText = ERUI::MakeText(this, INVTEXT("也可按 R 键就地重新部署沙盘"),
			EERTypeScale::Micro, T->BoneTertiary, false);
		ResultSubText->SetFont(ERUI::FontBold(15, 120));
		ResultSubText->SetJustification(ETextJustify::Center);
		ResCol->AddChildToVerticalBox(ResultSubText)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));

		Anchor(ResultBox, Root, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	}

	// ---- 右上角「接入 AI」自然语言指挥终端: 战场/地图双视角常驻
	// (战场视角按 Alt 呼出光标即可交互, 与兵团牌同一交互模型) ----
	AICommandPanel = UERAICommandPanel::InstallOnLegionMapHUD(this, Root, T->ScreenMargin);
	if (AICommandPanel)
	{
		AICommandPanel->SetHostVisible(true);
	}
}

void ULegionHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bBuilt)
	{
		bBuilt = true;
		Build();
	}

	// 左上角固定显示任务星球名 —— 当前「端点星」为占位, 后续应由委托/星区数据驱动
	if (TitleText) { TitleText->SetText(INVTEXT("端点星")); }
	RefreshStatus();
}

void ULegionHUDWidget::SetStrategicMapMode(bool bMapOpen)
{
	if (LegionBar)
	{
		LegionBar->SetVisibility(bMapOpen ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void ULegionHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetOwningPlayer()))
	{
		SetStrategicMapMode(PC->IsStrategicMapOpen());
	}

	RefreshPlateHover();

	PollTimer -= InDeltaTime;
	if (PollTimer > 0.0f) { return; }
	PollTimer = 0.2f;
	RefreshStatus();
	RefreshLegionBar();

	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (GM && GM->IsBattleOver() && !bResultShown)
	{
		bResultShown = true;
		ShowResult(GM->DidPlayerWin());
	}
}

void ULegionHUDWidget::RefreshStatus()
{
	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GM) { return; }

	const int32 AllyAlive = GM->CountAliveSoldiers(ELegionTeam::Player);
	const int32 EnemyAlive = GM->CountAliveSoldiers(ELegionTeam::Enemy);
	const int32 Pending = GM->CountPendingEnemyWaves();

	if (AllyAlive != CachedAllyAlive || EnemyAlive != CachedEnemyAlive || Pending != CachedPendingWaves)
	{
		CachedAllyAlive = AllyAlive;
		CachedEnemyAlive = EnemyAlive;
		CachedPendingWaves = Pending;

		const int32 AllyLegions = GM->CountAliveLegions(ELegionTeam::Player);
		const int32 EnemyLegions = GM->CountAliveLegions(ELegionTeam::Enemy);
		StatusAllyText->SetText(FText::FromString(FString::Printf(
			TEXT("我方  %d 旅 · %d 兵"), AllyLegions, AllyAlive)));
		StatusEnemyText->SetText(FText::FromString(FString::Printf(
			TEXT("敌方  %d 旅 · %d 兵"), EnemyLegions, EnemyAlive)));
	}
}

void ULegionHUDWidget::BuildLegionBar(int32 Count)
{
	if (!LegionBar) { return; }
	const UERUITheme* T = ERUI::Theme();
	const FSlateBrush Tr = ERUI::TransparentBrush();
	for (int32 i = 0; i < Count && i < 8; ++i)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle BS;
		BS.SetNormal(Tr); BS.SetHovered(Tr); BS.SetPressed(Tr); BS.SetDisabled(Tr);
		BS.SetNormalPadding(FMargin(0.0f)); BS.SetPressedPadding(FMargin(0.0f));
		Btn->SetStyle(BS);
		Btn->SetCursor(EMouseCursor::Hand);
		LegionPlateButtons.Add(Btn);
		switch (i)
		{
		case 0: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked0); break;
		case 1: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked1); break;
		case 2: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked2); break;
		case 3: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked3); break;
		case 4: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked4); break;
		case 5: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked5); break;
		case 6: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked6); break;
		default: Btn->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnPlateClicked7); break;
		}

		// 白底墨字彩条部队卡: 固定对称宽, 仅含 部队名称 + Logo + 兵员数量 三要素
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetMinDesiredWidth(132.0f);

		ERUI::FBoneCard Card = ERUI::MakeBoneCard(this, T->AllyMark, false, FMargin(14.0f, 8.0f));
		CardSize->AddChild(Card.Root);
		Btn->SetContent(CardSize);
		LegionPlateFrames.Add(Card.Frame);
		LegionPlateSelected.Add(false);
		LegionPlateDead.Add(false);
		UVerticalBox* Col = Card.Content;

		// 1. 部队名称 (如: 第一旅)
		UTextBlock* Name = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::InkText(), false);
		Name->SetFont(ERUI::FontBold(19, 80));
		Name->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Name))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}

		// 2. 部队占位 Logo (暗曜石方框 + 技能图标占位) —— 竖排空间紧凑, 图标 44px
		UBorder* LogoBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LogoBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.012f, 0.013f, 0.018f, 0.94f), 1.0f, FLinearColor(0.02f, 0.02f, 0.03f, 0.5f)));
		LogoBox->SetBrushColor(FLinearColor::White);
		LogoBox->SetPadding(FMargin(3.0f));

		UImage* LogoImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush IconBrush;
		IconBrush.ImageSize = FVector2D(44.0f, 44.0f);
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
		LogoImg->SetBrush(IconBrush);
		LogoBox->SetContent(LogoImg);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(LogoBox))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 5.0f));
		}

		// 3. 兵员数量
		UTextBlock* CountText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, ERUI::InkText(), false);
		CountText->SetFont(ERUI::FontBold(18, 40));
		CountText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(CountText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}

		LegionBar->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.0f, i > 0 ? 10.0f : 0.0f, 0.0f, 0.0f));
		LegionNameTexts.Add(Name);
		LegionLogoImages.Add(LogoImg);
		LegionCountTexts.Add(CountText);
		CachedCardLogos.Add(nullptr);
	}
}

void ULegionHUDWidget::RefreshLegionBar()
{
	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GM || !LegionBar) { return; }

	const int32 Num = GM->GetPlayerLegions().Num();
	if (Num != LegionPlateFrames.Num())
	{
		LegionBar->ClearChildren();
		LegionPlateButtons.Reset();
		LegionPlateFrames.Reset();
		LegionPlateSelected.Reset();
		LegionPlateDead.Reset();
		LegionNameTexts.Reset();
		LegionLogoImages.Reset();
		LegionCountTexts.Reset();
		CachedCardLogos.Reset();
		BuildLegionBar(Num);
	}
	if (Num == 0) { return; }

	ALegionPlayerController* PC = Cast<ALegionPlayerController>(World->GetFirstPlayerController());
	const FLinearColor InkAmber(0.58f, 0.31f, 0.0f, 1.0f);

	for (int32 i = 0; i < LegionPlateFrames.Num(); ++i)
	{
		ALegionUnit* L = GM->GetPlayerLegions()[i];
		if (!L) { continue; }
		const bool bSel = PC && PC->IsLegionSelected(L);
		const bool bDead = L->IsWiped();
		LegionPlateSelected[i] = bSel;
		LegionPlateDead[i] = bDead;

		const FString Name = L->GetCallsign().IsEmpty() ? ERGalacticMeta::GetBrigadeName(i + 1) : L->GetCallsign();
		LegionNameTexts[i]->SetText(FText::FromString(Name));
		LegionCountTexts[i]->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), L->GetAliveCount(), L->GetSoldierCount())));

		if (LegionLogoImages.IsValidIndex(i) && LegionLogoImages[i])
		{
			if (!CachedCardLogos.IsValidIndex(i) || !CachedCardLogos[i])
			{
				if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, ERGalacticMeta::GetClassLogoAssetPath(L->GetArchetype(), true)))
				{
					CachedCardLogos[i] = Tex;
					FSlateBrush Brush;
					Brush.SetResourceObject(Tex);
					Brush.ImageSize = FVector2D(44.0f, 44.0f);
					Brush.DrawAs = ESlateBrushDrawType::Image;
					LegionLogoImages[i]->SetBrush(Brush);
				}
			}
			LegionLogoImages[i]->SetColorAndOpacity(bDead ? FLinearColor(0.35f, 0.35f, 0.38f, 0.65f) : FLinearColor::White);
		}

		LegionNameTexts[i]->SetColorAndOpacity(FSlateColor(ERUI::InkText(bDead ? 0.38f : 1.0f)));
		const bool bDamaged = !bDead && L->GetAliveCount() < L->GetSoldierCount();
		LegionCountTexts[i]->SetColorAndOpacity(FSlateColor(
			bDead ? ERUI::InkText(0.35f) : (bDamaged ? InkAmber : ERUI::InkText(0.88f))));
	}
}

void ULegionHUDWidget::RefreshPlateHover()
{
	for (int32 i = 0; i < LegionPlateFrames.Num(); ++i)
	{
		const bool bHover = LegionPlateButtons.IsValidIndex(i) && LegionPlateButtons[i] && LegionPlateButtons[i]->IsHovered();
		const bool bSel = LegionPlateSelected.IsValidIndex(i) && LegionPlateSelected[i];
		const bool bDead = LegionPlateDead.IsValidIndex(i) && LegionPlateDead[i];
		const UERUITheme* T = ERUI::Theme();
		ERUI::StyleBoneFrame(LegionPlateFrames[i], bHover ? 1.0f : 0.0f, !bDead, bSel,
			bSel ? T->Accent : T->AllyMark, false);
	}
}

void ULegionHUDWidget::SelectPlate(int32 Index)
{
	if (ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		PC->SelectLegionByIndex(Index);
	}
}

void ULegionHUDWidget::OnReturnHallClicked()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")));
	}
}

void ULegionHUDWidget::ShowResult(bool bPlayerWon)
{
	const UERUITheme* T = ERUI::Theme();
	ResultText->SetText(bPlayerWon ? INVTEXT("胜  利") : INVTEXT("败  北"));
	ResultText->SetColorAndOpacity(FSlateColor(bPlayerWon ? T->Bone : T->DangerColor));

	FString Summary;
	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		const FLegionBattleOutcome& Out = Campaign->GetLastOutcome();
		if (Out.bValid && Out.bFromContract)
		{
			Summary = FString::Printf(
				TEXT("委托: %s\n基金经费 +%d 信用点  |  我方阵亡 %d 人  |  歼敌 %d 人\n%s 星区控制率 %+.1f%%"),
				*Out.ContractTitle,
				Out.CreditsEarned,
				Out.PlayerCasualties,
				Out.EnemyEliminated,
				*ERGalacticMeta::GetFactionName(Out.Employer),
				Out.ControlShiftApplied);
		}
		else if (Out.bValid)
		{
			Summary = FString::Printf(TEXT("演武结束  |  我方阵亡 %d 人  |  歼敌 %d 人"),
				Out.PlayerCasualties, Out.EnemyEliminated);
		}
	}
	if (ResultSummaryText)
	{
		ResultSummaryText->SetText(FText::FromString(Summary));
	}

	if (ResultBox)
	{
		ResultBox->SetVisibility(ESlateVisibility::Visible);
	}
}
