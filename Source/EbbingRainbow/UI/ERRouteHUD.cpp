// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERRouteHUD.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Core/ERLog.h"
#include "UI/ERRouteNode.h"
#include "UI/ERRouteWidgets.h"
#include "UI/ERUIStatics.h"
#include "Run/RouteGraphGenerator.h"
#include "Run/RouteLayout.h"
#include "Run/RunSubsystem.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> UERRouteHUD::RebuildWidget()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UERRouteHUD::NativePreConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativePreConstruct();

	RebuildLegend();

	if (IsDesignTime())
	{
		if (ProgressText) { ProgressText->SetText(INVTEXT("第 1 / 10 层")); }
		if (StatText)     { StatText->SetText(FText::FromString(FString::Printf(TEXT("战斗 0 场   ·   种子 %d"), PreviewSeed))); }
		if (NotificationBorder)
		{
			if (NotificationText) { NotificationText->SetText(PreviewNotificationText); }
			NotificationBorder->SetVisibility(bPreviewNotification ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		BuildDesignPreviewGraph();
	}
}

void UERRouteHUD::NativeConstruct()
{
	if (!bLayoutBuilt)
	{
		bLayoutBuilt = true;
		BuildLayout();
	}
	Super::NativeConstruct();

	if (NotificationBorder)
	{
		NotificationBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	RebuildLegend();
	if (RunSubsystem.IsValid())
	{
		RebuildNodes();
		Refresh();
	}
	else
	{
		NodeWidgets.Reset();
		NodePositions.Reset();
		if (NodeCanvas)
		{
			NodeCanvas->ClearChildren();
		}
	}
	if (Anim_Intro)
	{
		PlayAnimationForward(Anim_Intro);
	}
}

void UERRouteHUD::ResolveTypeVisual(ERouteNodeType Type, FText& OutName, FText& OutGlyph, FLinearColor& OutColor, UTexture2D*& OutIcon) const
{
	const FRouteNodeTypeInfo& Info = GetRouteNodeTypeInfo(Type);
	OutName = FText::FromString(Info.DisplayName);
	OutGlyph = FText::FromString(Info.Glyph);
	OutColor = Info.Color;
	OutIcon = nullptr;

	if (const FERRouteNodeStyleOverride* Ov = NodeTypeOverrides.Find(Type))
	{
		if (!Ov->DisplayNameOverride.IsEmpty()) { OutName = Ov->DisplayNameOverride; }
		if (!Ov->GlyphOverride.IsEmpty())       { OutGlyph = Ov->GlyphOverride; }
		if (Ov->bOverrideColor)                 { OutColor = Ov->ColorOverride; }
		OutIcon = Ov->IconTexture.Get();
	}
}

void UERRouteHUD::BuildLayout()
{
	const UERUITheme* T = ERUI::Theme();
	const FLinearColor TextHi(0.93f, 0.96f, 1.00f, 0.96f);
	const FLinearColor TextMid(0.74f, 0.82f, 0.94f, 0.70f);
	const FLinearColor TextLo(0.62f, 0.72f, 0.88f, 0.46f);

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
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (!TopScrim)
	{
		TopScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TopScrim"));
		TopScrim->SetBrush(ERUI::ScrimBrush());
		TopScrim->SetColorAndOpacity(FLinearColor(0.0f, 0.004f, 0.012f, 0.55f));
		TopScrim->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(TopScrim);
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		S->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 160.0f));
	}
	if (!BottomScrim)
	{
		BottomScrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BottomScrim"));
		BottomScrim->SetBrush(ERUI::ScrimBrush());
		BottomScrim->SetColorAndOpacity(FLinearColor(0.0f, 0.004f, 0.012f, 0.55f));
		BottomScrim->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		BottomScrim->SetRenderScale(FVector2D(1.0f, -1.0f));
		BottomScrim->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(BottomScrim);
		S->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f, -140.0f, 0.0f, 0.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
	}

	// 中央等比缩放地图画布 (2040 x 980, 上下非对称留白与 ERRouteLayout 严格一致)
	if (!NodeCanvas)
	{
		MapScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("MapScaleBox"));
		MapScaleBox->SetStretch(EStretch::ScaleToFit);
		MapScaleBox->SetStretchDirection(EStretchDirection::Both);
		MapScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UCanvasPanelSlot* MapSlot = RootCanvas->AddChildToCanvas(MapScaleBox);
			MapSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			MapSlot->SetOffsets(FMargin(0.0f, ERRouteLayout::PadTop, 0.0f, ERRouteLayout::PadBottom));
		}

		MapSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MapSizeBox"));
		MapSizeBox->SetWidthOverride(ERRouteLayout::MapW);
		MapSizeBox->SetHeightOverride(ERRouteLayout::MapH);
		MapSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		MapScaleBox->SetContent(MapSizeBox);

		MapOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MapOverlay"));
		MapOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		MapSizeBox->SetContent(MapOverlay);

		Ruler = WidgetTree->ConstructWidget<UERRouteLayerRuler>(UERRouteLayerRuler::StaticClass(), TEXT("Ruler"));
		Ruler->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = MapOverlay->AddChildToOverlay(Ruler);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		EdgeLayer = WidgetTree->ConstructWidget<UERRouteEdgeLayer>(UERRouteEdgeLayer::StaticClass(), TEXT("EdgeLayer"));
		EdgeLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = MapOverlay->AddChildToOverlay(EdgeLayer);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		NodeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NodeCanvas"));
		NodeCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = MapOverlay->AddChildToOverlay(NodeCanvas);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 左上标题组
	if (!TitleText)
	{
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
		HeaderRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		TitleAccentRule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TitleAccentRule"));
		TitleAccentRule->SetBrush(ERUI::WhiteBrush(FVector2D(2.0f, 36.0f)));
		TitleAccentRule->SetColorAndOpacity(FLinearColor(1.0f, 0.80f, 0.34f, 0.85f));
		TitleAccentRule->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBoxSlot* RuleSlot = HeaderRow->AddChildToHorizontalBox(TitleAccentRule);
		RuleSlot->SetVerticalAlignment(VAlign_Fill);
		RuleSlot->SetPadding(FMargin(0.0f, 2.0f, 12.0f, 2.0f));

		UVerticalBox* HeaderCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HeaderCol"));
		HeaderRow->AddChildToHorizontalBox(HeaderCol);

		TitleText = ERUI::MakeText(this, INVTEXT("航线图"), EERTypeScale::Title, TextHi);
		{
			FSlateFontInfo F = T->GetFont(EERTypeScale::Title);
			F.Size = 24;
			TitleText->SetFont(F);
			TitleText->SetShadowOffset(FVector2D(0.0f, 1.0f));
			TitleText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		}
		HeaderCol->AddChildToVerticalBox(TitleText);

		SubtitleText = ERUI::MakeText(this, INVTEXT("虹 不 我 与   ·   E B B I N G   R A I N B O W"), EERTypeScale::Micro,
			FLinearColor(0.50f, 0.72f, 0.98f, 0.62f));
		{
			FSlateFontInfo F = T->GetFont(EERTypeScale::Micro);
			F.Size = 9;
			F.LetterSpacing = 80;
			SubtitleText->SetFont(F);
		}
		HeaderCol->AddChildToVerticalBox(SubtitleText)->SetPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(HeaderRow);
		S->SetAnchors(FAnchors(0.0f, 0.0f));
		S->SetPosition(FVector2D(34.0f, 26.0f));
		S->SetAutoSize(true);
	}

	// 右上进度与种子统计
	if (!ProgressText)
	{
		UVerticalBox* StatusCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusCol"));
		StatusCol->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		ProgressText = ERUI::MakeText(this, INVTEXT("第 1 / 10 层"), EERTypeScale::Title, FLinearColor(1.0f, 0.86f, 0.46f, 0.98f));
		{
			FSlateFontInfo F = T->GetFont(EERTypeScale::Title);
			F.Size = 17;
			ProgressText->SetFont(F);
			ProgressText->SetShadowOffset(FVector2D(0.0f, 1.0f));
			ProgressText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		}
		StatusCol->AddChildToVerticalBox(ProgressText)->SetHorizontalAlignment(HAlign_Right);

		StatText = ERUI::MakeText(this, INVTEXT("战斗 0 场   ·   种子 0"), EERTypeScale::Micro, TextMid);
		{
			FSlateFontInfo F = T->GetFont(EERTypeScale::Micro);
			F.Size = 10;
			F.LetterSpacing = 40;
			StatText->SetFont(F);
		}
		UVerticalBoxSlot* StSlot = StatusCol->AddChildToVerticalBox(StatText);
		StSlot->SetHorizontalAlignment(HAlign_Right);
		StSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(StatusCol);
		S->SetAnchors(FAnchors(1.0f, 0.0f));
		S->SetAlignment(FVector2D(1.0f, 0.0f));
		S->SetPosition(FVector2D(-34.0f, 26.0f));
		S->SetAutoSize(true);
	}

	// 左下图例
	if (!LegendBox)
	{
		LegendBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LegendBox"));
		LegendBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(LegendBox);
		S->SetAnchors(FAnchors(0.0f, 1.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
		S->SetPosition(FVector2D(34.0f, -22.0f));
		S->SetAutoSize(true);
	}

	// 底部操作提示
	if (!HintText)
	{
		HintText = ERUI::MakeText(this, INVTEXT("点击发光节点前进   ·   战斗节点将进入竞技场"), EERTypeScale::Micro, TextLo);
		FSlateFontInfo F = T->GetFont(EERTypeScale::Micro);
		F.Size = 10;
		F.LetterSpacing = 60;
		HintText->SetFont(F);
		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(HintText);
		S->SetAnchors(FAnchors(0.5f, 1.0f));
		S->SetAlignment(FVector2D(0.5f, 1.0f));
		S->SetPosition(FVector2D(0.0f, -22.0f));
		S->SetAutoSize(true);
	}

	// 顶部通知胶囊
	if (!NotificationBorder)
	{
		NotificationBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NotificationBorder"));
		FSlateBrush PillBrush;
		PillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PillBrush.TintColor = FSlateColor(FLinearColor(0.020f, 0.034f, 0.062f, 0.86f));
		PillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PillBrush.OutlineSettings.CornerRadii = FVector4(16.0f, 16.0f, 16.0f, 16.0f);
		PillBrush.OutlineSettings.Width = 1.0f;
		PillBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.80f, 0.90f, 1.00f, 0.24f));
		NotificationBorder->SetBrush(PillBrush);
		NotificationBorder->SetBrushColor(FLinearColor::White);
		NotificationBorder->SetPadding(FMargin(22.0f, 9.0f));
		NotificationBorder->SetVisibility(ESlateVisibility::Collapsed);

		NotificationText = ERUI::MakeText(this, FText::GetEmpty(), EERTypeScale::Body, TextHi);
		NotificationBorder->SetContent(NotificationText);

		UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(NotificationBorder);
		S->SetAnchors(FAnchors(0.5f, 0.0f));
		S->SetAlignment(FVector2D(0.5f, 0.0f));
		S->SetPosition(FVector2D(0.0f, 30.0f));
		S->SetAutoSize(true);
	}
}

