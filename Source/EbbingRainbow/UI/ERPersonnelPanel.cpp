// Copyright Epic Games, Inc. All rights reserved.

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

	FSlateFontInfo MakeFont(const UERUITheme* T, EERTypeScale Scale, int32 Size, int32 Tracking = 0, bool bBold = false)
	{
		return T->GetFontExplicit(Size, bBold, Tracking);
	}
}

UERPersonnelPanel::UERPersonnelPanel()
{
	// 开发期统一占位立绘: 根目录 IMG.PNG -> /Game/UI/Textures/T_UI_HeroPortrait_Placeholder
	PlaceholderPortrait = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/UI/Textures/T_UI_HeroPortrait_Placeholder.T_UI_HeroPortrait_Placeholder")));
}

TSharedRef<SWidget> UERPersonnelPanel::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder) { Build(); }
	}
	return Super::RebuildWidget();
}

void UERPersonnelPanel::NativePreConstruct()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder) { Build(); }
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
		if (!PanelBorder) { Build(); }
	}
	BindDelegates();
	Super::NativeConstruct();

	const UERUITheme* T = ERUI::Theme();
	if (ScrimImage)
	{
		ScrimImage->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.62f));
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

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	// ---- 全屏压暗 ----
	ScrimImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ScrimImage"));
	ScrimImage->SetBrush(ERUI::WhiteBrush());
	ScrimImage->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.62f));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(ScrimImage);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	// ---- 主面板 (黑曜石 45° 切角巨构 + 四角对准刻度) ----
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrush(ERUI::TransparentBrush());
	PanelBorder->SetBrushColor(FLinearColor::White);
	PanelBorder->SetPadding(FMargin(0.0f));
	PanelBorder->SetHorizontalAlignment(HAlign_Fill);
	PanelBorder->SetVerticalAlignment(VAlign_Fill);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(PanelBorder);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 1360.0f, 820.0f));
		S->SetAutoSize(false);
	}

	UOverlay* PanelRootOv = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PanelRootOverlay"));
	PanelRootOv->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBorder->SetContent(PanelRootOv);

	PanelFrame = WidgetTree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass(), TEXT("PanelFrame"));
	PanelFrame->Chamfer = FVector4(20.0f, 0.0f, 20.0f, 0.0f);
	PanelFrame->FillColor = FLinearColor(0.006f, 0.007f, 0.010f, 0.96f);
	PanelFrame->BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.18f);
	PanelFrame->BorderThickness = 1.0f;
	PanelFrame->bDrawCornerBrackets = true;
	PanelFrame->BracketLength = 16.0f;
	PanelFrame->BracketThickness = 1.6f;
	PanelFrame->BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.80f);
	PanelFrame->AccentEdge = EERAccentEdge::Top;
	PanelFrame->bUsePrismAccent = true;
	PanelFrame->AccentThickness = 3.0f;
	PanelFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = PanelRootOv->AddChildToOverlay(PanelFrame);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelColumn"));
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UOverlaySlot* S = PanelRootOv->AddChildToOverlay(Column);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	// ======== 标题行 (Control: 共振式巨幅粗体 + 反白索引标签) ========
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	HeaderRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Column->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(40.0f, 26.0f, 34.0f, 0.0f));

	{
		HeaderPrismBar = WidgetTree->ConstructWidget<UERPrismBar>(UERPrismBar::StaticClass(), TEXT("HeaderPrismBar"));
		HeaderPrismBar->bVertical = true;
		HeaderPrismBar->BarSize = FVector2D(4.0f, 40.0f);
		HeaderPrismBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		HeaderRow->AddChildToHorizontalBox(HeaderPrismBar)->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));
	TitleCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(TitleCol);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	}

	HeaderCaption = ERUI::MakeText(this, INVTEXT("OPERATOR DOSSIER // SQUAD MANIFEST"), EERTypeScale::Micro, T->BoneTertiary, true);
	HeaderCaption->SetFont(MakeFont(T, EERTypeScale::Micro, 8, 280, false));
	TitleCol->AddChildToVerticalBox(HeaderCaption);

	HeaderTitle = ERUI::MakeText(this, INVTEXT("人员档案"), EERTypeScale::Title, T->Bone);
	HeaderTitle->SetFont(MakeFont(T, EERTypeScale::Title, 30, 80, true));
	TitleCol->AddChildToVerticalBox(HeaderTitle)->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));

	HeaderRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("HeaderSpacer")))
		->SetSize(FillSize());

	UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));
	StatusCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(StatusCol);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));
	}

	SquadSummaryText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Bone, true);
	SquadSummaryText->SetJustification(ETextJustify::Right);
	SquadSummaryText->SetFont(MakeFont(T, EERTypeScale::Title, 18, 80, true));
	StatusCol->AddChildToVerticalBox(SquadSummaryText);

	SquadStateText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	SquadStateText->SetJustification(ETextJustify::Right);
	SquadStateText->SetFont(MakeFont(T, EERTypeScale::Micro, 8, 180, false));
	StatusCol->AddChildToVerticalBox(SquadStateText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	// 关闭按钮: 锐利直角方框 [ × ]
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		FButtonStyle Style;
		const FSlateBrush Transparent = ERUI::TransparentBrush();
		Style.SetNormal(Transparent);
		Style.SetHovered(Transparent);
		Style.SetPressed(Transparent);
		Style.SetDisabled(Transparent);
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		CloseButton->SetStyle(Style);
		CloseButton->SetCursor(EMouseCursor::Hand);
	}
	{
		USizeBox* CloseSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CloseSize"));
		CloseSize->SetWidthOverride(38.0f);
		CloseSize->SetHeightOverride(38.0f);
		CloseButton->SetContent(CloseSize);

		UBorder* CloseRing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CloseRing"));
		CloseRing->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.02f, 0.022f, 0.028f, 0.85f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.28f)));
		CloseRing->SetBrushColor(FLinearColor::White);
		CloseRing->SetPadding(FMargin(0.0f));
		CloseRing->SetHorizontalAlignment(HAlign_Center);
		CloseRing->SetVerticalAlignment(VAlign_Center);
		CloseSize->SetContent(CloseRing);

		CloseGlyphText = ERUI::MakeText(this, INVTEXT("×"), EERTypeScale::Title, T->Bone);
		CloseGlyphText->SetJustification(ETextJustify::Center);
		CloseGlyphText->SetFont(MakeFont(T, EERTypeScale::Title, 20, 0, true));
		CloseRing->SetContent(CloseGlyphText);
	}
	HeaderRow->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);

	// 标题下的建筑分割线
	HeaderDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("HeaderDivider"));
	HeaderDivider->AnchorLength = 42.0f;
	HeaderDivider->AnchorColor = T->Bone;
	HeaderDivider->LineColor = T->BoneGhost;
	HeaderDivider->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Column->AddChildToVerticalBox(HeaderDivider)->SetPadding(FMargin(40.0f, 18.0f, 34.0f, 0.0f));

	// ======== 主体 ========
	UHorizontalBox* BodyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BodyRow"));
	BodyRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BodyRow);
		S->SetSize(FillSize());
		S->SetPadding(FMargin(36.0f, 22.0f, 30.0f, 0.0f));
	}

	// ---- 左: 成员列表 ----
	USizeBox* RailSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RailSize"));
	RailSize->SetWidthOverride(260.0f);
	{
		UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(RailSize);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* MemberRail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MemberRail"));
	MemberRail->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RailSize->SetContent(MemberRail);

	RailTitleText = ERUI::MakeText(this, INVTEXT("特勤编队 · SQUAD"), EERTypeScale::Micro, T->BoneTertiary);
	RailTitleText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 140));
	MemberRail->AddChildToVerticalBox(RailTitleText);

	MemberListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MemberListBox"));
	MemberListBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MemberRail->AddChildToVerticalBox(MemberListBox)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	MemberRail->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("RailSpacer")))
		->SetSize(FillSize());

	RailHintText = ERUI::MakeText(this, INVTEXT("点击成员查看档案"), EERTypeScale::Micro, ERUI::Fade(T->BoneTertiary, 0.8f));
	RailHintText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 40));
	MemberRail->AddChildToVerticalBox(RailHintText)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	// 竖发丝线
	RailRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RailRule"));
	RailRule->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 1.0f)));
	RailRule->SetColorAndOpacity(ERUI::Fade(T->BoneGhost, 0.9f));
	{
		UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(RailRule);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(28.0f, 2.0f, 28.0f, 2.0f));
	}

	// ---- 右: 档案 ----
	UHorizontalBox* DetailRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DetailRow"));
	DetailRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DetailContainer = DetailRow;
	BodyRow->AddChildToHorizontalBox(DetailRow)->SetSize(FillSize());

	// 立绘列 (锐利直角画框 + 反白编号铭牌)
	USizeBox* PortraitCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitCol"));
	PortraitCol->SetWidthOverride(384.0f);
	DetailRow->AddChildToHorizontalBox(PortraitCol)->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* PortraitColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PortraitColumn"));
	PortraitColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PortraitCol->SetContent(PortraitColumn);

	PortraitFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortraitFrame"));
	PortraitFrame->SetBrush(ERUI::RoundedBrush(0.0f, FLinearColor(0.008f, 0.009f, 0.012f, 0.96f), 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.24f)));
	PortraitFrame->SetBrushColor(FLinearColor::White);
	PortraitFrame->SetPadding(FMargin(0.0f));
	PortraitFrame->SetHorizontalAlignment(HAlign_Fill);
	PortraitFrame->SetVerticalAlignment(VAlign_Fill);
	{
		USizeBox* FrameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitFrameSize"));
		FrameSize->SetWidthOverride(384.0f);
		FrameSize->SetHeightOverride(516.0f);
		FrameSize->SetContent(PortraitFrame);
		PortraitColumn->AddChildToVerticalBox(FrameSize);
	}

	UOverlay* PortraitOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PortraitOverlay"));
	PortraitOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PortraitFrame->SetContent(PortraitOverlay);

	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
	PortraitImage->SetBrush(ERUI::WhiteBrush());
	PortraitImage->SetColorAndOpacity(FLinearColor(0.05f, 0.06f, 0.08f, 1.0f));
	PortraitOverlay->AddChildToOverlay(PortraitImage);

	PortraitScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitScrim"));
	PortraitScrim->SetBrush(ERUI::ScrimBrush());
	PortraitScrim->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.78f));
	PortraitScrim->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	{
		FWidgetTransform Flip;
		Flip.Scale = FVector2D(1.0f, -1.0f);
		PortraitScrim->SetRenderTransform(Flip);
	}
	PortraitOverlay->AddChildToOverlay(PortraitScrim);

	// 底部反白石膏编号块 [ 01 / 04 ]
	UBorder* IndexTag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortraitIndexTag"));
	IndexTag->SetBrush(ERUI::RoundedBrush(0.0f, T->Bone));
	IndexTag->SetBrushColor(FLinearColor::White);
	IndexTag->SetPadding(FMargin(10.0f, 3.0f));
	PortraitIndexText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->PureInk, true);
	PortraitIndexText->SetFont(MakeFont(T, EERTypeScale::Micro, 11, 160, true));
	IndexTag->SetContent(PortraitIndexText);
	{
		UOverlaySlot* S = PortraitOverlay->AddChildToOverlay(IndexTag);
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 12.0f));
	}

	PortraitCaptionText = ERUI::MakeText(this, INVTEXT("ARCHIVE // VISUAL RECORD"), EERTypeScale::Micro, T->BoneTertiary, true);
	PortraitCaptionText->SetFont(MakeFont(T, EERTypeScale::Micro, 8, 220, false));
	PortraitColumn->AddChildToVerticalBox(PortraitCaptionText)->SetPadding(FMargin(2.0f, 10.0f, 0.0f, 0.0f));

	// 信息列 (Control: 共振式 72pt 巨型名字 + 极小追踪英文遥测)
	UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InfoCol"));
	InfoCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = DetailRow->AddChildToHorizontalBox(InfoCol);
		S->SetSize(FillSize());
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(36.0f, 0.0f, 0.0f, 0.0f));
	}

	NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Display, T->Bone);
	NameText->SetFont(MakeFont(T, EERTypeScale::Display, 72, 20, true));
	InfoCol->AddChildToVerticalBox(NameText);

	CallsignText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Title, T->Accent, true);
	CallsignText->SetFont(MakeFont(T, EERTypeScale::Title, 15, 340, true));
	InfoCol->AddChildToVerticalBox(CallsignText)->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));

	UHorizontalBox* SchoolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SchoolRow"));
	SchoolRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SchoolRow)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	SchoolChipBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SchoolChipBorder"));
	SchoolChipBorder->SetBrush(ERUI::RoundedBrush(0.0f, T->Bone, 0.0f, FLinearColor::Transparent));
	SchoolChipBorder->SetBrushColor(FLinearColor::White);
	SchoolChipBorder->SetPadding(FMargin(10.0f, 3.0f));
	SchoolChipBorder->SetHorizontalAlignment(HAlign_Center);
	SchoolChipBorder->SetVerticalAlignment(VAlign_Center);
	SchoolRow->AddChildToHorizontalBox(SchoolChipBorder)->SetVerticalAlignment(VAlign_Center);

	SchoolChipText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->PureInk);
	SchoolChipText->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 120, true));
	SchoolChipBorder->SetContent(SchoolChipText);

	RoleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary, true);
	RoleText->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 180, false));
	{
		UHorizontalBoxSlot* S = SchoolRow->AddChildToHorizontalBox(RoleText);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	TaglineText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary);
	TaglineText->SetAutoWrapText(true);
	TaglineText->SetFont(MakeFont(T, EERTypeScale::Caption, 12, 40, false));
	InfoCol->AddChildToVerticalBox(TaglineText)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	InfoDivider = WidgetTree->ConstructWidget<UERTechDivider>(UERTechDivider::StaticClass(), TEXT("InfoDivider"));
	InfoDivider->AnchorLength = 30.0f;
	InfoDivider->AnchorColor = T->Accent;
	InfoDivider->LineColor = T->BoneGhost;
	InfoDivider->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(InfoDivider)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));

	StatsTitleText = ERUI::MakeText(this, INVTEXT("基本属性 · ATTRIBUTES"), EERTypeScale::Micro, T->BoneTertiary);
	StatsTitleText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 140));
	InfoCol->AddChildToVerticalBox(StatsTitleText)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));

	PrimaryStatsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PrimaryStatsBox"));
	PrimaryStatsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(PrimaryStatsBox)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

	SecondaryStatsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SecondaryStatsBox"));
	SecondaryStatsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SecondaryStatsBox)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	ResistTitleText = ERUI::MakeText(this, INVTEXT("四学派抗性 · RESISTANCE"), EERTypeScale::Micro, T->BoneTertiary);
	ResistTitleText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 140));
	InfoCol->AddChildToVerticalBox(ResistTitleText)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));

	ResistBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResistBox"));
	ResistBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(ResistBox)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

	SkillsTitleText = ERUI::MakeText(this, INVTEXT("战术技能 · LOADOUT"), EERTypeScale::Micro, T->BoneTertiary);
	SkillsTitleText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 140));
	InfoCol->AddChildToVerticalBox(SkillsTitleText)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));

	SkillsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillsBox"));
	SkillsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SkillsBox)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

	// ======== 页脚 ========
	UHorizontalBox* FooterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FooterRow"));
	FooterRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Column->AddChildToVerticalBox(FooterRow)->SetPadding(FMargin(36.0f, 12.0f, 30.0f, 20.0f));

	FooterHintText = ERUI::MakeText(this, INVTEXT("点击左侧成员切换档案 · Esc 关闭"), EERTypeScale::Micro, T->BoneTertiary);
	FooterHintText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 40));
	FooterRow->AddChildToHorizontalBox(FooterHintText)->SetVerticalAlignment(VAlign_Center);

	FooterRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("FooterSpacer")))
		->SetSize(FillSize());

	UTextBlock* FooterMark = ERUI::MakeText(this, INVTEXT("E R · P E R S O N N E L"), EERTypeScale::Micro, ERUI::Fade(T->BoneTertiary, 0.7f), true);
	FooterMark->SetFont(MakeFont(T, EERTypeScale::Micro, 8, 200, true));
	FooterRow->AddChildToHorizontalBox(FooterMark)->SetVerticalAlignment(VAlign_Center);
}

