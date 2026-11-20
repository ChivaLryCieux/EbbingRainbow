// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERPersonnelPanel.h"

#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleSkillLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "Hall/HallRoster.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Run/RunSubsystem.h"
#include "UI/ERPersonnelMemberRow.h"
#include "UI/ERStatCell.h"
#include "UI/ERUIStatics.h"
#include "UI/Primitives/ERTechPrimitives.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	FSlateChildSize FillSize()
	{
		return FSlateChildSize(ESlateSizeRule::Fill);
	}

	FSlateFontInfo MakeFont(const UERUITheme* T, int32 Size, int32 Tracking = 0, bool bBold = false)
	{
		return T->GetFontExplicit(Size, bBold, Tracking);
	}
}

UERPersonnelPanel::UERPersonnelPanel()
{
	PlaceholderPortrait = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/UI/Textures/T_UI_HeroPortrait_Placeholder.T_UI_HeroPortrait_Placeholder")));
	SetIsFocusable(true);
}

TSharedRef<SWidget> UERPersonnelPanel::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder && !PortraitImage) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERPersonnelPanel::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder && !PortraitImage) { Build(); }
	}
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		RefreshRoster();
		if (bPreviewInDesigner)
		{
			SetVisibility(ESlateVisibility::Visible);
			SetRenderOpacity(1.0f);
		}
	}
}

void UERPersonnelPanel::NativeConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder && !PortraitImage) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();

	if (ScrimImage)
	{
		ScrimImage->SetColorAndOpacity(FLinearColor(0.005f, 0.006f, 0.009f, 0.985f));
	}

	if (!IsDesignTime())
	{
		bPanelOpen = false;
		bClosing = false;
		AnimDirection = 0.0f;
		AnimT = 1.0f;
		SetRenderOpacity(1.0f);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UERPersonnelPanel::BindDelegates()
{
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UERPersonnelPanel::HandleCloseClicked);
	}
}

