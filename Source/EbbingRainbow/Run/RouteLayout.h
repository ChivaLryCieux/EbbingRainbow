// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Run/RunTypes.h"

/**
 * 航线图布局的唯一真相源: Slate 叠加层(SRouteMapOverlay)与 3D 场景(ARouteSceneBuilder)
 * 共用, 保证 3D 光晕/连线严格落在 Slate 节点正下方。
 * 画布是 MapW x MapH 的虚拟像素空间, 等比缩放进 (视口 - PadTop - PadBottom)。
 */
namespace ERRouteLayout
{
	constexpr float MapW = 2040.0f;
	constexpr float MapH = 980.0f;
	constexpr float MarginX = 150.0f;
	constexpr float MarginTop = 84.0f;
	/** 底部留给层级标尺 */
	constexpr float MarginBottom = 140.0f;
	constexpr float RulerY = MapH - 34.0f;

	constexpr float NodeW = 120.0f;
	constexpr float NodeH = 150.0f;
	/** 徽章半径 (画布像素) */
	constexpr float BadgeR = 44.0f;
	/** 连线在节点两端让出的半径 */
	constexpr float EdgeTrim = 56.0f;

	constexpr float PadTop = 88.0f;
	constexpr float PadBottom = 58.0f;

	inline float LayerX(const FRouteGraph& Graph, int32 Layer)
	{
		const float DX = (MapW - 2.0f * MarginX) / FMath::Max(1, Graph.NumLayers - 1);
		return MarginX + Layer * DX;
	}

	/** 同层相邻节点的最小纵向间距 (画布像素): 徽章直径 + 类型名 + 呼吸空间 */
	constexpr float MinLayerGap = 172.0f;

	/**
	 * 节点徽章中心 (画布坐标)。Y 先按 Slot 落格, 再对同层节点做最小间距松弛 (相邻 Slot 会被推开),
	 * X 带种子化的轻微抖动让图不像棋盘。纯函数, 同一张图结果恒定。
	 */
	inline FVector2D NodeCenter(const FRouteGraph& Graph, const FRouteNode& Node)
	{
		const float Top = MarginTop, Bottom = MapH - MarginBottom;
		const float DY = (Bottom - Top) / FMath::Max(1, Graph.NumSlots - 1);

		TArray<const FRouteNode*> Layer;
		for (const FRouteNode& N : Graph.Nodes)
		{
			if (N.Layer == Node.Layer) { Layer.Add(&N); }
		}
		Layer.Sort([](const FRouteNode& A, const FRouteNode& B) { return A.Slot < B.Slot; });

		TArray<float> Y;
		for (const FRouteNode* N : Layer) { Y.Add(Top + N->Slot * DY); }
		for (int32 Pass = 0; Pass < 12; ++Pass)
		{
			for (int32 I = 1; I < Y.Num(); ++I)
			{
				const float Gap = Y[I] - Y[I - 1];
				if (Gap < MinLayerGap)
				{
					const float D = (MinLayerGap - Gap) * 0.5f;
					Y[I] += D;
					Y[I - 1] -= D;
				}
			}
			for (float& V : Y) { V = FMath::Clamp(V, Top, Bottom); }
		}

		float NodeY = Top + Node.Slot * DY;
		for (int32 I = 0; I < Layer.Num(); ++I)
		{
			if (Layer[I]->Id == Node.Id) { NodeY = Y[I]; }
		}

		FRandomStream Rng(Graph.Seed ^ (Node.Id * 7919 + 101));
		const bool bEnds = (Node.Layer == 0 || Node.Layer == Graph.NumLayers - 1);
		const float JX = bEnds ? 0.0f : static_cast<float>(Rng.FRandRange(-14.0f, 14.0f));
		return FVector2D(LayerX(Graph, Node.Layer) + JX, NodeY);
	}

	/** 画布坐标 -> 视口像素 */
	inline FVector2D CanvasToScreen(const FVector2D& CanvasPos, const FVector2D& ViewSize)
	{
		const float AvailW = ViewSize.X;
		const float AvailH = ViewSize.Y - PadTop - PadBottom;
		const float Scale = FMath::Min(AvailW / MapW, AvailH / MapH);
		const float DrawW = MapW * Scale, DrawH = MapH * Scale;
		const float OX = (AvailW - DrawW) * 0.5f;
		const float OY = PadTop + (AvailH - DrawH) * 0.5f;
		return FVector2D(OX + CanvasPos.X * Scale, OY + CanvasPos.Y * Scale);
	}

	inline float CanvasScale(const FVector2D& ViewSize)
	{
		return FMath::Min(ViewSize.X / MapW, (ViewSize.Y - PadTop - PadBottom) / MapH);
	}

	/** 三次贝塞尔: 层级沿 X 推进, 切向沿 X (Slate 连线与 3D 光带共用同一条曲线) */
	inline FVector2D EdgeBezier(const FVector2D& S, const FVector2D& E, double T)
	{
		const double DX = E.X - S.X;
		const double U = 1.0 - T;
		const double W0 = U * U * U, W1 = 3.0 * U * U * T, W2 = 3.0 * U * T * T, W3 = T * T * T;
		return FVector2D(
			S.X * W0 + (S.X + DX * 0.45) * W1 + (E.X - DX * 0.45) * W2 + E.X * W3,
			S.Y * W0 + S.Y * W1 + E.Y * W2 + E.Y * W3);
	}

	/** 采样并裁掉两端 Trim 半径内的点 */
	inline void SampleEdge(const FVector2D& S, const FVector2D& E, TArray<FVector2D>& Out,
		int32 Segments = 32, float TrimStart = EdgeTrim, float TrimEnd = EdgeTrim)
	{
		Out.Reset(Segments + 1);
		for (int32 I = 0; I <= Segments; ++I)
		{
			const FVector2D P = EdgeBezier(S, E, (double)I / Segments);
			if (FVector2D::Distance(P, S) < TrimStart || FVector2D::Distance(P, E) < TrimEnd) { continue; }
			Out.Add(P);
		}
	}

	/** 从 FromId 出发可达的所有后继节点 (不含自身) */
	inline void ComputeReachable(const FRouteGraph& Graph, int32 FromId, TArray<bool>& Out)
	{
		Out.Init(false, Graph.Nodes.Num());
		TArray<int32> Stack;
		if (Graph.Nodes.IsValidIndex(FromId)) { Stack.Add(FromId); }
		while (Stack.Num() > 0)
		{
			const int32 Id = Stack.Pop();
			for (const int32 N : Graph.Nodes[Id].Next)
			{
				if (Graph.Nodes.IsValidIndex(N) && !Out[N]) { Out[N] = true; Stack.Add(N); }
			}
		}
	}

	inline const FLinearColor& GoldColor()
	{
		static const FLinearColor C(1.0f, 0.80f, 0.34f, 1.0f);
		return C;
	}
}
