// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERRouteWidgets.h"
#include "UI/ERUIStatics.h"
#include "UI/Motion/ERUIMotion.h"
#include "Run/RouteGraphGenerator.h"

#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	FLinearColor WithAlphaCol(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	FPaintGeometry MakeBoxGeo(const FGeometry& G, const FVector2D& Pos, const FVector2D& Size)
	{
		return G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos));
	}
}

// ============================================================
// 1. SERRouteSigilLeaf & UERRouteNodeSigil
// ============================================================

class SERRouteSigilLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERRouteSigilLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		RouteVis::ConfigureCircleBrush(CircleBrush);
		ForceVolatile(true);
	}

	ERouteNodeType Type = ERouteNodeType::Safe;
	FLinearColor Accent = FLinearColor::White;
	FLinearColor Gold = FLinearColor(1.0f, 0.80f, 0.34f, 1.0f);
	bool bFinal = false;
	float BaseRadius = 34.0f;
	float FinalScale = 1.22f;
	FVector2D DesiredSize = FVector2D(88.0f, 88.0f);
	float Phase = 0.0f;
	ERouteNodeVisual Visual = ERouteNodeVisual::Locked;
	bool bReachable = false;
	FERFloatSpring HoverSpring = FERFloatSpring::Tactile(0.0f);
	FERFloatSpring PressSpring = FERFloatSpring::Snappy(0.0f);

	void Advance(float DeltaTime, bool bHovered, bool bPressed)
	{
		const bool bLive = (Visual == ERouteNodeVisual::Available);
		HoverSpring.SetTarget((bHovered && bLive) ? 1.0f : 0.0f);
		PressSpring.SetTarget((bPressed && bLive) ? 1.0f : 0.0f);
		HoverSpring.Tick(DeltaTime);
		PressSpring.Tick(DeltaTime);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return DesiredSize; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FVector2D C = AllottedGeometry.GetLocalSize() * 0.5;
		const float Time = (float)Args.GetCurrentTime() + Phase;
		const float Pulse = ERMotion::SolemnPulse(Time, 1.25f);
		const float HoverT = FMath::Clamp(HoverSpring.Value, 0.0f, 1.15f);
		const float PressT = FMath::Clamp(PressSpring.Value, 0.0f, 1.0f);
		const float Sc = (bFinal ? FinalScale : 1.0f) * (1.0f + 0.065f * HoverT - 0.045f * PressT);
		const float R = BaseRadius * Sc;
		const FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry();
		const FLinearColor Bone(0.94f, 0.94f, 0.92f, 1.0f);

		// 铁律: 仅 Current 节点发光; 其余所有节点严格黑-白-点睛彩锐利矢量, 绝不发光!
		FLinearColor Ring, Fill, ShapeCol;
		float RingW = 1.2f, GlowAmt = 0.0f, ShapeA = 0.4f, InnerA = 0.0f;
		switch (Visual)
		{
		case ERouteNodeVisual::Available:
			Ring = Bone; RingW = 2.0f;
			Fill = FLinearColor(0.008f, 0.009f, 0.012f, 0.95f);
			ShapeCol = Accent; GlowAmt = 0.0f; ShapeA = 0.92f; InnerA = 0.0f;
			break;
		case ERouteNodeVisual::Current:
			Ring = FLinearColor(1.0f, 0.88f, 0.52f, 1.0f); RingW = 2.6f;
			Fill = FLinearColor(0.070f, 0.048f, 0.014f, 0.96f);
			ShapeCol = Gold; GlowAmt = 1.05f + 0.22f * Pulse; ShapeA = 0.95f; InnerA = 0.22f;
			break;
		case ERouteNodeVisual::Visited:
			Ring = WithAlphaCol(Gold, 0.52f); RingW = 1.3f;
			Fill = FLinearColor(0.012f, 0.012f, 0.016f, 0.92f);
			ShapeCol = Gold; GlowAmt = 0.0f; ShapeA = 0.30f;
			break;
		default:
			Ring = WithAlphaCol(Bone, bReachable ? 0.38f : 0.14f); RingW = 1.0f;
			Fill = FLinearColor(0.008f, 0.009f, 0.012f, bReachable ? 0.88f : 0.72f);
			ShapeCol = bReachable ? WithAlphaCol(Accent, 0.45f) : WithAlphaCol(Bone, 0.16f);
			GlowAmt = 0.0f; ShapeA = bReachable ? 0.46f : 0.16f;
			break;
		}

		auto Disc = [&](float Radius, const FLinearColor& Col, int32 L)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + L,
				AllottedGeometry.ToPaintGeometry(FVector2D(Radius * 2.0f, Radius * 2.0f), FSlateLayoutTransform(C - FVector2D(Radius, Radius))),
				&CircleBrush, ESlateDrawEffect::None, Col);
		};
		TArray<FVector2D> P;
		auto Line = [&](const FLinearColor& Col, float W, int32 L)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + L, Geo, P, ESlateDrawEffect::None, Col, true, W);
		};

		// 仅 Current 节点绘制微弱内晕 (外层大光晕交由 3D M_Route_Halo)
		if (GlowAmt > 0.01f)
		{
			Disc(R + 5.0f * Sc, WithAlphaCol(Gold, GlowAmt * 0.055f), 0);
			Disc(R + 2.5f * Sc, WithAlphaCol(Gold, GlowAmt * 0.065f), 0);
		}

		Disc(R, Fill, 1);
		if (InnerA > 0.0f)
		{
			Disc(R * 0.84f, WithAlphaCol(Gold, InnerA * 0.5f), 1);
			Disc(R * 0.58f, WithAlphaCol(Gold, InnerA * 0.5f), 1);
		}

		// 主圆环 (Available 绝不画模糊宽发光线, 保持 2px 锐利纯白线)
		RouteVis::MakeCircle(P, C, R, 64);
		if (Visual == ERouteNodeVisual::Current)
		{
			Line(WithAlphaCol(Ring, 0.18f), RingW + 5.0f, 2);
		}
		Line(Ring, RingW, 3);

		// 内精密发丝刻度环
		if (Visual != ERouteNodeVisual::Locked || bReachable)
		{
			RouteVis::MakeCircle(P, C, R - 6.0f * Sc, 56);
			Line(WithAlphaCol(Ring, Visual == ERouteNodeVisual::Available ? 0.24f : 0.20f), 0.9f, 3);
		}

		// 类型外轮廓 (点睛色锐利细线)
		RouteVis::MakeTypeShape(P, Type, C, R + 8.0f * Sc);
		Line(WithAlphaCol(ShapeCol, ShapeA), (Visual == ERouteNodeVisual::Available || Visual == ERouteNodeVisual::Current) ? 1.5f : 1.0f, 3);

		// Available 悬停时四角精密对准括号 [ ] 弹簧收敛锁定 (无发光, 纯机能美学)
		if (Visual == ERouteNodeVisual::Available && HoverT > 0.02f)
		{
			const float BoxR = R + FMath::Lerp(17.0f, 11.0f, HoverT) * Sc;
			const float Arm = 7.5f * Sc;
			const FLinearColor BCol = WithAlphaCol(Bone, 0.92f * FMath::Clamp(HoverT, 0.0f, 1.0f));
			auto Corner = [&](float SX, float SY)
			{
				P = {
					C + FVector2D(SX * BoxR, SY * (BoxR - Arm)),
					C + FVector2D(SX * BoxR, SY * BoxR),
					C + FVector2D(SX * (BoxR - Arm), SY * BoxR)
				};
				Line(BCol, 1.5f, 4);
			};
			Corner(-1.0f, -1.0f);
			Corner( 1.0f, -1.0f);
			Corner( 1.0f,  1.0f);
			Corner(-1.0f,  1.0f);
		}

		if (Visual == ERouteNodeVisual::Current)
		{
			const float Spin = Time * 0.18f;
			for (int32 K = 0; K < 3; ++K)
			{
				const float A0 = Spin + K * (2.0f * PI / 3.0f);
				RouteVis::MakeArc(P, C, R + 13.5f * Sc, A0, A0 + 1.25f, 32);
				Line(WithAlphaCol(Ring, 0.62f), 1.5f, 3);
			}
			// 四向天体准星发丝刻度
			for (int32 K = 0; K < 4; ++K)
			{
				const float Ang = K * (PI * 0.5f);
				const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
				P = { C + Dir * (R + 10.0f * Sc), C + Dir * (R + 17.0f * Sc) };
				Line(WithAlphaCol(Ring, 0.50f), 1.1f, 3);
			}
		}
		else if (Visual == ERouteNodeVisual::Visited)
		{
			const FVector2D K = C + FVector2D(R * 0.80f, -R * 0.80f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4,
				AllottedGeometry.ToPaintGeometry(FVector2D(18.0f, 18.0f), FSlateLayoutTransform(K - FVector2D(9.0f, 9.0f))),
				&CircleBrush, ESlateDrawEffect::None, WithAlphaCol(Gold, 0.92f));
			P = { K + FVector2D(-3.6f, 0.5f), K + FVector2D(-1.0f, 3.0f), K + FVector2D(3.8f, -2.8f) };
			Line(FLinearColor(0.01f, 0.01f, 0.01f, 1.0f), 1.8f, 5);
		}

		if (bFinal)
		{
			const bool bLive = (Visual != ERouteNodeVisual::Locked);
			RouteVis::MakeCircle(P, C, R + 15.0f, 72);
			Line(WithAlphaCol(Ring, bLive ? 0.36f : 0.16f), 1.0f, 3);
		}

		return LayerId + 8;
	}