void UERPersonnelPanel::Build()
{
	const UERUITheme* T = ERUI::Theme();

	// 全屏画布 (100% x 100% 明日方舟干员档案 × 控制:共振 巨构平面)
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Root->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Root;

	// 1. 全屏哑光黑曜石底幕 (#050609)
	ScrimImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ScrimImage"));
	ScrimImage->SetBrush(ERUI::WhiteBrush());
	ScrimImage->SetColorAndOpacity(FLinearColor(0.005f, 0.006f, 0.009f, 0.985f));
	ScrimImage->SetVisibility(ESlateVisibility::Visible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(ScrimImage);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// 2. 顶部 3px 全屏贯穿《虹不我与》光谱细线 (黑-白主调唯一一缕彩光)
	TopEdgePrism = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("TopEdgePrism"));
	TopEdgePrism->bVertical = false;
	TopEdgePrism->BarSize = FVector2D(1920.0f, 3.0f);
	TopEdgePrism->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(TopEdgePrism);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	// 3. 右半屏巨幅建筑背景水印代号 (148pt Linux Biolinum Bold, 4.5% 极淡存在感)
	WatermarkCallsignText = ERUI::MakeText(this, INVTEXT("BASALT"), EERTypeScale::Hero, FLinearColor(0.92f, 0.92f, 0.90f, 0.045f), true);
	WatermarkCallsignText->SetFont(MakeFont(T, 148, 120, true));
	WatermarkCallsignText->SetJustification(ETextJustify::Right);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(WatermarkCallsignText);
		S->SetAnchors(FAnchors(1.0f, 0.5f));
		S->SetAlignment(FVector2D(1.0f, 0.5f));
		S->SetPosition(FVector2D(-28.0f, -140.0f));
		S->SetAutoSize(true);
	}

	// 4. 左侧全高人物原画舞台 (铺满左边 58% 屏幕宽 × 100% 屏幕高!)
	UOverlay* PortraitStageOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PortraitStage"));
	PortraitStageOv->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PortraitStage = PortraitStageOv;
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(PortraitStageOv);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 0.58f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	UScaleBox* PortraitScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("PortraitScaleBox"));
	PortraitScale->SetStretch(EStretch::ScaleToFit);
	PortraitScale->SetStretchDirection(EStretchDirection::Both);
	PortraitScale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = PortraitStageOv->AddChildToOverlay(PortraitScale);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
	PortraitImage->SetBrush(ERUI::WhiteBrush(FVector2D(1024.0f, 1360.0f)));
	PortraitImage->SetColorAndOpacity(FLinearColor::White);
	PortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PortraitScale->SetContent(PortraitImage);

	// 底部渐压遮罩: 让底部浮动编队切换条与原画自然融为一体
	PortraitScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitScrim"));
	PortraitScrim->SetBrush(ERUI::ScrimBrush());
	PortraitScrim->SetColorAndOpacity(FLinearColor(0.005f, 0.006f, 0.009f, 0.96f));
	PortraitScrim->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	{
		FWidgetTransform Flip;
		Flip.Scale = FVector2D(1.0f, -1.0f);
		PortraitScrim->SetRenderTransform(Flip);
	}
	PortraitScrim->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(PortraitScrim);
		S->SetAnchors(FAnchors(0.0f, 1.0f, 0.62f, 1.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f, -260.0f, 0.0f, 0.0f));
	}

	// 5. 全屏静谧光学四角标尺
	CornerReticle = WidgetTree->ConstructWidget<UERCornerReticle>(UERCornerReticle::StaticClass(), TEXT("CornerReticle"));
	CornerReticle->TopRightCode = TEXT("");
	CornerReticle->BottomRightCode = TEXT("ARCHIVE // FULLSCREEN");
	CornerReticle->ReticleColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.24f);
	CornerReticle->bDrawCenterTicks = false;
	CornerReticle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(CornerReticle);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// 6. 左上浮层标题碑 (不遮挡人物主体, 黑底微衬确保高可读)
	{
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
		HeaderRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		HeaderPrismBar = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("HeaderPrismBar"));
		HeaderPrismBar->bVertical = true;
		HeaderPrismBar->BarSize = FVector2D(4.0f, 44.0f);
		HeaderPrismBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		HeaderRow->AddChildToHorizontalBox(HeaderPrismBar)->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));
		TitleCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* TSlot = HeaderRow->AddChildToHorizontalBox(TitleCol);
		TSlot->SetVerticalAlignment(VAlign_Center);
		TSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));

		HeaderCaption = ERUI::MakeText(this, INVTEXT("OPERATOR DOSSIER // SQUAD MANIFEST"), EERTypeScale::Micro, T->BoneSecondary, true);
		HeaderCaption->SetFont(MakeFont(T, 9, 280, false));
		TitleCol->AddChildToVerticalBox(HeaderCaption);

		HeaderTitle = ERUI::MakeText(this, INVTEXT("人员档案"), EERTypeScale::Title, T->Bone);
		HeaderTitle->SetFont(MakeFont(T, 32, 80, true));
		HeaderTitle->SetShadowOffset(FVector2D(0.0f, 2.0f));
		HeaderTitle->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		TitleCol->AddChildToVerticalBox(HeaderTitle);

		UCanvasPanelSlot* S = Root->AddChildToCanvas(HeaderRow);
		S->SetAnchors(FAnchors(0.0f, 0.0f));
		S->SetPosition(FVector2D(44.0f, 36.0f));
		S->SetAutoSize(true);
	}

	// 7. 右上编队状态 + 锐利直角关闭按钮 [ × ESC ]
	{
		UHorizontalBox* TopRightRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopRightRow"));
		TopRightRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));
		StatusCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* SSlot = TopRightRow->AddChildToHorizontalBox(StatusCol);
		SSlot->SetVerticalAlignment(VAlign_Center);
		SSlot->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));

		SquadSummaryText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, true);
		SquadSummaryText->SetJustification(ETextJustify::Right);
		SquadSummaryText->SetFont(MakeFont(T, 18, 80, true));
		StatusCol->AddChildToVerticalBox(SquadSummaryText);

		SquadStateText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
		SquadStateText->SetJustification(ETextJustify::Right);
		SquadStateText->SetFont(MakeFont(T, 8, 180, false));
		StatusCol->AddChildToVerticalBox(SquadStateText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		{
			FButtonStyle Style;
			const FSlateBrush Tr = ERUI::TransparentBrush();
			Style.SetNormal(Tr); Style.SetHovered(Tr); Style.SetPressed(Tr); Style.SetDisabled(Tr);
			Style.SetNormalPadding(FMargin(0.0f)); Style.SetPressedPadding(FMargin(0.0f));
			CloseButton->SetStyle(Style);
			CloseButton->SetCursor(EMouseCursor::Hand);
		}
		{
			UBorder* CloseBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CloseRing"));
			CloseBox->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.015f, 0.017f, 0.022f, 0.92f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.36f)));
			CloseBox->SetBrushColor(FLinearColor::White);
			CloseBox->SetPadding(FMargin(16.0f, 8.0f));
			CloseButton->SetContent(CloseBox);

			CloseGlyphText = ERUI::MakeText(this, INVTEXT("×  ESC"), EERTypeScale::Micro, T->Bone, true);
			CloseGlyphText->SetFont(MakeFont(T, 11, 200, true));
			CloseBox->SetContent(CloseGlyphText);
		}
		TopRightRow->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);

		UCanvasPanelSlot* S = Root->AddChildToCanvas(TopRightRow);
		S->SetAnchors(FAnchors(1.0f, 0.0f));
		S->SetAlignment(FVector2D(1.0f, 0.0f));
		S->SetPosition(FVector2D(-44.0f, 36.0f));
		S->SetAutoSize(true);
	}

	// 8. 左下浮动编队横向切换条 (不抢占原画高度, [01 磐] [02 燧] [03 霆] [04 蚀])
	{
		UVerticalBox* DockCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OperatorDockCol"));
		DockCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UHorizontalBox* IdxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DockHeaderRow"));
		IdxRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DockCol->AddChildToVerticalBox(IdxRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		UBorder* IdxTag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortraitIndexTag"));
		IdxTag->SetBrush(ERUI::RoundedBrush(0.0f, T->Bone));
		IdxTag->SetBrushColor(FLinearColor::White);
		IdxTag->SetPadding(FMargin(10.0f, 3.0f));
		PortraitIndexText = ERUI::MakeText(this, INVTEXT("01 / 04"), EERTypeScale::Micro, T->PureInk, true);
		PortraitIndexText->SetFont(MakeFont(T, 11, 160, true));
		IdxTag->SetContent(PortraitIndexText);
		IdxRow->AddChildToHorizontalBox(IdxTag)->SetVerticalAlignment(VAlign_Center);

		PortraitCaptionText = ERUI::MakeText(this, INVTEXT("SQUAD SELECT // [1-4] 切换干员档案"), EERTypeScale::Micro, T->BoneSecondary, true);
		PortraitCaptionText->SetFont(MakeFont(T, 8, 220, false));
		UHorizontalBoxSlot* CapSlot = IdxRow->AddChildToHorizontalBox(PortraitCaptionText);
		CapSlot->SetVerticalAlignment(VAlign_Center);
		CapSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));

		UHorizontalBox* HList = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MemberListBox"));
		HList->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		MemberListBox = HList;
		DockCol->AddChildToVerticalBox(HList);

		UCanvasPanelSlot* S = Root->AddChildToCanvas(DockCol);
		S->SetAnchors(FAnchors(0.0f, 1.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
		S->SetPosition(FVector2D(44.0f, -38.0f));
		S->SetAutoSize(true);
	}

	// 9. 右半屏 Control: 共振式巨构档案区 (锚定右侧 43% 全高, 极度留白与大小字对比)
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrush(ERUI::TransparentBrush());
	PanelBorder->SetBrushColor(FLinearColor::White);
	PanelBorder->SetPadding(FMargin(0.0f));
	PanelBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(PanelBorder);
		S->SetAnchors(FAnchors(0.56f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(24.0f, 116.0f, 52.0f, 54.0f));
	}

	UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailContainer"));
	InfoCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DetailContainer = InfoCol;
	PanelBorder->SetContent(InfoCol);

	// 92pt 巨型粗体名 + 17pt 宽字距 Linux Biolinum 代号
	NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Hero, T->Bone);
	NameText->SetFont(MakeFont(T, 92, 20, true));
	InfoCol->AddChildToVerticalBox(NameText);

	CallsignText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Accent, true);
	CallsignText->SetFont(MakeFont(T, 17, 360, true));
	InfoCol->AddChildToVerticalBox(CallsignText)->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));

	UHorizontalBox* SchoolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SchoolRow"));
	SchoolRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SchoolRow)->SetPadding(FMargin(2.0f, 16.0f, 0.0f, 0.0f));

	SchoolChipBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SchoolChipBorder"));
	SchoolChipBorder->SetBrush(ERUI::RoundedBrush(0.0f, T->Bone));
	SchoolChipBorder->SetBrushColor(FLinearColor::White);
	SchoolChipBorder->SetPadding(FMargin(12.0f, 4.0f));
	SchoolRow->AddChildToHorizontalBox(SchoolChipBorder)->SetVerticalAlignment(VAlign_Center);

	SchoolChipText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->PureInk);
	SchoolChipText->SetFont(MakeFont(T, 10, 140, true));
	SchoolChipBorder->SetContent(SchoolChipText);

	RoleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, true);
	RoleText->SetFont(MakeFont(T, 11, 200, false));
	{
		UHorizontalBoxSlot* S = SchoolRow->AddChildToHorizontalBox(RoleText);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	}

	TaglineText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, T->BoneSecondary);
	TaglineText->SetAutoWrapText(true);
	TaglineText->SetFont(MakeFont(T, 13, 40, false));
	InfoCol->AddChildToVerticalBox(TaglineText)->SetPadding(FMargin(2.0f, 16.0f, 0.0f, 0.0f));

	InfoDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("InfoDivider"));
	InfoDivider->AnchorLength = 36.0f;
	InfoDivider->AnchorColor = T->Accent;
	InfoDivider->LineColor = T->BoneGhost;
	InfoDivider->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(InfoDivider)->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));

	StatsTitleText = ERUI::MakeText(this, INVTEXT("基本属性 // ATTRIBUTES"), EERTypeScale::Micro, T->BoneTertiary, true);
	StatsTitleText->SetFont(MakeFont(T, 9, 240, false));
	InfoCol->AddChildToVerticalBox(StatsTitleText)->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));

	PrimaryStatsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PrimaryStatsBox"));
	PrimaryStatsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(PrimaryStatsBox)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	SecondaryStatsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SecondaryStatsBox"));
	SecondaryStatsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SecondaryStatsBox)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));

	ResistTitleText = ERUI::MakeText(this, INVTEXT("四学派抗性 // RESISTANCE"), EERTypeScale::Micro, T->BoneTertiary, true);
	ResistTitleText->SetFont(MakeFont(T, 9, 240, false));
	InfoCol->AddChildToVerticalBox(ResistTitleText)->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));

	ResistBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResistBox"));
	ResistBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(ResistBox)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	SkillsTitleText = ERUI::MakeText(this, INVTEXT("战术技能 // LOADOUT"), EERTypeScale::Micro, T->BoneTertiary, true);
	SkillsTitleText->SetFont(MakeFont(T, 9, 240, false));
	InfoCol->AddChildToVerticalBox(SkillsTitleText)->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));

	SkillsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillsBox"));
	SkillsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SkillsBox)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	InfoCol->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DetailBottomSpacer")))
		->SetSize(FillSize());

	FooterHintText = ERUI::MakeText(this, INVTEXT("EBBING RAINBOW // OPERATOR ARCHIVE SYSTEM"), EERTypeScale::Micro, ERUI::Fade(T->BoneTertiary, 0.7f), true);
	FooterHintText->SetFont(MakeFont(T, 8, 260, false));
	InfoCol->AddChildToVerticalBox(FooterHintText);
}

