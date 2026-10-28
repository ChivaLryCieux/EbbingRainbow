// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/UI/SRouteMapOverlay.h"

#include "Battle/UI/SBattleGlassKit.h"
#include "Run/RunSubsystem.h"
#include "Run/UI/SRouteMapVisuals.h"
#include "Run/UI/SRouteNodeWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SRouteMapOverlay::Construct(const FArguments& InArgs)
{
	RunSubsystem = InArgs._RunSubsystem;
	OnNodeClicked = InArgs._OnNodeClicked;

	BattleGlass::ConfigureGlassBrush(PanelBrush, 20.0f, FLinearColor(0.040f, 0.060f, 0.095f, 0.72f), 1.0f,
		FLinearColor(0.90f, 0.95f, 1.00f, 0.16f));
	BattleGlass::ConfigureGlassBrush(PillBrush, 14.0f, FLinearColor(0.060f, 0.090f, 0.135f, 0.82f), 1.0f,
		FLinearColor(0.90f, 0.95f, 1.00f, 0.22f));

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);

	const int32 Seed = RunSubsystem.IsValid() ? RunSubsystem->GetRunSeed() : 0;

	ChildSlot
	[
		SNew(SOverlay)

		// 背景
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SRouteBackdrop)
			.Seed(Seed)
		]

		// 地图本体（固定虚拟画布，等比缩放）
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 88.0f, 0.0f, 58.0f))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::Both)
			[
				SNew(SBox)
				.WidthOverride(MapWidth)
				.HeightOverride(MapHeight)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SAssignNew(EdgeLayer, SRouteEdgeLayer)
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SAssignNew(NodeCanvas, SCanvas)
					]
				]
			]
		]

		// 顶部：标题 + 种子/进度
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.Padding(FMargin(26.0f, 20.0f, 26.0f, 0.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(FMargin(18.0f, 10.0f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("航线图")))
						.Font(TitleFont)
						.ColorAndOpacity(FSlateColor(BattleGlass::TextPrimary))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(1.0f, 1.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("ROUTE  MAP")))
						.Font(SubtitleFont)
						.ColorAndOpacity(FSlateColor(BattleGlass::TextTertiary))
					]
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(&PillBrush)
				.Padding(FMargin(16.0f, 9.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("种子")))
						.Font(LabelFont)
						.ColorAndOpacity(FSlateColor(BattleGlass::TextTertiary))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
					[
						SAssignNew(SeedText, STextBlock)
						.Font(ValueFont)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.90f, 1.00f, 0.95f)))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
					[
						SAssignNew(ProgressText, STextBlock)
						.Font(LabelFont)
						.ColorAndOpacity(FSlateColor(BattleGlass::TextSecondary))
					]
				]
			]
		]

		// 底部提示
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 24.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("点击高亮节点前进  ·  战斗节点将进入竞技场  ·  同一种子生成同一张地图")))
			.Font(LabelFont)
			.ColorAndOpacity(FSlateColor(BattleGlass::TextTertiary))
		]

		// 通知
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 62.0f))
		[
			SAssignNew(NotificationBorder, SBorder)
			.BorderImage(&PillBrush)
			.Padding(FMargin(20.0f, 10.0f))
			.Visibility(EVisibility::Collapsed)
			[
				SAssignNew(NotificationText, STextBlock)
				.Font(ValueFont)
				.ColorAndOpacity(FSlateColor(BattleGlass::TextPrimary))
			]
		]
	];

	RebuildNodes();
	Refresh();
}

void SRouteMapOverlay::RebuildNodes()
{
	if (!NodeCanvas.IsValid())
	{
		return;
	}

	NodeCanvas->ClearChildren();
	NodeWidgets.Reset();
	NodePositions.Reset();

	if (!RunSubsystem.IsValid())
	{
		return;
	}

	const FRouteGraph& Graph = RunSubsystem->GetGraph();
	if (!Graph.IsValid())
	{
		return;
	}

	NodePositions.SetNum(Graph.Nodes.Num());
	NodeWidgets.SetNum(Graph.Nodes.Num());

	const float DeltaX = (MapWidth - 2.0f * MarginX) / FMath::Max(1, Graph.NumLayers - 1);
	const float DeltaY = (MapHeight - 2.0f * MarginY) / FMath::Max(1, Graph.NumSlots - 1);

	for (const FRouteNode& Node : Graph.Nodes)
	{
		// 层级沿 X 轴推进，Slot 沿 Y 轴分布。
		const FVector2D Center(MarginX + Node.Layer * DeltaX, MarginY + Node.Slot * DeltaY);
		NodePositions[Node.Id] = Center;

		const FRouteNodeTypeInfo& Info = GetRouteNodeTypeInfo(Node.Type);
		const FText Tooltip = FText::FromString(FString::Printf(TEXT("%s\n第 %d 层 · 遭遇种子 %d"),
			Info.DisplayName, Node.Layer + 1, Node.EncounterSeed));

		TSharedPtr<SRouteNodeWidget> Widget;
		NodeCanvas->AddSlot()
			.Position(FVector2D(Center.X - NodeWidth * 0.5f, Center.Y - NodeHeight * 0.5f))
			.Size(FVector2D(NodeWidth, NodeHeight))
			[
				SAssignNew(Widget, SRouteNodeWidget)
				.NodeId(Node.Id)
				.Glyph(FText::FromString(Info.Glyph))
				.Label(FText::FromString(Info.DisplayName))
				.Tooltip(Tooltip)
				.Accent(Info.Color)
				.OnClicked(FOnClicked::CreateSP(this, &SRouteMapOverlay::HandleNodeButtonClicked, Node.Id))
			];

		NodeWidgets[Node.Id] = Widget;
	}

	UE_LOG(LogTemp, Log, TEXT("[RouteMap] 生成节点控件 %d 个，画布 %.0fx%.0f，边层=%s"),
		NodeWidgets.Num(), MapWidth, MapHeight, EdgeLayer.IsValid() ? TEXT("ok") : TEXT("missing"));
}