void UERRouteHUD::RebuildLegend()
{
	if (!LegendBox || !WidgetTree)
	{
		return;
	}
	LegendBox->ClearChildren();

	const TSubclassOf<UERRouteLegendItem> ItemCls = LegendItemClass
		? LegendItemClass
		: TSubclassOf<UERRouteLegendItem>(UERRouteLegendItem::StaticClass());

	const int32 Count = static_cast<int32>(ERouteNodeType::Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const ERouteNodeType Type = static_cast<ERouteNodeType>(i);
		FText Name, Glyph;
		FLinearColor Color;
		UTexture2D* Icon = nullptr;
		ResolveTypeVisual(Type, Name, Glyph, Color, Icon);

		UERRouteLegendItem* Item = WidgetTree->ConstructWidget<UERRouteLegendItem>(ItemCls);
		if (!Item) { continue; }
		Item->SetupLegendItem(Type, Name, Color, Icon);
		UHorizontalBoxSlot* S = LegendBox->AddChildToHorizontalBox(Item);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, (i == Count - 1) ? 0.0f : LegendItemSpacing, 0.0f));
	}
}

void UERRouteHUD::BuildDesignPreviewGraph()
{
	if (!NodeCanvas || !WidgetTree)
	{
		return;
	}
	NodeCanvas->ClearChildren();
	if (!bPreviewGraphInDesigner)
	{
		return;
	}

	FRouteGraph Graph;
	if (!FRouteGraphGenerator::Generate(PreviewSeed, FRouteGenSettings(), Graph) || !Graph.IsValid())
	{
		return;
	}

	const TSubclassOf<UERRouteNode> NodeCls = NodeWidgetClass
		? NodeWidgetClass
		: TSubclassOf<UERRouteNode>(UERRouteNode::StaticClass());

	TArray<FVector2D> Centers;
	Centers.SetNum(Graph.Nodes.Num());
	TArray<bool> Reach;
	ERRouteLayout::ComputeReachable(Graph, Graph.StartNodeId, Reach);

	for (const FRouteNode& Node : Graph.Nodes)
	{
		const FVector2D Center = ERRouteLayout::NodeCenter(Graph, Node);
		Centers[Node.Id] = Center;

		FText Name, Glyph;
		FLinearColor Color;
		UTexture2D* Icon = nullptr;
		ResolveTypeVisual(Node.Type, Name, Glyph, Color, Icon);

		UERRouteNode* Widget = WidgetTree->ConstructWidget<UERRouteNode>(NodeCls);
		if (!Widget) { continue; }

		ERouteNodeVisual Vis = ERouteNodeVisual::Locked;
		if (Node.Id == Graph.StartNodeId) { Vis = ERouteNodeVisual::Current; }
		else if (Graph.Nodes[Graph.StartNodeId].Next.Contains(Node.Id)) { Vis = ERouteNodeVisual::Available; }

		Widget->PreviewNodeType = Node.Type;
		Widget->PreviewGlyph = Glyph;
		Widget->PreviewLabel = Name;
		Widget->PreviewAccent = Color;
		Widget->PreviewIcon = Icon;
		Widget->bPreviewFinal = (Node.Layer == Graph.NumLayers - 1);
		Widget->PreviewVisualState = Vis;
		Widget->bPreviewReachable = Reach[Node.Id];

		Widget->SetupNode(Node.Id, Node.Type, Glyph, Name, FText::GetEmpty(), Color, Widget->bPreviewFinal, Icon);
		Widget->SetNodeVisual(Vis, Reach[Node.Id]);

		UCanvasPanelSlot* S = NodeCanvas->AddChildToCanvas(Widget);
		S->SetAnchors(FAnchors(0.0f, 0.0f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(Center);
		S->SetSize(FVector2D(ERRouteLayout::NodeW, ERRouteLayout::NodeH));
	}

	if (EdgeLayer)
	{
		TArray<SRouteEdgeLayer::FEdgeDraw> Edges;
		for (const FRouteNode& Node : Graph.Nodes)
		{
			for (const int32 NextId : Node.Next)
			{
				if (!Centers.IsValidIndex(Node.Id) || !Centers.IsValidIndex(NextId)) { continue; }
				SRouteEdgeLayer::FEdgeDraw ED;
				ED.Start = Centers[Node.Id];
				ED.End = Centers[NextId];
				ED.bAvailable = (Node.Id == Graph.StartNodeId);
				ED.bReachable = Reach[NextId] && (Node.Id == Graph.StartNodeId || Reach[Node.Id]);
				Edges.Add(ED);
			}
		}
		EdgeLayer->SetEdges(Edges);
	}

	if (Ruler)
	{
		TArray<float> Xs;
		for (int32 L = 0; L < Graph.NumLayers; ++L) { Xs.Add(ERRouteLayout::LayerX(Graph, L)); }
		Ruler->SetRulerData(Xs, 0);
	}
}

void UERRouteHUD::InitializeRouteUI(URunSubsystem* InRunSubsystem)
{
	RunSubsystem = InRunSubsystem;
	RebuildNodes();
	Refresh();
}

void UERRouteHUD::RebuildNodes()
{
	if (!NodeCanvas || !WidgetTree)
	{
		return;
	}

	NodeCanvas->ClearChildren();
	NodeWidgets.Reset();
	NodePositions.Reset();

	URunSubsystem* Run = RunSubsystem.Get();
	if (!Run)
	{
		return;
	}

	const FRouteGraph& Graph = Run->GetGraph();
	if (!Graph.IsValid())
	{
		return;
	}

	const TSubclassOf<UERRouteNode> NodeCls = NodeWidgetClass
		? NodeWidgetClass
		: TSubclassOf<UERRouteNode>(UERRouteNode::StaticClass());

	NodePositions.SetNum(Graph.Nodes.Num());
	NodeWidgets.SetNum(Graph.Nodes.Num());

	for (const FRouteNode& Node : Graph.Nodes)
	{
		const FVector2D Center = ERRouteLayout::NodeCenter(Graph, Node);
		NodePositions[Node.Id] = Center;

		FText Name, Glyph;
		FLinearColor Color;
		UTexture2D* Icon = nullptr;
		ResolveTypeVisual(Node.Type, Name, Glyph, Color, Icon);

		const FText Tooltip = FText::FromString(FString::Printf(TEXT("%s\n第 %d 层 · 遭遇种子 %d"),
			*Name.ToString(), Node.Layer + 1, Node.EncounterSeed));

		UERRouteNode* Widget = WidgetTree->ConstructWidget<UERRouteNode>(NodeCls);
		if (!Widget) { continue; }

		Widget->SetupNode(Node.Id, Node.Type, Glyph, Name, Tooltip, Color, Node.Layer == Graph.NumLayers - 1, Icon);
		Widget->OnRouteNodeClicked.AddDynamic(this, &UERRouteHUD::HandleNodeClicked);

		UCanvasPanelSlot* S = NodeCanvas->AddChildToCanvas(Widget);
		S->SetAnchors(FAnchors(0.0f, 0.0f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(Center);
		S->SetSize(FVector2D(ERRouteLayout::NodeW, ERRouteLayout::NodeH));

		NodeWidgets[Node.Id] = Widget;
	}

	UE_LOG(LogERRun, Log, TEXT("[RouteHUD/UMG] 生成节点控件 %d 个，画布 %.0fx%.0f"),
		NodeWidgets.Num(), ERRouteLayout::MapW, ERRouteLayout::MapH);
}

void UERRouteHUD::Refresh()
{
	URunSubsystem* Run = RunSubsystem.Get();
	if (!Run)
	{
		return;
	}

	const FRouteGraph& Graph = Run->GetGraph();
	if (!Graph.IsValid())
	{
		return;
	}

	const int32 CurrentNodeId = Run->GetCurrentNodeId();
	TArray<bool> Reach;
	ERRouteLayout::ComputeReachable(Graph, CurrentNodeId, Reach);

	TArray<SRouteEdgeLayer::FEdgeDraw> Edges;
	for (const FRouteNode& Node : Graph.Nodes)
	{
		for (const int32 NextId : Node.Next)
		{
			if (!NodePositions.IsValidIndex(Node.Id) || !NodePositions.IsValidIndex(NextId))
			{
				continue;
			}
			SRouteEdgeLayer::FEdgeDraw Edge;
			Edge.Start = NodePositions[Node.Id];
			Edge.End = NodePositions[NextId];
			Edge.bTaken = Node.bVisited && Graph.Nodes[NextId].bVisited;
			Edge.bAvailable = (Node.Id == CurrentNodeId) && Run->IsNodeSelectable(NextId);
			Edge.bReachable = Reach[NextId] && (Node.Id == CurrentNodeId || Reach[Node.Id]);
			Edges.Add(Edge);
		}
	}

	for (const FRouteNode& Node : Graph.Nodes)
	{
		if (!NodeWidgets.IsValidIndex(Node.Id) || !NodeWidgets[Node.Id])
		{
			continue;
		}

		ERouteNodeVisual Visual = ERouteNodeVisual::Locked;
		if (Node.Id == CurrentNodeId)
		{
			Visual = ERouteNodeVisual::Current;
		}
		else if (Node.bVisited)
		{
			Visual = ERouteNodeVisual::Visited;
		}
		else if (Run->IsNodeSelectable(Node.Id))
		{
			Visual = ERouteNodeVisual::Available;
		}

		NodeWidgets[Node.Id]->SetNodeVisual(Visual, Reach[Node.Id]);
	}

	if (EdgeLayer)
	{
		EdgeLayer->SetEdges(Edges);
	}

	if (Ruler)
	{
		TArray<float> Xs;
		for (int32 L = 0; L < Graph.NumLayers; ++L)
		{
			Xs.Add(ERRouteLayout::LayerX(Graph, L));
		}
		const FRouteNode* Current = Run->GetCurrentNode();
		Ruler->SetRulerData(Xs, Current ? Current->Layer : 0);
	}

	if (StatText)
	{
		StatText->SetText(FText::FromString(FString::Printf(TEXT("战斗 %d 场   ·   种子 %d"),
			Run->GetBattlesFought(), Run->GetRunSeed())));
	}
	if (ProgressText)
	{
		const FRouteNode* Current = Run->GetCurrentNode();
		const int32 Layer = Current ? Current->Layer + 1 : 0;
		ProgressText->SetText(FText::FromString(FString::Printf(TEXT("第 %d / %d 层"), Layer, Graph.NumLayers)));
	}
}

void UERRouteHUD::ShowNotification(const FText& Message, float Duration)
{
	if (NotificationBorder && NotificationText)
	{
		NotificationText->SetText(Message);
		NotificationBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		NotificationBorder->SetRenderOpacity(1.0f);
		NotificationTimeLeft = Duration;
	}
	if (Anim_Notify)
	{
		PlayAnimationForward(Anim_Notify);
	}
}

bool UERRouteHUD::GetMapLayout(FVector2D& OutOriginPx, float& OutScalePx) const
{
	if (!NodeCanvas)
	{
		return false;
	}
	const FGeometry& Canvas = NodeCanvas->GetTickSpaceGeometry();
	const FGeometry& Own = GetTickSpaceGeometry();
	if (Canvas.GetLocalSize().X <= 1.0 || Own.GetAbsoluteSize().X <= 1.0)
	{
		return false;
	}
	OutOriginPx = Canvas.GetAbsolutePosition() - Own.GetAbsolutePosition();
	OutScalePx = static_cast<float>(Canvas.GetAbsoluteSize().X / Canvas.GetLocalSize().X);
	return true;
}

void UERRouteHUD::HandleNodeClicked(int32 NodeId)
{
	URunSubsystem* Run = RunSubsystem.Get();
	if (Run && Run->IsNodeSelectable(NodeId))
	{
		OnNodeSelected.Broadcast(NodeId);
	}
}

void UERRouteHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (NotificationTimeLeft > 0.0f && NotificationBorder)
	{
		NotificationTimeLeft -= InDeltaTime;
		if (NotificationTimeLeft <= 0.0f)
		{
			NotificationBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			NotificationBorder->SetRenderOpacity(FMath::Clamp(NotificationTimeLeft / 0.55f, 0.0f, 1.0f));
		}
	}
}