void UERPersonnelPanel::OpenPanel()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder && !PortraitImage) { Build(); }
	}

	RefreshRoster();

	bPanelOpen = true;
	bClosing = false;
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();

	if (bEnableEntranceAnimation)
	{
		AnimT = 0.0f;
		AnimDirection = 1.0f;
		SetRenderOpacity(0.0f);
		HeaderCaptionCipher.Start(TEXT("OPERATOR DOSSIER // SQUAD MANIFEST"), 0.42f, 0.05f);
		DetailTrack.Trigger(0.55f);
		if (Profiles.IsValidIndex(SelectedIndex))
		{
			CallsignCipher.Start(Profiles[SelectedIndex].Callsign.ToString(), 0.36f, 0.08f);
		}
	}
	else
	{
		AnimT = 1.0f;
		AnimDirection = 0.0f;
		SetRenderOpacity(1.0f);
		DetailTrack.FinishImmediately();
	}

	if (!IsDesignTime())
	{
		BP_OnPanelOpened();
	}
}

void UERPersonnelPanel::ClosePanel()
{
	if (!bPanelOpen || bClosing)
	{
		return;
	}
	bClosing = true;

	if (bEnableEntranceAnimation)
	{
		AnimDirection = -1.0f;
	}
	else
	{
		AnimT = 0.0f;
		AnimDirection = -1.0f;
		UpdateAnimation(0.0f);
	}
}