private:
	FSlateBrush CircleBrush;
};

TSharedRef<SWidget> UERRouteNodeSigil::RebuildWidget()
{
	MySigil = SNew(SERRouteSigilLeaf);
	return MySigil.ToSharedRef();
}

void UERRouteNodeSigil::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MySigil.IsValid())
	{
		MySigil->Type = NodeType;
		MySigil->Accent = AccentColor;
		MySigil->Gold = CurrentGoldColor;
		MySigil->bFinal = bIsFinal;
		MySigil->BaseRadius = BaseRadius;
		MySigil->FinalScale = FinalScaleMultiplier;
		MySigil->DesiredSize = DesiredWidgetSize;
		MySigil->Phase = Phase;
		MySigil->Visual = VisualState;
		MySigil->bReachable = bReachable;
	}
}

void UERRouteNodeSigil::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MySigil.Reset();
}

void UERRouteNodeSigil::SetNodeParams(ERouteNodeType InType, FLinearColor InAccent, bool bInFinal, float InPhase)
{
	NodeType = InType;
	AccentColor = InAccent;
	bIsFinal = bInFinal;
	Phase = InPhase;
	SynchronizeProperties();
}

void UERRouteNodeSigil::SetVisualState(ERouteNodeVisual InVisual, bool bInReachable)
{
	VisualState = InVisual;
	bReachable = bInReachable;
	SynchronizeProperties();
}