void SRouteMapOverlay::Refresh()
{
	if (!RunSubsystem.IsValid() || !EdgeLayer.IsValid())
	{
		return;
	}

	const FRouteGraph& Graph = RunSubsystem->GetGraph();
	if (!Graph.IsValid())
	{
		return;
	}

	const int32 CurrentNodeId = RunSubsystem->GetCurrentNodeId();

	TArray<SRouteEdgeLayer::FEdgeDraw> Edges;
	TArray<FVector2D> Halos;

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
			Edge.bAvailable = (Node.Id == CurrentNodeId) && RunSubsystem->IsNodeSelectable(NextId);
			Edges.Add(Edge);
		}
	}

	for (const FRouteNode& Node : Graph.Nodes)
	{
		if (!NodeWidgets.IsValidIndex(Node.Id) || !NodeWidgets[Node.Id].IsValid())
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
		else if (RunSubsystem->IsNodeSelectable(Node.Id))
		{
			Visual = ERouteNodeVisual::Available;
		}

		NodeWidgets[Node.Id]->SetVisual(Visual);
	}

	if (NodePositions.IsValidIndex(CurrentNodeId))
	{
		Halos.Add(NodePositions[CurrentNodeId]);
	}

	EdgeLayer->SetDrawData(Edges, Halos);

	if (SeedText.IsValid())
	{
		SeedText->SetText(GetSeedLabel());
	}
	if (ProgressText.IsValid())
	{
		ProgressText->SetText(GetProgressLabel());
	}
}

void SRouteMapOverlay::ShowNotification(const FText& Message, float Duration)
{
	if (!NotificationBorder.IsValid() || !NotificationText.IsValid())
	{
		return;
	}

	NotificationText->SetText(Message);
	NotificationBorder->SetVisibility(EVisibility::SelfHitTestInvisible);
	NotificationBorder->SetRenderOpacity(1.0f);
	NotificationTimeLeft = Duration;
}

FReply SRouteMapOverlay::HandleNodeButtonClicked(int32 NodeId)
{
	if (RunSubsystem.IsValid() && RunSubsystem->IsNodeSelectable(NodeId))
	{
		OnNodeClicked.ExecuteIfBound(NodeId);
	}
	return FReply::Handled();
}

void SRouteMapOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NotificationTimeLeft > 0.0f && NotificationBorder.IsValid())
	{
		NotificationTimeLeft -= InDeltaTime;
		if (NotificationTimeLeft <= 0.0f)
		{
			NotificationBorder->SetVisibility(EVisibility::Collapsed);
		}
		else
		{
			NotificationBorder->SetRenderOpacity(FMath::Clamp(NotificationTimeLeft / 0.55f, 0.0f, 1.0f));
		}
	}
}

FText SRouteMapOverlay::GetSeedLabel() const
{
	const int32 Seed = RunSubsystem.IsValid() ? RunSubsystem->GetRunSeed() : 0;
	return FText::FromString(FString::Printf(TEXT("%d"), Seed));
}

FText SRouteMapOverlay::GetProgressLabel() const
{
	if (!RunSubsystem.IsValid())
	{
		return FText::GetEmpty();
	}

	const FRouteGraph& Graph = RunSubsystem->GetGraph();
	const FRouteNode* Current = RunSubsystem->GetCurrentNode();
	const int32 Layer = Current ? Current->Layer + 1 : 0;

	return FText::FromString(FString::Printf(TEXT("第 %d / %d 层   ·   战斗 %d 场"),
		Layer, Graph.NumLayers, RunSubsystem->GetBattlesFought()));
}