void UERPersonnelPanel::UpdateAnimation(float DeltaTime)
{
	if (AnimDirection == 0.0f)
	{
		return;
	}

	const float Duration = FMath::Max(0.05f, EntranceDuration);
	AnimT = FMath::Clamp(AnimT + AnimDirection * (DeltaTime / Duration), 0.0f, 1.0f);
	const float Ease = (AnimDirection > 0.0f) ? ERMotion::ExpoOut(AnimT) : ERMotion::CubicOut(AnimT);

	SetRenderOpacity(Ease);
	if (TopEdgePrism)   { TopEdgePrism->SetRevealAlpha(Ease); }
	if (CornerReticle)  { CornerReticle->SetRevealAlpha(Ease); }
	if (HeaderPrismBar) { HeaderPrismBar->SetRevealAlpha(Ease); }
	if (HeaderDivider)  { HeaderDivider->SetRevealAlpha(Ease); }

	if (AnimDirection < 0.0f && AnimT <= 0.0f)
	{
		AnimDirection = 0.0f;
		bClosing = false;
		bPanelOpen = false;
		SetRenderOpacity(1.0f);
		SetVisibility(ESlateVisibility::Collapsed);
		OnPanelClosed.Broadcast();
		if (!IsDesignTime())
		{
			BP_OnPanelClosed();
		}
	}
}

void UERPersonnelPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateAnimation(InDeltaTime);

	if (DetailTrack.Tick(InDeltaTime))
	{
		const float ArtE  = DetailTrack.Quint(0.00f, 0.44f);
		const float InfoE = DetailTrack.Expo(0.04f, 0.38f);
		const float DivE  = DetailTrack.Expo(0.10f, 0.40f);
		if (PortraitStage)
		{
			PortraitStage->SetRenderOpacity(FMath::Lerp(0.18f, 1.0f, ArtE));
			PortraitStage->SetRenderTranslation(FVector2D(-26.0f * (1.0f - ArtE), 0.0f));
		}
		if (DetailContainer)
		{
			DetailContainer->SetRenderOpacity(FMath::Lerp(0.18f, 1.0f, InfoE));
			DetailContainer->SetRenderTranslation(FVector2D(22.0f * (1.0f - InfoE), 0.0f));
		}
		if (WatermarkCallsignText)
		{
			WatermarkCallsignText->SetRenderTranslation(FVector2D(32.0f * (1.0f - ArtE), 0.0f));
		}
		if (InfoDivider)
		{
			InfoDivider->SetRevealAlpha(DivE);
		}
	}

	FString Decoded;
	if (HeaderCaptionCipher.Tick(InDeltaTime, Decoded) && HeaderCaption)
	{
		HeaderCaption->SetText(FText::FromString(Decoded));
	}
	if (CallsignCipher.Tick(InDeltaTime, Decoded) && CallsignText)
	{
		CallsignText->SetText(FText::FromString(Decoded));
	}
}

FReply UERPersonnelPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bPanelOpen && bCloseOnOutsideClick && PanelBorder
		&& !PanelBorder->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		ClosePanel();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UERPersonnelPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bPanelOpen)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		ClosePanel();
		return FReply::Handled();
	}
	if (Profiles.Num() > 0)
	{
		if (Key == EKeys::Left || Key == EKeys::A)
		{
			SelectMember((SelectedIndex + Profiles.Num() - 1) % Profiles.Num());
			return FReply::Handled();
		}
		if (Key == EKeys::Right || Key == EKeys::D)
		{
			SelectMember((SelectedIndex + 1) % Profiles.Num());
			return FReply::Handled();
		}
		if (Key == EKeys::One   && Profiles.IsValidIndex(0)) { SelectMember(0); return FReply::Handled(); }
		if (Key == EKeys::Two   && Profiles.IsValidIndex(1)) { SelectMember(1); return FReply::Handled(); }
		if (Key == EKeys::Three && Profiles.IsValidIndex(2)) { SelectMember(2); return FReply::Handled(); }
		if (Key == EKeys::Four  && Profiles.IsValidIndex(3)) { SelectMember(3); return FReply::Handled(); }
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UERPersonnelPanel::RefreshRoster()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder && !PortraitImage) { Build(); }
	}

	Profiles = ERHallRoster::GetProfiles();
	SelectedIndex = FMath::Clamp(SelectedIndex, 0, FMath::Max(0, Profiles.Num() - 1));

	BuildMemberRows();
	RefreshHeader();
	RefreshDetail();
}