void UERRouteNodeSigil::AdvanceInteraction(float DeltaTime, bool bHovered, bool bPressed)
{
	if (MySigil.IsValid())
	{
		MySigil->Advance(DeltaTime, bHovered, bPressed);
	}
}

// ============================================================
// 2. SERRouteEdgeLeaf & UERRouteEdgeLayer
// ============================================================

class SERRouteEdgeLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERRouteEdgeLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { ForceVolatile(true); }

	TArray<SRouteEdgeLayer::FEdgeDraw> Edges;
	TArray<FVector2D> DesignNodeCenters;
	FLinearColor AvailCol, TakenCol, ReachCol, LockedCol;
	float AvailW = 1.7f, TakenW = 1.5f, ReachW = 1.2f, LockedW = 1.0f;
	float TrimRadius = ERRouteLayout::EdgeTrim;

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(ERRouteLayout::MapW, ERRouteLayout::MapH);
	}

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry();
		TArray<FVector2D> Pts;

		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			for (const SRouteEdgeLayer::FEdgeDraw& E : Edges)
			{
				const int32 Rank = E.bAvailable ? 3 : (E.bTaken ? 2 : (E.bReachable ? 1 : 0));
				if (Rank != Pass) { continue; }

				ERRouteLayout::SampleEdge(E.Start, E.End, Pts, 64, TrimRadius, TrimRadius);
				if (Pts.Num() < 2) { continue; }

				FLinearColor Color;
				float Thickness;
				switch (Rank)
				{
				case 3:  Color = AvailCol;  Thickness = AvailW;  break;
				case 2:  Color = TakenCol;  Thickness = TakenW;  break;
				case 1:  Color = ReachCol;  Thickness = ReachW;  break;
				default: Color = LockedCol; Thickness = LockedW; break;
				}
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Pass, Geo, Pts, ESlateDrawEffect::None, Color, true, Thickness);
			}
		}

		// 设计期辅助: 在节点坐标处画出轻柔定位环, 方便在 WBP_RouteHUD 里看清 DAG 落点
		for (const FVector2D& Center : DesignNodeCenters)
		{
			RouteVis::MakeCircle(Pts, Center, 34.0f, 36);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 4, Geo, Pts, ESlateDrawEffect::None,
				FLinearColor(0.75f, 0.86f, 1.0f, 0.35f), true, 1.2f);
		}

		return LayerId + 5;
	}
};

