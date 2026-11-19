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

	FSlateFontInfo MakeFont(const UERUITheme* T, EERTypeScale Scale, int32 Size, int32 Tracking = 0, bool bLatin = false)
	{
		FSlateFontInfo Font = T->GetFont(Scale, bLatin);
		Font.Size = Size;
		Font.LetterSpacing = Tracking;
		return Font;
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

	// ---- 主面板 ----
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrush(ERUI::RoundedBrush(26.0f, T->InkPanel, 1.0f, T->BoneGhost));
	PanelBorder->SetBrushColor(FLinearColor::White);
	PanelBorder->SetPadding(FMargin(0.0f));
	PanelBorder->SetHorizontalAlignment(HAlign_Fill);
	PanelBorder->SetVerticalAlignment(VAlign_Fill);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(PanelBorder);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 1320.0f, 800.0f));
		S->SetAutoSize(false);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelColumn"));
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBorder->SetContent(Column);

	// ======== 标题行 ========
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	HeaderRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Column->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(36.0f, 26.0f, 30.0f, 0.0f));

	{
		USizeBox* TickSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentTickSize"));
		TickSize->SetWidthOverride(2.0f);
		TickSize->SetHeightOverride(26.0f);
		UBorder* Tick = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AccentTick"));
		Tick->SetBrush(ERUI::RoundedBrush(1.0f, T->Accent));
		Tick->SetBrushColor(FLinearColor::White);
		Tick->SetPadding(FMargin(0.0f));
		TickSize->SetContent(Tick);
		HeaderRow->AddChildToHorizontalBox(TickSize)->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleCol"));
	TitleCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(TitleCol);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	HeaderTitle = ERUI::MakeText(this, INVTEXT("人员管理"), EERTypeScale::Title, T->Bone);
	HeaderTitle->SetFont(MakeFont(T, EERTypeScale::Title, 21, 60));
	TitleCol->AddChildToVerticalBox(HeaderTitle);

	HeaderCaption = ERUI::MakeText(this, INVTEXT("P E R S O N N E L   R O S T E R"), EERTypeScale::Micro, T->BoneTertiary, true);
	HeaderCaption->SetFont(MakeFont(T, EERTypeScale::Micro, 8, 200, true));
	TitleCol->AddChildToVerticalBox(HeaderCaption)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	HeaderRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("HeaderSpacer")))
		->SetSize(FillSize());

	UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));
	StatusCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(StatusCol);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
	}

	SquadSummaryText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary);
	SquadSummaryText->SetJustification(ETextJustify::Right);
	SquadSummaryText->SetFont(MakeFont(T, EERTypeScale::Caption, 11, 80));
	StatusCol->AddChildToVerticalBox(SquadSummaryText);

	SquadStateText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneTertiary);
	SquadStateText->SetJustification(ETextJustify::Right);
	SquadStateText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 60));
	StatusCol->AddChildToVerticalBox(SquadStateText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	// 关闭按钮: 发丝圆环 + ×
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
		CloseSize->SetWidthOverride(34.0f);
		CloseSize->SetHeightOverride(34.0f);
		CloseButton->SetContent(CloseSize);

		UBorder* CloseRing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CloseRing"));
		CloseRing->SetBrush(ERUI::RoundedBrush(17.0f, FLinearColor(0.06f, 0.08f, 0.11f, 0.35f), 1.0f, T->BoneGhost));
		CloseRing->SetBrushColor(FLinearColor::White);
		CloseRing->SetPadding(FMargin(0.0f));
		CloseRing->SetHorizontalAlignment(HAlign_Center);
		CloseRing->SetVerticalAlignment(VAlign_Center);
		CloseSize->SetContent(CloseRing);

		CloseGlyphText = ERUI::MakeText(this, INVTEXT("×"), EERTypeScale::Body, T->BoneSecondary);
		CloseGlyphText->SetJustification(ETextJustify::Center);
		CloseGlyphText->SetFont(MakeFont(T, EERTypeScale::Body, 17, 0));
		CloseRing->SetContent(CloseGlyphText);
	}
	HeaderRow->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);

	// 标题下的发丝线
	HeaderRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HeaderRule"));
	HeaderRule->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 1.0f)));
	HeaderRule->SetColorAndOpacity(T->BoneGhost);
	Column->AddChildToVerticalBox(HeaderRule)->SetPadding(FMargin(36.0f, 18.0f, 30.0f, 0.0f));

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
	BodyRow->AddChildToHorizontalBox(DetailRow)->SetSize(FillSize());

	// 立绘列
	USizeBox* PortraitCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitCol"));
	PortraitCol->SetWidthOverride(384.0f);
	DetailRow->AddChildToHorizontalBox(PortraitCol)->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* PortraitColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PortraitColumn"));
	PortraitColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PortraitCol->SetContent(PortraitColumn);

	PortraitFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortraitFrame"));
	PortraitFrame->SetBrush(ERUI::RoundedBrush(4.0f, FLinearColor(0.02f, 0.03f, 0.045f, 0.72f), 1.0f, T->BoneGhost));
	PortraitFrame->SetBrushColor(FLinearColor::White);
	PortraitFrame->SetPadding(FMargin(0.0f));
	PortraitFrame->SetHorizontalAlignment(HAlign_Fill);
	PortraitFrame->SetVerticalAlignment(VAlign_Fill);
	{
		USizeBox* FrameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitFrameSize"));
		FrameSize->SetWidthOverride(384.0f);
		FrameSize->SetHeightOverride(510.0f);
		FrameSize->SetContent(PortraitFrame);
		PortraitColumn->AddChildToVerticalBox(FrameSize);
	}

	UOverlay* PortraitOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PortraitOverlay"));
	PortraitOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PortraitFrame->SetContent(PortraitOverlay);

	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
	PortraitImage->SetBrush(ERUI::WhiteBrush());
	PortraitImage->SetColorAndOpacity(FLinearColor(0.08f, 0.10f, 0.13f, 1.0f));
	PortraitOverlay->AddChildToOverlay(PortraitImage);

	PortraitScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitScrim"));
	PortraitScrim->SetBrush(ERUI::ScrimBrush());
	PortraitScrim->SetColorAndOpacity(ERUI::Fade(T->InkScrim, 0.72f));
	PortraitScrim->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	{
		// 渐压纹理是"上实下透", 翻转后变成"下实上透", 让立绘底部沉入面板
		FWidgetTransform Flip;
		Flip.Scale = FVector2D(1.0f, -1.0f);
		PortraitScrim->SetRenderTransform(Flip);
	}
	PortraitOverlay->AddChildToOverlay(PortraitScrim);

	PortraitIndexText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, ERUI::Fade(T->Bone, 0.78f), true);
	PortraitIndexText->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 120, true));
	{
		UOverlaySlot* S = PortraitOverlay->AddChildToOverlay(PortraitIndexText);
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 12.0f));
	}

	PortraitCaptionText = ERUI::MakeText(this, INVTEXT("档案影像 · ARCHIVE"), EERTypeScale::Micro, T->BoneTertiary);
	PortraitCaptionText->SetFont(MakeFont(T, EERTypeScale::Micro, 9, 80));
	PortraitColumn->AddChildToVerticalBox(PortraitCaptionText)->SetPadding(FMargin(2.0f, 10.0f, 0.0f, 0.0f));

	// 信息列
	UVerticalBox* InfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InfoCol"));
	InfoCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	{
		UHorizontalBoxSlot* S = DetailRow->AddChildToHorizontalBox(InfoCol);
		S->SetSize(FillSize());
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(34.0f, 0.0f, 0.0f, 0.0f));
	}

	NameText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Display, T->Bone);
	NameText->SetFont(MakeFont(T, EERTypeScale::Display, 40, 30));
	InfoCol->AddChildToVerticalBox(NameText);

	CallsignText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary, true);
	CallsignText->SetFont(MakeFont(T, EERTypeScale::Caption, 12, 260, true));
	InfoCol->AddChildToVerticalBox(CallsignText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

	UHorizontalBox* SchoolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SchoolRow"));
	SchoolRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InfoCol->AddChildToVerticalBox(SchoolRow)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	SchoolChipBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SchoolChipBorder"));
	SchoolChipBorder->SetBrush(ERUI::RoundedBrush(4.0f, FLinearColor(0.10f, 0.12f, 0.16f, 0.55f), 1.0f, T->BoneGhost));
	SchoolChipBorder->SetBrushColor(FLinearColor::White);
	SchoolChipBorder->SetPadding(FMargin(10.0f, 4.0f));
	SchoolChipBorder->SetHorizontalAlignment(HAlign_Center);
	SchoolChipBorder->SetVerticalAlignment(VAlign_Center);
	SchoolRow->AddChildToHorizontalBox(SchoolChipBorder)->SetVerticalAlignment(VAlign_Center);

	SchoolChipText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary);
	SchoolChipText->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 40));
	SchoolChipBorder->SetContent(SchoolChipText);

	RoleText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Micro, T->BoneSecondary);
	RoleText->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 100));
	{
		UHorizontalBoxSlot* S = SchoolRow->AddChildToHorizontalBox(RoleText);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	TaglineText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Caption, T->BoneSecondary);
	TaglineText->SetAutoWrapText(true);
	TaglineText->SetFont(MakeFont(T, EERTypeScale::Caption, 12, 30));
	InfoCol->AddChildToVerticalBox(TaglineText)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));

	InfoRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InfoRule"));
	InfoRule->SetBrush(ERUI::WhiteBrush(FVector2D(1.0f, 1.0f)));
	InfoRule->SetColorAndOpacity(T->BoneGhost);
	InfoCol->AddChildToVerticalBox(InfoRule)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));

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
	}
	else
	{
		AnimT = 1.0f;
		AnimDirection = 0.0f;
		SetRenderOpacity(1.0f);
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
	const float Ease = 1.0f - FMath::Pow(1.0f - AnimT, 3.0f);

	SetRenderOpacity(Ease);
	if (PanelBorder)
	{
		const float Scale = FMath::Lerp(0.985f, 1.0f, Ease);
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
		SchoolChipText->SetColorAndOpacity(FSlateColor(ERUI::Fade(SchoolColor, 0.95f)));
	}
	if (SchoolChipBorder)
	{
		SchoolChipBorder->SetBrush(ERUI::RoundedBrush(4.0f,
			FLinearColor(SchoolColor.R, SchoolColor.G, SchoolColor.B, 0.12f), 1.0f,
			SchoolColor.CopyWithNewOpacity(0.38f)));
		SchoolChipBorder->SetBrushColor(FLinearColor::White);
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
		Chip->SetBrush(ERUI::RoundedBrush(3.0f,
			FLinearColor(SchoolColor.R, SchoolColor.G, SchoolColor.B, 0.09f), 1.0f,
			SchoolColor.CopyWithNewOpacity(0.30f)));
		Chip->SetBrushColor(FLinearColor::White);
		Chip->SetPadding(FMargin(9.0f, 4.0f));
		Chip->SetHorizontalAlignment(HAlign_Center);
		Chip->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Text = ERUI::MakeText(this, Label, EERTypeScale::Micro, T->BoneSecondary);
		Text->SetFont(MakeFont(T, EERTypeScale::Micro, 10, 30));
		Chip->SetContent(Text);

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
	SelectedIndex = MemberIndex;
	RefreshDetail();
	RefreshSelectionVisuals();
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