void UERPersonnelPanel::BuildMemberRows()
{
	if (!MemberListBox || !WidgetTree)
	{
		return;
	}

	MemberListBox->ClearChildren();
	MemberRows.Reset();

	const TSubclassOf<UERPersonnelMemberRow> RowClass = MemberRowClass
		? MemberRowClass
		: TSubclassOf<UERPersonnelMemberRow>(UERPersonnelMemberRow::StaticClass());

	UHorizontalBox* AsHBox = Cast<UHorizontalBox>(MemberListBox.Get());
	UVerticalBox* AsVBox = Cast<UVerticalBox>(MemberListBox.Get());

	for (int32 Index = 0; Index < Profiles.Num(); ++Index)
	{
		UERPersonnelMemberRow* Row = WidgetTree->ConstructWidget<UERPersonnelMemberRow>(RowClass);
		if (!Row)
		{
			continue;
		}
		Row->DefaultPortrait = ResolvePortrait(Profiles[Index]);
		Row->Setup(Index, Profiles[Index], GetShownIntegrity(Index), HasRunState(), Index == SelectedIndex);
		Row->OnMemberSelected.AddUniqueDynamic(this, &UERPersonnelPanel::HandleMemberSelected);

		if (AsHBox)
		{
			UHorizontalBoxSlot* S = AsHBox->AddChildToHorizontalBox(Row);
			S->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		else if (AsVBox)
		{
			UVerticalBoxSlot* S = AsVBox->AddChildToVerticalBox(Row);
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}
		else
		{
			MemberListBox->AddChild(Row);
		}
		MemberRows.Add(Row);
	}
}

void UERPersonnelPanel::RefreshSelectionVisuals()
{
	for (int32 Index = 0; Index < MemberRows.Num(); ++Index)
	{
		if (UERPersonnelMemberRow* Row = MemberRows[Index].Get())
		{
			Row->SetSelected(Index == SelectedIndex);
		}
	}
}

void UERPersonnelPanel::RefreshHeader()
{
	if (SquadSummaryText)
	{
		SquadSummaryText->SetText(FText::FromString(FString::Printf(TEXT("编队 %02d / %02d"), Profiles.Num(), Profiles.Num())));
	}
	if (SquadStateText)
	{
		const URunSubsystem* Run = GetRunSubsystem();
		if (Run && Run->HasActiveRun())
		{
			float Sum = 0.0f;
			float MaxSum = 0.0f;
			for (const FRunUnitState& Unit : Run->GetParty())
			{
				Sum += Unit.Integrity;
				MaxSum += Unit.Stats.MaxIntegrity;
			}
			const int32 Percent = MaxSum > 0.0f ? FMath::RoundToInt(100.0f * Sum / MaxSum) : 100;
			SquadStateText->SetText(FText::FromString(FString::Printf(TEXT("EXPEDITION ACTIVE // 平均完整度 %d%%"), Percent)));
		}
		else
		{
			SquadStateText->SetText(INVTEXT("STANDBY // 尚未启程"));
		}
	}
}

void UERPersonnelPanel::RefreshDetail()
{
	if (!Profiles.IsValidIndex(SelectedIndex))
	{
		return;
	}

	const UERUITheme* T = ERUI::Theme();
	const FHallHeroProfile& Profile = Profiles[SelectedIndex];
	const FBattleUnitStats& Stats = Profile.Stats;
	const FLinearColor SchoolColor = Profile.GetSchoolColor();

	if (WatermarkCallsignText)
	{
		WatermarkCallsignText->SetText(Profile.Callsign);
	}
	if (NameText)
	{
		NameText->SetText(Profile.HeroName);
	}
	if (CallsignText)
	{
		CallsignText->SetText(Profile.Callsign);
		CallsignText->SetColorAndOpacity(FSlateColor(SchoolColor));
	}
	if (SchoolChipText)
	{
		SchoolChipText->SetText(FText::FromString(FString::Printf(TEXT("学派 · %s"), *Profile.GetSchoolName().ToString())));
		SchoolChipText->SetColorAndOpacity(FSlateColor(T->PureInk));
	}
	if (SchoolChipBorder)
	{
		SchoolChipBorder->SetBrush(ERUI::RoundedBrush(0.0f, SchoolColor));
		SchoolChipBorder->SetBrushColor(FLinearColor::White);
	}
	if (InfoDivider)
	{
		InfoDivider->SetDividerColors(T->BoneGhost, SchoolColor);
	}
	if (RoleText)
	{
		RoleText->SetText(FText::FromString(FString::Printf(TEXT("%s  //  %s"),
			*Profile.RoleTitle.ToString(), *Profile.RoleCaption.ToString())));
	}
	if (TaglineText)
	{
		TaglineText->SetText(Profile.Tagline);
	}

	// ---- 全屏左半幅立绘 ----
	UTexture2D* PortraitTexture = ResolvePortrait(Profile);
	if (PortraitImage)
	{
		if (PortraitTexture)
		{
			PortraitImage->SetBrushFromTexture(PortraitTexture, true);
			PortraitImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			PortraitImage->SetBrush(ERUI::WhiteBrush(FVector2D(1024.0f, 1360.0f)));
			PortraitImage->SetColorAndOpacity(FLinearColor(0.05f, 0.06f, 0.08f, 1.0f));
		}
	}
	if (PortraitIndexText)
	{
		PortraitIndexText->SetText(FText::FromString(FString::Printf(TEXT("%02d / %02d"), SelectedIndex + 1, Profiles.Num())));
	}

	// ---- 基本属性 ----
	if (PrimaryStatsBox)
	{
		PrimaryStatsBox->ClearChildren();

		const float ShownIntegrity = GetShownIntegrity(SelectedIndex);
		const bool bLow = ShownIntegrity < Stats.MaxIntegrity * 0.35f;
		const FLinearColor IntegrityTint = bLow ? T->DangerColor : T->Bone;
		const FText IntegrityValue = HasRunState()
			? FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(ShownIntegrity), FMath::RoundToInt(Stats.MaxIntegrity)))
			: FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(Stats.MaxIntegrity)));

		MakeStatCell(PrimaryStatsBox, INVTEXT("完整度 // HP"), IntegrityValue, IntegrityTint);
		MakeStatCell(PrimaryStatsBox, INVTEXT("行动点 // AP"), FText::FromString(FString::Printf(TEXT("%d"), Stats.MaxAP)), T->Bone);
		MakeStatCell(PrimaryStatsBox, INVTEXT("先攻 // INIT"), FText::FromString(FString::Printf(TEXT("%d"), Stats.Initiative)), T->Bone);
		MakeStatCell(PrimaryStatsBox, INVTEXT("闪避 // EVA"), FormatPercent(Stats.EvasionRate, false), T->Bone);
	}

	if (SecondaryStatsBox)
	{
		SecondaryStatsBox->ClearChildren();
		MakeStatCell(SecondaryStatsBox, INVTEXT("暴击 // CRIT"), FormatPercent(Stats.CritChance, false), T->BoneSecondary);
		MakeStatCell(SecondaryStatsBox, INVTEXT("暴伤 // MULT"), FormatPercent(Stats.CritMultiplier, false), T->BoneSecondary);
		MakeStatCell(SecondaryStatsBox, INVTEXT("命中 // ACC"), FormatPercent(Stats.Accuracy, true), T->BoneSecondary);
	}

	// ---- 四学派抗性 ----
	if (ResistBox)
	{
		ResistBox->ClearChildren();
		auto AddResist = [this, T](EDamageType School, float Value)
		{
			const FLinearColor Color = ERHallRoster::GetSchoolColor(School);
			const FLinearColor Tint = Value > 0.0f
				? Color
				: (Value < 0.0f ? T->DangerColor : T->BoneTertiary);
			MakeStatCell(ResistBox, ERHallRoster::GetSchoolName(School), FormatPercent(Value, true), Tint);
		};
		AddResist(EDamageType::Kinetic, Stats.KineticResistance);
		AddResist(EDamageType::Thermal, Stats.ThermalResistance);
		AddResist(EDamageType::Electric, Stats.ElectricResistance);
		AddResist(EDamageType::Chemical, Stats.ChemicalResistance);
	}

	PopulateSkills(Profile);
	RefreshSelectionVisuals();
}

