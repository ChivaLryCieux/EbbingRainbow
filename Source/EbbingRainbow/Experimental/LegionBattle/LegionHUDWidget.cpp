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

	// ---- 左上: 虹彩竖条 + 委托/战场标题 (纯白排印 + 错位阴影) ----
	{
		TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));

		UERPrismBar* Prism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("Prism"));
		Prism->bVertical = true;
		Prism->BarSize = FVector2D(4.0f, 48.0f);
		TitleRow->AddChildToHorizontalBox(Prism)->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));

		UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));

		TitleText = ERUI::MakeText(this, INVTEXT("端点星"), EERTypeScale::Title, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f));
		ERUI::ApplyResonantText(TitleText, 44, true, 140, FLinearColor(0.99f, 0.99f, 0.98f, 1.0f), FVector2D(3.0f, 4.0f), 0.85f);
		TitleCol->AddChildToVerticalBox(TitleText);

		TitleRow->AddChildToHorizontalBox(TitleCol);
		Anchor(TitleRow, Root, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(T->ScreenMargin, 34.0f));
	}

	// ---- 左下: 双方编制存活 (无底卡纯白字 + 错位黑影 + 阵营竖标) ----
	{
		StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));

		auto MakeStatusRow = [&](const FLinearColor& BarCol, TObjectPtr<UTextBlock>& OutText) -> UHorizontalBox*
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

			UOverlay* BarOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			UImage* BarShadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			BarShadow->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 18.0f)));
			BarShadow->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.84f));
			BarShadow->SetRenderTranslation(FVector2D(1.5f, 2.0f));
			BarOv->AddChildToOverlay(BarShadow);

			UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Bar->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 18.0f)));
			Bar->SetColorAndOpacity(BarCol);
			BarOv->AddChildToOverlay(Bar);

			UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(BarOv);
			BS->SetVerticalAlignment(VAlign_Center);
			BS->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));

			OutText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), false);
			ERUI::ApplyResonantText(OutText, 17, true, 90, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), FVector2D(2.0f, 2.5f), 0.88f);
			Row->AddChildToHorizontalBox(OutText)->SetVerticalAlignment(VAlign_Center);
			return Row;
		};

		StatusCol->AddChildToVerticalBox(MakeStatusRow(T->AllyMark, StatusAllyText));
		StatusCol->AddChildToVerticalBox(MakeStatusRow(T->EnemyMark, StatusEnemyText))->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

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

	// ---- 极简战终结算层 (《控制共振》式建筑排版: 仅硕大纯白「胜利 / 失联」+ 下方一行小字「点击返回大厅」) ----
	{
		ResultOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultOverlay"));
		ResultOverlay->SetBrush(ERUI::WhiteBrush());
		ResultOverlay->SetBrushColor(FLinearColor(0.004f, 0.005f, 0.008f, 0.56f));
		ResultOverlay->SetHorizontalAlignment(HAlign_Center);
		ResultOverlay->SetVerticalAlignment(VAlign_Center);
		ResultOverlay->SetVisibility(ESlateVisibility::Collapsed);

		UVerticalBox* ResCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultCol"));
		ResultOverlay->SetContent(ResCol);

		ResultText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Hero, FLinearColor(0.98f, 0.98f, 0.98f, 1.0f), true);
		ResultText->SetFont(ERUI::FontBold(180, 320));
		ResultText->SetJustification(ETextJustify::Center);
		ResultText->SetShadowOffset(FVector2D(0.0f, 6.0f));
		ResultText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
		if (UVerticalBoxSlot* TitleSlot = ResCol->AddChildToVerticalBox(ResultText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}

		ReturnHallButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReturnHallButton"));
		{
			const FSlateBrush Tr = ERUI::TransparentBrush();
			FButtonStyle BS;
			BS.SetNormal(Tr);
			BS.SetHovered(Tr);
			BS.SetPressed(Tr);
			BS.SetDisabled(Tr);
			BS.SetNormalPadding(FMargin(28.0f, 12.0f));
			BS.SetPressedPadding(FMargin(28.0f, 12.0f));
			ReturnHallButton->SetStyle(BS);
			ReturnHallButton->SetCursor(EMouseCursor::Hand);
			ReturnHallButton->OnClicked.AddDynamic(this, &ULegionHUDWidget::OnReturnHallClicked);
		}

		ReturnHallLabel = ERUI::MakeText(this, INVTEXT("点击返回大厅"), EERTypeScale::Body,
			FLinearColor(0.97f, 0.97f, 0.97f, 0.78f), true);
		ReturnHallLabel->SetFont(ERUI::FontRegular(20, 240));
		ReturnHallLabel->SetJustification(ETextJustify::Center);
		ReturnHallButton->SetContent(ReturnHallLabel);

		if (UVerticalBoxSlot* BtnSlot = ResCol->AddChildToVerticalBox(ReturnHallButton))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Center);
			BtnSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
		}

		UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(ResultOverlay);
		OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		OverlaySlot->SetOffsets(FMargin(0.0f));
		OverlaySlot->SetZOrder(100);
	}

	// ---- 右上角势力 Logo 横排 (「接入 AI」按钮之上): 从右至左 = 虹膜 + 雇主 + 敌对,
	//      纯 logo 无底片, 纵横比自适应; 无合同时只留虹膜与叛军 (演武默认对手) ----
	{
		FactionLogoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FactionLogoRow"));

		EFactionId EnemyFaction = EFactionId::Rebels;
		bool bHasEmployer = false;
		EFactionId EmployerFaction = EFactionId::Empire;
		if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
		{
			if (Campaign->HasActiveContract())
			{
				const FGalacticContract& Contract = Campaign->GetActiveContract();
				EnemyFaction = Contract.EnemyFaction;
				EmployerFaction = Contract.Employer;
				bHasEmployer = true;
			}
		}

		// UMG 横排从左往右布局, 锚点对齐右缘: 先加最左侧 (敌对), 虹膜最后 => 停在最右
		auto AddLogo = [&](const TCHAR* Path)
		{
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, Path))
			{
				if (UImage* Img = ERUI::MakeLogoImage(this, Tex, 64.0f))
				{
					UHorizontalBoxSlot* S = FactionLogoRow->AddChildToHorizontalBox(Img);
					S->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
					S->SetVerticalAlignment(VAlign_Center);
				}
			}
		};
		AddLogo(ERGalacticMeta::GetFactionLogoAssetPath(EnemyFaction));
		if (bHasEmployer) { AddLogo(ERGalacticMeta::GetFactionLogoAssetPath(EmployerFaction)); }
		AddLogo(ERGalacticMeta::GetIrisLogoAssetPath());

		UCanvasPanelSlot* LogoSlot = Root->AddChildToCanvas(FactionLogoRow);
		LogoSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		LogoSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		LogoSlot->SetPosition(FVector2D(-T->ScreenMargin, 24.0f));
		LogoSlot->SetAutoSize(true);
		LogoSlot->SetZOrder(24);
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

	if (UWorld* World = GetWorld())
	{
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(World->GetAuthGameMode()))
		{
			if (!BattleEndHandle.IsValid())
			{
				BattleEndHandle = GM->OnLegionBattleEnd.AddUObject(this, &ULegionHUDWidget::HandleBattleEnded);
			}
			if (GM->IsBattleOver() && !bResultShown)
			{
				HandleBattleEnded(GM->DidPlayerWin());
			}
		}
	}
}

