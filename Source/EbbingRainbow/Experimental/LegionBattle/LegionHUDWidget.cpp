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
		TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));

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
		StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));

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

void ULegionHUDWidget::SetStrategicMapMode(bool bMapOpen)
{
	if (LegionBar)
	{
		LegionBar->SetVisibility((bMapOpen || bResultShown) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
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
