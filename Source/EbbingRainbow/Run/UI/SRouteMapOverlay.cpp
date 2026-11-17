// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/UI/SRouteMapOverlay.h"
#include "Core/ERLog.h"

#include "Battle/UI/SBattleGlassKit.h"
#include "Run/RouteLayout.h"
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

namespace
{
	// 航线图是深色场景: 文字走自己的浅色三阶 (BattleGlass 的文字色是为白底准备的深石板色)
	const FLinearColor TextHi(0.93f, 0.96f, 1.00f, 0.96f);
	const FLinearColor TextMid(0.74f, 0.82f, 0.94f, 0.70f);
	const FLinearColor TextLo(0.62f, 0.72f, 0.88f, 0.46f);
}

void SRouteMapOverlay::Construct(const FArguments& InArgs)
{
	RunSubsystem = InArgs._RunSubsystem;
	OnNodeClicked = InArgs._OnNodeClicked;

	BattleGlass::ConfigureGlassBrush(PillBrush, 16.0f, FLinearColor(0.020f, 0.034f, 0.062f, 0.86f), 1.0f,
		FLinearColor(0.80f, 0.90f, 1.00f, 0.24f));

	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 24);
	const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	const FSlateFontInfo ProgressFont = FCoreStyle::GetDefaultFontStyle("Bold", 17);

	const int32 Seed = RunSubsystem.IsValid() ? RunSubsystem->GetRunSeed() : 0;

	// 图例: 每个节点类型一个 (图标 + 名字)
	TSharedRef<SHorizontalBox> Legend = SNew(SHorizontalBox);
	for (int32 T = 0; T < static_cast<int32>(ERouteNodeType::Count); ++T)
	{
		const FRouteNodeTypeInfo& Info = GetRouteNodeTypeInfo(static_cast<ERouteNodeType>(T));
		Legend->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
			[
				SNew(SRouteTypeIcon).Type(Info.Type).Accent(Info.Color)
			];
		Legend->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 22.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Info.DisplayName))
				.Font(LabelFont)
				.ColorAndOpacity(FSlateColor(TextMid))
			];
	}

	ChildSlot
	[
		SNew(SOverlay)

		// 背景压暗
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
		.Padding(FMargin(0.0f, ERRouteLayout::PadTop, 0.0f, ERRouteLayout::PadBottom))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::Both)
			[
				SNew(SBox)
				.WidthOverride(ERRouteLayout::MapW)
				.HeightOverride(ERRouteLayout::MapH)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SAssignNew(Ruler, SRouteLayerRuler)
					]

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

		// 左上: 标题
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(34.0f, 26.0f, 0.0f, 0.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.0f, 2.0f, 12.0f, 2.0f))
			[
				SNew(SBox)
				.WidthOverride(2.0f)
				[
					SNew(SBorder)
					.BorderImage(White)
					.BorderBackgroundColor(FLinearColor(1.0f, 0.80f, 0.34f, 0.85f))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("航线图")))
					.Font(TitleFont)
					.ShadowOffset(FVector2D(0.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
					.ColorAndOpacity(FSlateColor(TextHi))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(1.0f, 1.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("虹 不 我 与   ·   E B B I N G   R A I N B O W")))
					.Font(SubtitleFont)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.72f, 0.98f, 0.62f)))
				]
			]
		]

		// 右上: 进度 + 战绩/种子
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 26.0f, 34.0f, 0.0f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SAssignNew(ProgressText, STextBlock)
				.Font(ProgressFont)
				.ShadowOffset(FVector2D(0.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.86f, 0.46f, 0.98f)))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
			[
				SAssignNew(StatText, STextBlock)
				.Font(LabelFont)
				.ColorAndOpacity(FSlateColor(TextMid))
			]
		]

		// 左下: 图例
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(34.0f, 0.0f, 0.0f, 22.0f))
		[
			Legend
		]

		// 底部提示
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 22.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("点击发光节点前进   ·   战斗节点将进入竞技场")))
			.Font(LabelFont)
			.ColorAndOpacity(FSlateColor(TextLo))
		]

		// 通知
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 30.0f, 0.0f, 0.0f))
		[
			SAssignNew(NotificationBorder, SBorder)
			.BorderImage(&PillBrush)
			.Padding(FMargin(22.0f, 9.0f))
			.Visibility(EVisibility::Collapsed)
			[
				SAssignNew(NotificationText, STextBlock)
				.Font(ValueFont)
				.ColorAndOpacity(FSlateColor(TextHi))
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

	for (const FRouteNode& Node : Graph.Nodes)
	{
		// 层级沿 X 轴推进，Slot 沿 Y 轴分布。
		const FVector2D Center = ERRouteLayout::NodeCenter(Graph, Node);
		NodePositions[Node.Id] = Center;

		const FRouteNodeTypeInfo& Info = GetRouteNodeTypeInfo(Node.Type);
		const FText Tooltip = FText::FromString(FString::Printf(TEXT("%s\n第 %d 层 · 遭遇种子 %d"),
			Info.DisplayName, Node.Layer + 1, Node.EncounterSeed));

		TSharedPtr<SRouteNodeWidget> Widget;
		NodeCanvas->AddSlot()
			.Position(FVector2D(Center.X - ERRouteLayout::NodeW * 0.5f, Center.Y - ERRouteLayout::NodeH * 0.5f))
			.Size(FVector2D(ERRouteLayout::NodeW, ERRouteLayout::NodeH))
			[
				SAssignNew(Widget, SRouteNodeWidget)
				.NodeId(Node.Id)
				.NodeType(Node.Type)
				.Glyph(FText::FromString(Info.Glyph))
				.Label(FText::FromString(Info.DisplayName))
				.Tooltip(Tooltip)
				.Accent(Info.Color)
				.bFinal(Node.Layer == Graph.NumLayers - 1)
				.OnClicked(FOnClicked::CreateSP(this, &SRouteMapOverlay::HandleNodeButtonClicked, Node.Id))
			];

		NodeWidgets[Node.Id] = Widget;
	}

	UE_LOG(LogERRun, Log, TEXT("[RouteMap] 生成节点控件 %d 个，画布 %.0fx%.0f，边层=%s"),
		NodeWidgets.Num(), ERRouteLayout::MapW, ERRouteLayout::MapH, EdgeLayer.IsValid() ? TEXT("ok") : TEXT("missing"));
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
			Edge.bAvailable = (Node.Id == CurrentNodeId) && RunSubsystem->IsNodeSelectable(NextId);
			Edge.bReachable = Reach[NextId] && (Node.Id == CurrentNodeId || Reach[Node.Id]);
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

		NodeWidgets[Node.Id]->SetVisual(Visual, Reach[Node.Id]);
	}

	EdgeLayer->SetDrawData(Edges);

	if (Ruler.IsValid())
	{
		TArray<float> Xs;
		for (int32 L = 0; L < Graph.NumLayers; ++L)
		{
			Xs.Add(ERRouteLayout::LayerX(Graph, L));
		}
		const FRouteNode* Current = RunSubsystem->GetCurrentNode();
		Ruler->SetData(Xs, Current ? Current->Layer : 0);
	}

	if (StatText.IsValid())
	{
		StatText->SetText(GetStatLabel());
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

bool SRouteMapOverlay::GetMapLayout(FVector2D& OutOriginPx, float& OutScalePx) const
{
	if (!NodeCanvas.IsValid())
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

FText SRouteMapOverlay::GetStatLabel() const
{
	if (!RunSubsystem.IsValid())
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("战斗 %d 场   ·   种子 %d"),
		RunSubsystem->GetBattlesFought(), RunSubsystem->GetRunSeed()));
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

	return FText::FromString(FString::Printf(TEXT("第 %d / %d 层"), Layer, Graph.NumLayers));
}