void UERPersonnelPanel::OpenPanel()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder) { Build(); }
	}
	if (!PanelBorder)
	{
		return;
	}

	RefreshRoster();

	bPanelOpen = true;
	bClosing = false;
	SetVisibility(ESlateVisibility::Visible);

	if (bEnableEntranceAnimation)
	{
		AnimT = 0.0f;
		AnimDirection = 1.0f;
		SetRenderOpacity(0.0f);
		HeaderCaptionCipher.Start(TEXT("OPERATOR DOSSIER // SQUAD MANIFEST"), 0.38f, 0.04f);
		DetailTrack.Trigger(0.45f);
		if (Profiles.IsValidIndex(SelectedIndex))
		{
			CallsignCipher.Start(Profiles[SelectedIndex].Callsign.ToString(), 0.34f, 0.08f);
		}
	}
	else
	{
		AnimT = 1.0f;
		AnimDirection = 0.0f;
		SetRenderOpacity(1.0f);
		DetailTrack.FinishImmediately();
	}
	PanelBorder->SetRenderTranslation(FVector2D::ZeroVector);
	PanelBorder->SetRenderScale(FVector2D(1.0f, 1.0f));

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
	if (PanelFrame)
	{
		PanelFrame->SetMotionState(FMath::Lerp(0.55f, 1.0f, Ease), 0.0f, (AnimDirection > 0.0f && AnimT < 1.0f) ? AnimT : 1.0f);
	}
	if (HeaderDivider)
	{
		HeaderDivider->SetRevealAlpha(Ease);
	}
	if (HeaderPrismBar)
	{
		HeaderPrismBar->SetRevealAlpha(Ease);
	}
	if (PanelBorder)
	{
		const float Scale = FMath::Lerp(0.975f, 1.0f, Ease);
		PanelBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		PanelBorder->SetRenderScale(FVector2D(Scale, Scale));
		PanelBorder->SetRenderTranslation(FVector2D(0.0f, (1.0f - Ease) * EntranceOffsetY));
	}

	if (AnimDirection < 0.0f && AnimT <= 0.0f)
	{
		AnimDirection = 0.0f;
		bClosing = false;
		bPanelOpen = false;
		SetRenderOpacity(1.0f);
		SetVisibility(ESlateVisibility::Collapsed);
		if (PanelBorder)
		{
			PanelBorder->SetRenderTranslation(FVector2D::ZeroVector);
			PanelBorder->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
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
		const float E = DetailTrack.Expo(0.0f, 0.34f);
		const float DivE = DetailTrack.Expo(0.06f, 0.36f);
		if (DetailContainer)
		{
			DetailContainer->SetRenderOpacity(FMath::Lerp(0.25f, 1.0f, E));
			DetailContainer->SetRenderTranslation(FVector2D(16.0f * (1.0f - E), 0.0f));
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
	if (bPanelOpen && InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClosePanel();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UERPersonnelPanel::RefreshRoster()
{
	if (!bBuilt)
	{
		bBuilt = true;
		if (!PanelBorder) { Build(); }
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

		UVerticalBoxSlot* RowSlot = MemberListBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
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
		SquadSummaryText->SetText(FText::FromString(FString::Printf(TEXT("编队 %d / %d"), Profiles.Num(), Profiles.Num())));
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
			SquadStateText->SetText(FText::FromString(FString::Printf(TEXT("远征进行中 · 平均完整度 %d%%"), Percent)));
		}
		else
		{
			SquadStateText->SetText(INVTEXT("待命中 · 尚未启程"));
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

	if (NameText)
	{
		NameText->SetText(Profile.HeroName);
	}
	if (CallsignText)
	{
		CallsignText->SetText(Profile.Callsign);
		CallsignText->SetColorAndOpacity(FSlateColor(ERUI::Fade(SchoolColor, 0.92f)));
	}
	if (SchoolChipText)
	{
		SchoolChipText->SetText(FText::FromString(FString::Printf(TEXT("学派 · %s"), *Profile.GetSchoolName().ToString())));
		SchoolChipText->SetColorAndOpacity(FSlateColor(T->PureInk));
	}
	if (SchoolChipBorder)
	{
		// 高对比反白标牌 + 学派左色感 (单点睛色)
		SchoolChipBorder->SetBrush(ERUI::RoundedBrush(0.0f, SchoolColor, 0.0f, FLinearColor::Transparent));
		SchoolChipBorder->SetBrushColor(FLinearColor::White);
	}
	if (InfoDivider)
	{
		InfoDivider->SetDividerColors(T->BoneGhost, SchoolColor);
	}
	if (RoleText)
	{
		RoleText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"),
			*Profile.RoleTitle.ToString(), *Profile.RoleCaption.ToString())));
	}
	if (TaglineText)
	{
		TaglineText->SetText(Profile.Tagline);
	}

	// ---- 立绘 ----
	UTexture2D* PortraitTexture = ResolvePortrait(Profile);
	if (PortraitImage)
	{
		if (PortraitTexture)
		{
			PortraitImage->SetBrushFromTexture(PortraitTexture, false);
			PortraitImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			PortraitImage->SetBrush(ERUI::WhiteBrush());
			PortraitImage->SetColorAndOpacity(FLinearColor(0.08f, 0.10f, 0.13f, 1.0f));
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
			? FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(ShownIntegrity), FMath::RoundToInt(Stats.MaxIntegrity)))
			: FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(Stats.MaxIntegrity)));

		MakeStatCell(PrimaryStatsBox, INVTEXT("完整度"), IntegrityValue, IntegrityTint);
		MakeStatCell(PrimaryStatsBox, INVTEXT("行动点"), FText::FromString(FString::Printf(TEXT("%d"), Stats.MaxAP)), T->Bone);
		MakeStatCell(PrimaryStatsBox, INVTEXT("先攻"), FText::FromString(FString::Printf(TEXT("%d"), Stats.Initiative)), T->Bone);
		MakeStatCell(PrimaryStatsBox, INVTEXT("闪避"), FormatPercent(Stats.EvasionRate, false), T->Bone);
	}

	if (SecondaryStatsBox)
	{
		SecondaryStatsBox->ClearChildren();
		MakeStatCell(SecondaryStatsBox, INVTEXT("暴击"), FormatPercent(Stats.CritChance, false), T->BoneSecondary);
		MakeStatCell(SecondaryStatsBox, INVTEXT("暴伤"), FormatPercent(Stats.CritMultiplier, false), T->BoneSecondary);
		MakeStatCell(SecondaryStatsBox, INVTEXT("命中"), FormatPercent(Stats.Accuracy, true), T->BoneSecondary);
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
			FLinearColor(0.016f, 0.018f, 0.024f, 0.92f), 1.0f,
			FLinearColor(0.92f, 0.92f, 0.90f, 0.22f)));
		Chip->SetBrushColor(FLinearColor::White);
		Chip->SetPadding(FMargin(11.0f, 5.0f));
		Chip->SetHorizontalAlignment(HAlign_Center);
		Chip->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ChipRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Chip->SetContent(ChipRow);

		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Bar->SetBrush(ERUI::WhiteBrush(FVector2D(2.0f, 11.0f)));
		Bar->SetColorAndOpacity(SchoolColor);
		Bar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* BSlot = ChipRow->AddChildToHorizontalBox(Bar);
		BSlot->SetVerticalAlignment(VAlign_Center);
		BSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));

		UTextBlock* Text = ERUI::MakeText(this, Label, EERTypeScale::Micro, T->Bone);
		Text->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 60, false));
		ChipRow->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);

		UHorizontalBoxSlot* ChipSlot = SkillsBox->AddChildToHorizontalBox(Chip);
		ChipSlot->SetVerticalAlignment(VAlign_Center);
		ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
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
		CellSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
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
	return FText::FromString(FString::Printf(TEXT("编队 %d / %d"), Profiles.Num(), Profiles.Num()));
}

FText UERPersonnelPanel::GetSquadState() const
{
	return HasRunState() ? INVTEXT("远征进行中") : INVTEXT("待命中 · 尚未启程");
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
		DetailTrack.Trigger(0.42f);
		CallsignCipher.Start(Profiles[SelectedIndex].Callsign.ToString(), 0.30f, 0.02f);
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