void UERPersonnelPanel::PopulateSkills(const FHallHeroProfile& Profile)
{
	if (!SkillsBox || !WidgetTree)
	{
		return;
	}
	SkillsBox->ClearChildren();

	const UERUITheme* T = ERUI::Theme();
	const FLinearColor SchoolColor = Profile.GetSchoolColor();
	UBattleSkillLibrary* Library = UBattleSkillLibrary::Get(ResolvePlayWorld());

	TArray<FText> SkillNames;
	for (const FName& SkillId : Profile.Stats.SkillIds)
	{
		FText Label = FText::FromString(SkillId.ToString());
		if (Library)
		{
			if (const UBattleSkillData* Data = Library->FindSkillById(SkillId))
			{
				if (!Data->DisplayName.IsEmpty())
				{
					Label = Data->DisplayName;
				}
			}
		}
		SkillNames.Add(Label);
	}
	if (SkillNames.Num() == 0)
	{
		SkillNames.Add(INVTEXT("—"));
	}

	for (const FText& Label : SkillNames)
	{
		UBorder* Chip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Chip->SetBrush(ERUI::RoundedBrush(0.0f,
			FLinearColor(0.015f, 0.017f, 0.022f, 0.94f), 1.0f,
			FLinearColor(0.92f, 0.92f, 0.90f, 0.24f)));
		Chip->SetBrushColor(FLinearColor::White);
		Chip->SetPadding(FMargin(12.0f, 6.0f));
		Chip->SetHorizontalAlignment(HAlign_Center);
		Chip->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ChipRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Chip->SetContent(ChipRow);

		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Bar->SetBrush(ERUI::WhiteBrush(FVector2D(2.5f, 12.0f)));
		Bar->SetColorAndOpacity(SchoolColor);
		Bar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* BSlot = ChipRow->AddChildToHorizontalBox(Bar);
		BSlot->SetVerticalAlignment(VAlign_Center);
		BSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		UTextBlock* Text = ERUI::MakeText(this, Label, EERTypeScale::Micro, T->Bone);
		Text->SetFont(MakeFont(T, 11, 60, true));
		ChipRow->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);

		UHorizontalBoxSlot* ChipSlot = SkillsBox->AddChildToHorizontalBox(Chip);
		ChipSlot->SetVerticalAlignment(VAlign_Center);
		ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
	}
}