TSharedRef<SWidget> UERRouteEdgeLayer::RebuildWidget()
{
	MyEdgeLeaf = SNew(SERRouteEdgeLeaf);
	return MyEdgeLeaf.ToSharedRef();
}

void UERRouteEdgeLayer::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyEdgeLeaf.IsValid())
	{
		return;
	}

	MyEdgeLeaf->AvailCol = AvailableEdgeColor;
	MyEdgeLeaf->AvailW = AvailableEdgeThickness;
	MyEdgeLeaf->TakenCol = TakenEdgeColor;
	MyEdgeLeaf->TakenW = TakenEdgeThickness;
	MyEdgeLeaf->ReachCol = ReachableEdgeColor;
	MyEdgeLeaf->ReachW = ReachableEdgeThickness;
	MyEdgeLeaf->LockedCol = LockedEdgeColor;
	MyEdgeLeaf->LockedW = LockedEdgeThickness;
	MyEdgeLeaf->TrimRadius = EdgeTrimRadius;

	const bool bInGame = (GetWorld() != nullptr) && GetWorld()->IsGameWorld();
	if (!bInGame && CachedEdges.Num() == 0 && bShowDesignPreview)
	{
		FRouteGraph PreviewGraph;
		FRouteGraphGenerator::Generate(PreviewSeed, FRouteGenSettings(), PreviewGraph);
		TArray<FVector2D> Centers;
		Centers.SetNum(PreviewGraph.Nodes.Num());
		for (const FRouteNode& N : PreviewGraph.Nodes)
		{
			Centers[N.Id] = ERRouteLayout::NodeCenter(PreviewGraph, N);
		}

		TArray<SRouteEdgeLayer::FEdgeDraw> PreviewEdges;
		for (const FRouteNode& N : PreviewGraph.Nodes)
		{
			for (const int32 NextId : N.Next)
			{
				if (!Centers.IsValidIndex(N.Id) || !Centers.IsValidIndex(NextId)) { continue; }
				SRouteEdgeLayer::FEdgeDraw ED;
				ED.Start = Centers[N.Id];
				ED.End = Centers[NextId];
				ED.bTaken = (N.Layer == 0);
				ED.bAvailable = (N.Layer == 1);
				ED.bReachable = (N.Layer >= 2);
				PreviewEdges.Add(ED);
			}
		}
		MyEdgeLeaf->Edges = PreviewEdges;
		MyEdgeLeaf->DesignNodeCenters = Centers;
	}
	else
	{
		MyEdgeLeaf->Edges = CachedEdges;
		MyEdgeLeaf->DesignNodeCenters.Reset();
	}
}

void UERRouteEdgeLayer::SetEdges(const TArray<SRouteEdgeLayer::FEdgeDraw>& InEdges)
{
	CachedEdges = InEdges;
	SynchronizeProperties();
}

void UERRouteEdgeLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyEdgeLeaf.Reset();
}

// ============================================================
// 3. SERRouteRulerLeaf & UERRouteLayerRuler
// ============================================================

class SERRouteRulerLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERRouteRulerLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	TArray<float> LayerXs;
	int32 CurrentLayer = 0;
	float RulerY = ERRouteLayout::RulerY;
	FLinearColor Gold = FLinearColor(1.0f, 0.80f, 0.34f, 1.0f);
	FLinearColor BaseLine = FLinearColor(0.60f, 0.72f, 0.92f, 0.20f);
	FString FinalLabel = TEXT("终");
	int32 FontSize = 9;
	int32 ActiveFontSize = 10;

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(ERRouteLayout::MapW, ERRouteLayout::MapH);
	}

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		if (!White || LayerXs.Num() < 2) { return LayerId; }

		const float Y = RulerY;
		const float X0 = LayerXs[0] - 44.0f;
		const float X1 = LayerXs.Last() + 44.0f;
		const int32 Cur = FMath::Clamp(CurrentLayer, 0, LayerXs.Num() - 1);

		auto Rect = [&](float X, float YY, float W, float H, const FLinearColor& C)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, MakeBoxGeo(AllottedGeometry, FVector2D(X, YY), FVector2D(W, H)),
				White, ESlateDrawEffect::None, C);
		};

		Rect(X0, Y - 0.5f, X1 - X0, 1.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.16f));
		const float XC = LayerXs[Cur];
		Rect(X0, Y - 0.8f, XC - X0, 1.6f, FLinearColor(Gold.R, Gold.G, Gold.B, 0.88f));

		FSlateFontInfo Font = ERUI::FontRegular(FontSize, 160);
		FSlateFontInfo FontBold = ERUI::FontBold(ActiveFontSize + 1, 120);
		TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		for (int32 I = 0; I < LayerXs.Num(); ++I)
		{
			const float X = LayerXs[I];
			const bool bPast = I < Cur, bNow = I == Cur;
			const bool bLast = I == LayerXs.Num() - 1;
			const FLinearColor TickC = bNow ? FLinearColor(1.0f, 0.90f, 0.58f, 1.0f)
				: bPast ? FLinearColor(Gold.R, Gold.G, Gold.B, 0.68f)
				: FLinearColor(0.92f, 0.92f, 0.90f, 0.34f);

			// 层间精密副刻度
			if (I + 1 < LayerXs.Num())
			{
				const float MidX = 0.5f * (X + LayerXs[I + 1]);
				Rect(MidX - 0.5f, Y - 2.5f, 1.0f, 5.0f, FLinearColor(0.92f, 0.92f, 0.90f, 0.12f));
			}

			if (bNow)
			{
				TArray<FVector2D> D;
				RouteVis::MakePolygon(D, FVector2D(X, Y), 7.5f, 4, -PI * 0.5f);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), D,
					ESlateDrawEffect::None, TickC, true, 1.6f);
				Rect(X - 2.0f, Y - 2.0f, 4.0f, 4.0f, TickC);
			}
			else
			{
				Rect(X - 0.5f, Y - (bLast ? 7.0f : 4.5f), 1.0f, bLast ? 14.0f : 9.0f, TickC);
			}

			const FString Label = bLast ? FString::Printf(TEXT("%02d · %s"), I + 1, *FinalLabel) : FString::Printf(TEXT("%02d"), I + 1);
			const FSlateFontInfo& F = (bNow || bLast) ? FontBold : Font;
			const FVector2D TS = Measure->Measure(Label, F);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				MakeBoxGeo(AllottedGeometry, FVector2D(X - TS.X * 0.5f, Y + 13.0f), TS),
				Label, F, ESlateDrawEffect::None,
				bNow ? FLinearColor(1.0f, 0.88f, 0.55f, 0.98f)
				: bPast ? FLinearColor(Gold.R, Gold.G, Gold.B, 0.60f)
				: bLast ? FLinearColor(0.94f, 0.94f, 0.92f, 0.72f)
				: FLinearColor(0.92f, 0.92f, 0.90f, 0.36f));
		}

		return LayerId + 2;
	}
};