void ULegionHUDWidget::NativeDestruct()
{
	if (BattleEndHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (ALegionGameMode* GM = Cast<ALegionGameMode>(World->GetAuthGameMode()))
			{
				GM->OnLegionBattleEnd.Remove(BattleEndHandle);
			}
		}
		BattleEndHandle.Reset();
	}
	Super::NativeDestruct();
}

void ULegionHUDWidget::UpdateHUDVisibility()
{
	const bool bHideAll = bResultShown || bPauseMenuOpen;
	if (TitleRow)       { TitleRow->SetVisibility(bHideAll ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible); }
	if (StatusCol)      { StatusCol->SetVisibility(bHideAll ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible); }
	if (LegionBar)      { LegionBar->SetVisibility((bHideAll || bMapOpenCached) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible); }
	if (FactionLogoRow) { FactionLogoRow->SetVisibility((bHideAll || bMapOpenCached) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible); }
	if (AICommandPanel) { AICommandPanel->SetHostVisible(!bHideAll); }
}

void ULegionHUDWidget::SetStrategicMapMode(bool bMapOpen)
{
	bMapOpenCached = bMapOpen;
	UpdateHUDVisibility();
}

void ULegionHUDWidget::SetPauseMenuMode(bool bPaused)
{
	bPauseMenuOpen = bPaused;
	UpdateHUDVisibility();
}

void ULegionHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UWorld* World = GetWorld();
	ALegionGameMode* GM = World ? Cast<ALegionGameMode>(World->GetAuthGameMode()) : nullptr;
	if (GM && GM->IsBattleOver() && !bResultShown)
	{
		HandleBattleEnded(GM->DidPlayerWin());
	}

	if (bResultShown)
	{
		if (ReturnHallLabel && ReturnHallButton)
		{
			const bool bHov = ReturnHallButton->IsHovered() && !bReturningToHall;
			ReturnHallLabel->SetColorAndOpacity(FSlateColor(
				FLinearColor(0.98f, 0.98f, 0.98f, bHov ? 1.0f : 0.72f)));
		}
		return;
	}

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

		// 无底卡纯白字+错位阴影部队行: [竖条] [兵种图标] [旅级番号 / 兵员数量]
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Btn->SetContent(Row);

		UOverlay* BarOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* BarShadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		BarShadow->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 38.0f)));
		BarShadow->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.84f));
		BarShadow->SetRenderTranslation(FVector2D(1.5f, 2.0f));
		BarOv->AddChildToOverlay(BarShadow);

		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Bar->SetBrush(ERUI::WhiteBrush(FVector2D(3.0f, 38.0f)));
		Bar->SetColorAndOpacity(T->AllyMark);
		BarOv->AddChildToOverlay(Bar);

		if (UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarOv))
		{
			BarSlot->SetVerticalAlignment(VAlign_Center);
			BarSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		}

		LegionPlateBars.Add(Bar);
		LegionPlateSelected.Add(false);
		LegionPlateDead.Add(false);

		// 部队占位 Logo (轻薄暗影框保证与草地/天空边界锐利)
		UBorder* LogoBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LogoBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.010f, 0.014f, 0.56f), 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f)));
		LogoBox->SetBrushColor(FLinearColor::White);
		LogoBox->SetPadding(FMargin(2.0f));

		UImage* LogoImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush IconBrush;
		IconBrush.ImageSize = FVector2D(36.0f, 36.0f);
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
		LogoImg->SetBrush(IconBrush);
		LogoBox->SetContent(LogoImg);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LogoBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		}

		UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TextCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(TextCol))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 1. 部队名称 (如: 第一旅)
		UTextBlock* Name = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), false);
		ERUI::ApplyResonantText(Name, 18, true, 100, FLinearColor(0.99f, 0.99f, 0.98f, 0.96f), FVector2D(2.0f, 2.5f), 0.88f);
		TextCol->AddChildToVerticalBox(Name);

		// 2. 兵员数量
		UTextBlock* CountText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, FLinearColor(0.99f, 0.99f, 0.98f, 0.84f), false);
		ERUI::ApplyResonantText(CountText, 14, true, 60, FLinearColor(0.99f, 0.99f, 0.98f, 0.84f), FVector2D(1.5f, 2.0f), 0.88f);
		TextCol->AddChildToVerticalBox(CountText)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));

		LegionBar->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.0f, i > 0 ? 12.0f : 0.0f, 0.0f, 0.0f));
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
	if (Num != LegionPlateBars.Num())
	{
		LegionBar->ClearChildren();
		LegionPlateButtons.Reset();
		LegionPlateBars.Reset();
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
	const FLinearColor GoldWarn(0.99f, 0.72f, 0.24f, 0.96f);

	for (int32 i = 0; i < LegionPlateBars.Num(); ++i)
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
					Brush.ImageSize = FVector2D(36.0f, 36.0f);
					Brush.DrawAs = ESlateBrushDrawType::Image;
					LegionLogoImages[i]->SetBrush(Brush);
				}
			}
			LegionLogoImages[i]->SetColorAndOpacity(bDead ? FLinearColor(0.45f, 0.45f, 0.48f, 0.45f) : FLinearColor::White);
		}

		LegionNameTexts[i]->SetColorAndOpacity(FSlateColor(
			bDead ? FLinearColor(0.92f, 0.92f, 0.90f, 0.38f)
			      : (bSel ? FLinearColor(1.00f, 0.86f, 0.56f, 1.00f) : FLinearColor(0.99f, 0.99f, 0.98f, 0.96f))));
		const bool bDamaged = !bDead && L->GetAliveCount() < L->GetSoldierCount();
		LegionCountTexts[i]->SetColorAndOpacity(FSlateColor(
			bDead ? FLinearColor(0.92f, 0.92f, 0.90f, 0.35f) : (bDamaged ? GoldWarn : FLinearColor(0.99f, 0.99f, 0.98f, 0.84f))));
	}
}