UERStatCell* UERPersonnelPanel::MakeStatCell(UHorizontalBox* Box, const FText& Label, const FText& Value, FLinearColor Tint)
{
	if (!Box || !WidgetTree)
	{
		return nullptr;
	}
	const TSubclassOf<UERStatCell> CellClass = StatCellClass
		? StatCellClass
		: TSubclassOf<UERStatCell>(UERStatCell::StaticClass());
	UERStatCell* Cell = WidgetTree->ConstructWidget<UERStatCell>(CellClass);
	if (!Cell)
	{
		return nullptr;
	}
	Cell->SetStat(Label, Value, Tint);
	UHorizontalBoxSlot* CellSlot = Box->AddChildToHorizontalBox(Cell);
	CellSlot->SetSize(FillSize());
	CellSlot->SetVerticalAlignment(VAlign_Center);
	if (Box->GetChildrenCount() > 1)
	{
		CellSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	}
	return Cell;
}

UTexture2D* UERPersonnelPanel::ResolvePortrait(const FHallHeroProfile& Profile) const
{
	if (!Profile.Portrait.IsNull())
	{
		if (UTexture2D* Texture = Profile.Portrait.LoadSynchronous())
		{
			return Texture;
		}
	}
	if (DefaultPortrait)
	{
		return DefaultPortrait;
	}
	if (!PlaceholderPortrait.IsNull())
	{
		if (UTexture2D* Placeholder = PlaceholderPortrait.LoadSynchronous())
		{
			return Placeholder;
		}
	}
	return UERUITheme::Get()->DefaultPortraitAsset;
}

FText UERPersonnelPanel::FormatPercent(float Ratio, bool bSigned) const
{
	const int32 Percent = FMath::RoundToInt(Ratio * 100.0f);
	if (!bSigned)
	{
		return FText::FromString(FString::Printf(TEXT("%d%%"), Percent));
	}
	if (Percent == 0)
	{
		return FText::FromString(TEXT("0%"));
	}
	return FText::FromString(FString::Printf(TEXT("%+d%%"), Percent));
}

FHallHeroProfile UERPersonnelPanel::GetSelectedProfile() const
{
	return Profiles.IsValidIndex(SelectedIndex) ? Profiles[SelectedIndex] : FHallHeroProfile();
}

FText UERPersonnelPanel::GetSquadSummary() const
{
	return FText::FromString(FString::Printf(TEXT("编队 %02d / %02d"), Profiles.Num(), Profiles.Num()));
}

FText UERPersonnelPanel::GetSquadState() const
{
	return HasRunState() ? INVTEXT("EXPEDITION ACTIVE") : INVTEXT("STANDBY // 尚未启程");
}

bool UERPersonnelPanel::HasRunState() const
{
	const URunSubsystem* Run = GetRunSubsystem();
	return Run && Run->HasActiveRun();
}

float UERPersonnelPanel::GetShownIntegrity(int32 MemberIndex) const
{
	if (!Profiles.IsValidIndex(MemberIndex))
	{
		return 0.0f;
	}
	const FBattleUnitStats& Stats = Profiles[MemberIndex].Stats;
	if (HasRunState())
	{
		const URunSubsystem* Run = GetRunSubsystem();
		if (Run && Run->GetParty().IsValidIndex(MemberIndex))
		{
			return FMath::Clamp(Run->GetParty()[MemberIndex].Integrity, 0.0f, Stats.MaxIntegrity);
		}
	}
	return Stats.MaxIntegrity;
}

UWorld* UERPersonnelPanel::ResolvePlayWorld() const
{
	if (UWorld* World = GetWorld())
	{
		return World;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetWorld();
	}
	return nullptr;
}

URunSubsystem* UERPersonnelPanel::GetRunSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<URunSubsystem>();
	}
	return nullptr;
}

void UERPersonnelPanel::SelectMember(int32 MemberIndex)
{
	if (!Profiles.IsValidIndex(MemberIndex))
	{
		return;
	}
	const bool bChanged = (SelectedIndex != MemberIndex);
	SelectedIndex = MemberIndex;
	RefreshDetail();
	RefreshSelectionVisuals();
	if (bChanged && !IsDesignTime())
	{
		DetailTrack.Trigger(0.46f);
		CallsignCipher.Start(Profiles[SelectedIndex].Callsign.ToString(), 0.32f, 0.02f);
	}
	if (!IsDesignTime())
	{
		BP_OnMemberSelected(MemberIndex);
	}
}

void UERPersonnelPanel::HandleMemberSelected(int32 MemberIndex)
{
	SelectMember(MemberIndex);
}

void UERPersonnelPanel::HandleCloseClicked()
{
	ClosePanel();
}