TSharedRef<SWidget> UERRouteLayerRuler::RebuildWidget()
{
	MyRulerLeaf = SNew(SERRouteRulerLeaf);
	return MyRulerLeaf.ToSharedRef();
}

void UERRouteLayerRuler::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyRulerLeaf.IsValid())
	{
		return;
	}

	MyRulerLeaf->RulerY = RulerY;
	MyRulerLeaf->Gold = GoldColor;
	MyRulerLeaf->BaseLine = BaseLineColor;
	MyRulerLeaf->FinalLabel = FinalLayerText.ToString();
	MyRulerLeaf->FontSize = NumberFontSize;
	MyRulerLeaf->ActiveFontSize = ActiveNumberFontSize;

	const bool bInGame = (GetWorld() != nullptr) && GetWorld()->IsGameWorld();
	if (!bInGame && CachedLayerXs.Num() == 0)
	{
		TArray<float> PreviewXs;
		const int32 N = FMath::Max(2, PreviewNumLayers);
		const float DX = (ERRouteLayout::MapW - 2.0f * ERRouteLayout::MarginX) / (float)(N - 1);
		for (int32 L = 0; L < N; ++L)
		{
			PreviewXs.Add(ERRouteLayout::MarginX + L * DX);
		}
		MyRulerLeaf->LayerXs = PreviewXs;
		MyRulerLeaf->CurrentLayer = FMath::Clamp(PreviewCurrentLayer, 0, N - 1);
	}
	else
	{
		MyRulerLeaf->LayerXs = CachedLayerXs;
		MyRulerLeaf->CurrentLayer = CachedCurrentLayer;
	}
}

void UERRouteLayerRuler::SetRulerData(const TArray<float>& InLayerXs, int32 InCurrentLayer)
{
	CachedLayerXs = InLayerXs;
	CachedCurrentLayer = InCurrentLayer;
	SynchronizeProperties();
}

void UERRouteLayerRuler::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyRulerLeaf.Reset();
}

// ============================================================
// 4. SERRouteTypeIconLeaf & UERRouteTypeIcon
// ============================================================

class SERRouteTypeIconLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERRouteTypeIconLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	ERouteNodeType Type = ERouteNodeType::Safe;
	FLinearColor Accent = FLinearColor::White;
	float Radius = 7.0f;
	float Thickness = 1.6f;
	FVector2D DesiredSize = FVector2D(20.0f, 20.0f);

	virtual FVector2D ComputeDesiredSize(float) const override { return DesiredSize; }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FVector2D C = AllottedGeometry.GetLocalSize() * 0.5;
		TArray<FVector2D> P;
		RouteVis::MakeTypeShape(P, Type, C, Radius);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P,
			ESlateDrawEffect::None, FLinearColor(Accent.R, Accent.G, Accent.B, 0.95f), true, Thickness);
		RouteVis::MakeCircle(P, C, 2.2f, 12);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P,
			ESlateDrawEffect::None, FLinearColor(Accent.R, Accent.G, Accent.B, 0.75f), true, Thickness * 1.5f);
		return LayerId;
	}
};

TSharedRef<SWidget> UERRouteTypeIcon::RebuildWidget()
{
	MyIconLeaf = SNew(SERRouteTypeIconLeaf);
	return MyIconLeaf.ToSharedRef();
}

void UERRouteTypeIcon::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyIconLeaf.IsValid())
	{
		MyIconLeaf->Type = NodeType;
		MyIconLeaf->Accent = AccentColor;
		MyIconLeaf->Radius = ShapeRadius;
		MyIconLeaf->Thickness = StrokeThickness;
		MyIconLeaf->DesiredSize = IconDesiredSize;
	}
}

void UERRouteTypeIcon::SetIconParams(ERouteNodeType InType, FLinearColor InAccent)
{
	NodeType = InType;
	AccentColor = InAccent;
	SynchronizeProperties();
}

void UERRouteTypeIcon::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyIconLeaf.Reset();
}