void ULegionHUDWidget::RefreshPlateHover()
{
	const UERUITheme* T = ERUI::Theme();
	for (int32 i = 0; i < LegionPlateBars.Num(); ++i)
	{
		const bool bHover = LegionPlateButtons.IsValidIndex(i) && LegionPlateButtons[i] && LegionPlateButtons[i]->IsHovered();
		const bool bSel = LegionPlateSelected.IsValidIndex(i) && LegionPlateSelected[i];
		const bool bDead = LegionPlateDead.IsValidIndex(i) && LegionPlateDead[i];
		if (LegionPlateBars[i])
		{
			const FLinearColor BarCol = bDead
				? FLinearColor(0.6f, 0.6f, 0.62f, 0.35f)
				: (bSel ? T->Accent : (bHover ? FLinearColor(0.99f, 0.99f, 0.98f, 1.0f) : T->AllyMark));
			LegionPlateBars[i]->SetColorAndOpacity(BarCol);
		}
		if (LegionPlateButtons.IsValidIndex(i) && LegionPlateButtons[i])
		{
			LegionPlateButtons[i]->SetRenderTranslation(FVector2D((bHover || bSel) ? 6.0f : 0.0f, 0.0f));
		}
	}
}

void ULegionHUDWidget::SelectPlate(int32 Index)
{
	if (ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		PC->SelectLegionByIndex(Index);
	}
}

bool ULegionHUDWidget::IsResultOverlayVisible() const
{
	return ResultOverlay && ResultOverlay->GetVisibility() == ESlateVisibility::Visible;
}

bool ULegionHUDWidget::IsReturnHallButtonReady() const
{
	return ReturnHallButton && ReturnHallButton->IsVisible() && !bReturningToHall;
}

void ULegionHUDWidget::HandleBattleEnded(bool bPlayerWon)
{
	if (bResultShown) { return; }
	bResultShown = true;
	ShowResult(bPlayerWon);
}

void ULegionHUDWidget::OnReturnHallClicked()
{
	if (bReturningToHall) { return; }
	bReturningToHall = true;
	if (ReturnHallButton)
	{
		ReturnHallButton->SetIsEnabled(false);
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")));
	}
}

void ULegionHUDWidget::ShowResult(bool bPlayerWon)
{
	// 仅保留硕大纯白「胜利 / 失联」+ 下方「点击返回大厅」, 清空其余全部 HUD 元素
	if (TitleRow)       { TitleRow->SetVisibility(ESlateVisibility::Collapsed); }
	if (StatusCol)      { StatusCol->SetVisibility(ESlateVisibility::Collapsed); }
	if (FactionLogoRow) { FactionLogoRow->SetVisibility(ESlateVisibility::Collapsed); }
	if (LegionBar)      { LegionBar->SetVisibility(ESlateVisibility::Collapsed); }
	if (AICommandPanel) { AICommandPanel->SetHostVisible(false); }

	if (ResultText)
	{
		ResultText->SetText(bPlayerWon ? INVTEXT("胜利") : INVTEXT("失联"));
		ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.98f, 0.98f, 1.0f)));
	}
	if (ReturnHallButton)
	{
		ReturnHallButton->SetIsEnabled(true);
	}
	if (ReturnHallLabel)
	{
		if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
		{
			ReturnHallLabel->SetText(Campaign->GetSaveStatus() == EGalacticCampaignSaveStatus::WriteFailed
				? INVTEXT("进度未保存 · 返回大厅后可重试")
				: INVTEXT("点击返回大厅"));
		}
		ReturnHallLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.98f, 0.98f, 0.72f)));
	}
	if (ResultOverlay)
	{
		ResultOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	// 兜底确保玩家控制器进入战终纯 UI 模式 (鼠标可见 + 第一人称操控切断)
	if (ALegionPlayerController* PC = Cast<ALegionPlayerController>(GetOwningPlayer()))
	{
		PC->EnterBattleOverMode();
	}
}
