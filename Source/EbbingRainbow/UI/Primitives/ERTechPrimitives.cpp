// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Primitives/ERTechPrimitives.h"
#include "UI/ERUIStatics.h"
#include "UI/Motion/ERUIMotion.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	/**
	 * 为 FSlateDrawElement::MakeCustomVerts 提供 100% 有效的纯白纹理句柄:
	 * 注意 FCoreStyle::WhiteBrush 是无纹理的 FSlateColorBrush, GetResourceHandle() 会返回无效句柄导致 MakeCustomVerts 静默跳过!
	 * 必须绑定真实 UTexture2D (/Game/UI/Textures/T_UI_White) 或带引擎纹理的 GenericWhiteBox。
	 */
	FSlateResourceHandle GetWhiteResourceHandle()
	{
		if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return FSlateResourceHandle();
		}
		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();

		if (UTexture2D* WhiteTex = ERUI::GetWhiteTexture())
		{
			static FSlateBrush ProjectWhiteBrush;
			if (ProjectWhiteBrush.GetResourceObject() != WhiteTex)
			{
				ProjectWhiteBrush.SetResourceObject(WhiteTex);
				ProjectWhiteBrush.DrawAs = ESlateBrushDrawType::Image;
				ProjectWhiteBrush.ImageSize = FVector2D(4.0f, 4.0f);
				ProjectWhiteBrush.TintColor = FSlateColor(FLinearColor::White);
			}
			const FSlateResourceHandle Handle = Renderer->GetResourceHandle(ProjectWhiteBrush);
			if (Handle.IsValid())
			{
				return Handle;
			}
		}

		if (const FSlateBrush* GenericWhite = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
		{
			const FSlateResourceHandle Handle = Renderer->GetResourceHandle(*GenericWhite);
			if (Handle.IsValid())
			{
				return Handle;
			}
		}

		if (const FSlateBrush* Def = FCoreStyle::Get().GetDefaultBrush())
		{
			return Renderer->GetResourceHandle(*Def);
		}
		return FSlateResourceHandle();
	}

	/** 构造顺时针切角多边形顶点 (支持独立 4 角 45° 切角) */
	void BuildChamferPolygon(const FVector2f& Size, const FVector4f& Chamfer, TArray<FVector2f>& OutPts)
	{
		OutPts.Reset(8);
		const float W = Size.X;
		const float H = Size.Y;
		const float MaxC = FMath::Max(0.0f, FMath::Min(W, H) * 0.45f);
		const float TL = FMath::Clamp(Chamfer.X, 0.0f, MaxC);
		const float TR = FMath::Clamp(Chamfer.Y, 0.0f, MaxC);
		const float BR = FMath::Clamp(Chamfer.Z, 0.0f, MaxC);
		const float BL = FMath::Clamp(Chamfer.W, 0.0f, MaxC);

		auto AddUniquePt = [&OutPts](float X, float Y)
		{
			const FVector2f P(X, Y);
			if (OutPts.Num() == 0 || !OutPts.Last().Equals(P, 0.05f))
			{
				OutPts.Add(P);
			}
		};

		AddUniquePt(TL, 0.0f);
		AddUniquePt(W - TR, 0.0f);
		if (TR > 0.05f) { AddUniquePt(W, TR); }
		AddUniquePt(W, H - BR);
		if (BR > 0.05f) { AddUniquePt(W - BR, H); }
		AddUniquePt(BL, H);
		if (BL > 0.05f) { AddUniquePt(0.0f, H - BL); }
		AddUniquePt(0.0f, TL);
	}

	/** 用 GPU 三角扇填充凸多边形 */
	void DrawConvexFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, const TArray<FVector2f>& Poly, const FLinearColor& FillColor)
	{
		if (Poly.Num() < 3 || FillColor.A <= 0.001f)
		{
			return;
		}
		const FSlateResourceHandle Handle = GetWhiteResourceHandle();
		if (!Handle.IsValid())
		{
			return;
		}

		const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
		const FColor Col = FillColor.ToFColor(true);

		FVector2f Center(0.0f, 0.0f);
		for (const FVector2f& P : Poly) { Center += P; }
		Center /= (float)Poly.Num();

		TArray<FSlateVertex> Verts;
		Verts.Reserve(Poly.Num() + 1);
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, Center, FVector2f(0.5f, 0.5f), Col));
		for (const FVector2f& P : Poly)
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, P, FVector2f(0.5f, 0.5f), Col));
		}

		TArray<SlateIndex> Indices;
		Indices.Reserve(Poly.Num() * 3);
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			Indices.Add(0);
			Indices.Add(1 + i);
			Indices.Add(1 + ((i + 1) % Poly.Num()));
		}

		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Verts, Indices, nullptr, 0, 0);
	}

	/** 绘制 3 段光谱渐变矩形 (冷青 -> 折射暖金 -> 控制朱红) */
	void DrawPrismRect(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, const FVector2f& Pos, const FVector2f& Size, bool bVertical,
		const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2)
	{
		if (Size.X <= 0.05f || Size.Y <= 0.05f)
		{
			return;
		}
		const FSlateResourceHandle Handle = GetWhiteResourceHandle();
		if (!Handle.IsValid())
		{
			return;
		}
		const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
		const FColor Col0 = C0.ToFColor(true);
		const FColor Col1 = C1.ToFColor(true);
		const FColor Col2 = C2.ToFColor(true);

		TArray<FSlateVertex> Verts;
		Verts.Reserve(6);
		if (bVertical)
		{
			const float MidY = Pos.Y + Size.Y * 0.52f;
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X, Pos.Y), FVector2f(0.5f, 0.5f), Col0));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X + Size.X, Pos.Y), FVector2f(0.5f, 0.5f), Col0));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X, MidY), FVector2f(0.5f, 0.5f), Col1));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X + Size.X, MidY), FVector2f(0.5f, 0.5f), Col1));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X, Pos.Y + Size.Y), FVector2f(0.5f, 0.5f), Col2));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X + Size.X, Pos.Y + Size.Y), FVector2f(0.5f, 0.5f), Col2));
		}
		else
		{
			const float MidX = Pos.X + Size.X * 0.52f;
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X, Pos.Y), FVector2f(0.5f, 0.5f), Col0));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X, Pos.Y + Size.Y), FVector2f(0.5f, 0.5f), Col0));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(MidX, Pos.Y), FVector2f(0.5f, 0.5f), Col1));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(MidX, Pos.Y + Size.Y), FVector2f(0.5f, 0.5f), Col1));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X + Size.X, Pos.Y), FVector2f(0.5f, 0.5f), Col2));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pos.X + Size.X, Pos.Y + Size.Y), FVector2f(0.5f, 0.5f), Col2));
		}

		static const TArray<SlateIndex> Indices = { 0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4 };
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Verts, Indices, nullptr, 0, 0);
	}
}

// ============================================================
// 1. SERTechFrameLeaf & UERTechFrame
// ============================================================

class SERTechFrameLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERTechFrameLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	FVector4f Chamfer = FVector4f(12.0f, 0.0f, 12.0f, 0.0f);
	FLinearColor FillColor = FLinearColor(0.008f, 0.009f, 0.012f, 0.92f);
	FLinearColor BorderColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.18f);
	float BorderThickness = 1.0f;
	bool bDrawBrackets = true;
	float BracketLen = 10.0f;
	float BracketThick = 1.4f;
	FLinearColor BracketColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.72f);
	EERAccentEdge AccentEdge = EERAccentEdge::None;
	bool bPrismAccent = false;
	float AccentThick = 3.0f;
	FLinearColor AccentColor = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f);
	bool bTopSheen = true;
	float RevealAlpha = 1.0f;
	float HoverAlpha = 0.0f;
	float SweepAlpha = 0.0f;
	FVector2D DesiredSize = FVector2D(64.0f, 32.0f);

	virtual FVector2D ComputeDesiredSize(float) const override { return DesiredSize; }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FVector2f FullSize = FVector2f(Geo.GetLocalSize());
		if (FullSize.X < 2.0f || FullSize.Y < 2.0f || RevealAlpha <= 0.005f)
		{
			return LayerId;
		}

		const float Rev = ERMotion::Clamp01(RevealAlpha);
		const FVector2f Size(FullSize.X * FMath::Lerp(0.15f, 1.0f, Rev), FullSize.Y);
		const float W = Size.X;
		const float H = Size.Y;

		// 1. 切角主体填充
		TArray<FVector2f> Poly;
		BuildChamferPolygon(Size, Chamfer, Poly);
		FLinearColor Fill = FillColor;
		Fill.A *= Rev;
		DrawConvexFill(OutDrawElements, LayerId, Geo, Poly, Fill);

		// 2. 极淡扫描高光 (SweepAlpha 0..1 之间划过一道锐利斜向微光)
		if (SweepAlpha > 0.01f && SweepAlpha < 0.99f)
		{
			const float BandW = FMath::Clamp(W * 0.16f, 22.0f, 72.0f);
			const float CenterX = FMath::Lerp(-BandW, W + BandW, SweepAlpha);
			const float Skew = H * 0.28f;
			const float X0 = FMath::Clamp(CenterX - BandW * 0.5f, 0.0f, W);
			const float X1 = FMath::Clamp(CenterX + BandW * 0.5f, 0.0f, W);
			const float X2 = FMath::Clamp(CenterX + BandW * 0.5f - Skew, 0.0f, W);
			const float X3 = FMath::Clamp(CenterX - BandW * 0.5f - Skew, 0.0f, W);
			if (FMath::Abs(X1 - X0) > 1.0f || FMath::Abs(X2 - X3) > 1.0f)
			{
				const float Peak = FMath::Sin(SweepAlpha * PI);
				TArray<FVector2f> SweepPoly = {
					FVector2f(X0, 0.0f), FVector2f(X1, 0.0f),
					FVector2f(X2, H),    FVector2f(X3, H)
				};
				DrawConvexFill(OutDrawElements, LayerId + 1, Geo, SweepPoly,
					FLinearColor(1.0f, 1.0f, 1.0f, 0.055f * Peak * Rev));
			}
		}

		// 3. 顶部微高光发丝线
		const FPaintGeometry PaintGeo = Geo.ToPaintGeometry();
		if (bTopSheen && W > 8.0f)
		{
			const float TL = FMath::Clamp(Chamfer.X, 0.0f, FMath::Min(W, H) * 0.45f);
			const float TR = FMath::Clamp(Chamfer.Y, 0.0f, FMath::Min(W, H) * 0.45f);
			TArray<FVector2D> Sheen = { FVector2D(TL + 1.0f, 1.0f), FVector2D(W - TR - 1.0f, 1.0f) };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, PaintGeo, Sheen,
				ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, (0.06f + 0.08f * HoverAlpha) * Rev), false, 1.0f);
		}

		// 4. 锐利 1px 外轮廓
		if (BorderThickness > 0.05f && Poly.Num() >= 3)
		{
			TArray<FVector2D> Outline;
			Outline.Reserve(Poly.Num() + 1);
			for (const FVector2f& P : Poly) { Outline.Add(FVector2D(P.X, P.Y)); }
			const FVector2D FirstPt = Outline[0];
			Outline.Add(FirstPt);

			FLinearColor OutCol = BorderColor;
			OutCol.A = FMath::Clamp(OutCol.A + 0.32f * HoverAlpha, 0.0f, 1.0f) * Rev;
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, PaintGeo, Outline,
				ESlateDrawEffect::None, OutCol, true, BorderThickness);
		}

		// 5. 点睛色边条 (单色或虹彩光谱)
		if (AccentEdge != EERAccentEdge::None && AccentThick > 0.1f)
		{
			const UERUITheme* Theme = ERUI::Theme();
			const float MaxC = FMath::Max(0.0f, FMath::Min(W, H) * 0.45f);
			const float TL = FMath::Clamp(Chamfer.X, 0.0f, MaxC);
			const float TR = FMath::Clamp(Chamfer.Y, 0.0f, MaxC);
			const float BR = FMath::Clamp(Chamfer.Z, 0.0f, MaxC);
			const float BL = FMath::Clamp(Chamfer.W, 0.0f, MaxC);

			FVector2f BarPos(0.0f, 0.0f);
			FVector2f BarSize(0.0f, 0.0f);
			bool bVert = true;

			switch (AccentEdge)
			{
			case EERAccentEdge::Left:
				BarPos = FVector2f(0.0f, TL);
				BarSize = FVector2f(AccentThick, FMath::Max(0.0f, H - TL - BL));
				bVert = true;
				break;
			case EERAccentEdge::Right:
				BarPos = FVector2f(W - AccentThick, TR);
				BarSize = FVector2f(AccentThick, FMath::Max(0.0f, H - TR - BR));
				bVert = true;
				break;
			case EERAccentEdge::Top:
				BarPos = FVector2f(TL, 0.0f);
				BarSize = FVector2f(FMath::Max(0.0f, W - TL - TR), AccentThick);
				bVert = false;
				break;
			case EERAccentEdge::Bottom:
				BarPos = FVector2f(BL, H - AccentThick);
				BarSize = FVector2f(FMath::Max(0.0f, W - BL - BR), AccentThick);
				bVert = false;
				break;
			default:
				break;
			}

			if (BarSize.X > 0.1f && BarSize.Y > 0.1f)
			{
				if (bPrismAccent)
				{
					DrawPrismRect(OutDrawElements, LayerId + 3, Geo, BarPos, BarSize, bVert,
						ERUI::Fade(Theme->PrismStart, Rev),
						ERUI::Fade(Theme->PrismMid, Rev),
						ERUI::Fade(Theme->PrismEnd, Rev));
				}
				else
				{
					const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
					if (White)
					{
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3,
							Geo.ToPaintGeometry(FVector2D(BarSize.X, BarSize.Y), FSlateLayoutTransform(FVector2D(BarPos.X, BarPos.Y))),
							White, ESlateDrawEffect::None, ERUI::Fade(AccentColor, Rev));
					}
				}
			}
		}

		// 6. 四角精密 L 形对准刻度 (自动避开切角处, 只在直角处落位, 切角处用 45° 加强斜杠)
		if (bDrawBrackets && BracketLen > 1.0f)
		{
			const float L = FMath::Min(BracketLen, FMath::Min(W, H) * 0.35f);
			const float MaxC = FMath::Max(0.0f, FMath::Min(W, H) * 0.45f);
			const float TL = FMath::Clamp(Chamfer.X, 0.0f, MaxC);
			const float TR = FMath::Clamp(Chamfer.Y, 0.0f, MaxC);
			const float BR = FMath::Clamp(Chamfer.Z, 0.0f, MaxC);
			const float BL = FMath::Clamp(Chamfer.W, 0.0f, MaxC);

			FLinearColor BCol = BracketColor;
			BCol.A = FMath::Clamp(BCol.A + 0.28f * HoverAlpha, 0.0f, 1.0f) * Rev;

			auto DrawPolyLine = [&](const TArray<FVector2D>& Pts)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 4, PaintGeo, Pts,
					ESlateDrawEffect::None, BCol, true, BracketThick);
			};

			// Top-Left
			if (TL > 0.5f) { DrawPolyLine({ FVector2D(0.0f, TL), FVector2D(TL, 0.0f) }); }
			else           { DrawPolyLine({ FVector2D(0.0f, L), FVector2D(0.0f, 0.0f), FVector2D(L, 0.0f) }); }

			// Top-Right
			if (TR > 0.5f) { DrawPolyLine({ FVector2D(W - TR, 0.0f), FVector2D(W, TR) }); }
			else           { DrawPolyLine({ FVector2D(W - L, 0.0f), FVector2D(W, 0.0f), FVector2D(W, L) }); }

			// Bottom-Right
			if (BR > 0.5f) { DrawPolyLine({ FVector2D(W, H - BR), FVector2D(W - BR, H) }); }
			else           { DrawPolyLine({ FVector2D(W, H - L), FVector2D(W, H), FVector2D(W - L, H) }); }

			// Bottom-Left
			if (BL > 0.5f) { DrawPolyLine({ FVector2D(BL, H), FVector2D(0.0f, H - BL) }); }
			else           { DrawPolyLine({ FVector2D(L, H), FVector2D(0.0f, H), FVector2D(0.0f, H - L) }); }
		}

		return LayerId + 5;
	}
};

TSharedRef<SWidget> UERTechFrame::RebuildWidget()
{
	MyLeaf = SNew(SERTechFrameLeaf);
	return MyLeaf.ToSharedRef();
}

void UERTechFrame::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyLeaf.IsValid()) { return; }
	MyLeaf->Chamfer = FVector4f((float)Chamfer.X, (float)Chamfer.Y, (float)Chamfer.Z, (float)Chamfer.W);
	MyLeaf->FillColor = FillColor;
	MyLeaf->BorderColor = BorderColor;
	MyLeaf->BorderThickness = BorderThickness;
	MyLeaf->bDrawBrackets = bDrawCornerBrackets;
	MyLeaf->BracketLen = BracketLength;
	MyLeaf->BracketThick = BracketThickness;
	MyLeaf->BracketColor = BracketColor;
	MyLeaf->AccentEdge = AccentEdge;
	MyLeaf->bPrismAccent = bUsePrismAccent;
	MyLeaf->AccentThick = AccentThickness;
	MyLeaf->AccentColor = AccentColor;
	MyLeaf->bTopSheen = bTopHairlineSheen;
	MyLeaf->RevealAlpha = RevealAlpha;
	MyLeaf->HoverAlpha = HoverAlpha;
	MyLeaf->SweepAlpha = SweepAlpha;
	MyLeaf->DesiredSize = DesiredFrameSize;
}

void UERTechFrame::SetFrameColors(FLinearColor InFill, FLinearColor InBorder, FLinearColor InBracket, FLinearColor InAccent)
{
	FillColor = InFill;
	BorderColor = InBorder;
	BracketColor = InBracket;
	AccentColor = InAccent;
	SynchronizeProperties();
}

void UERTechFrame::SetMotionState(float InRevealAlpha, float InHoverAlpha, float InSweepAlpha)
{
	RevealAlpha = InRevealAlpha;
	HoverAlpha = InHoverAlpha;
	SweepAlpha = InSweepAlpha;
	if (MyLeaf.IsValid())
	{
		MyLeaf->RevealAlpha = RevealAlpha;
		MyLeaf->HoverAlpha = HoverAlpha;
		MyLeaf->SweepAlpha = SweepAlpha;
	}
}

void UERTechFrame::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyLeaf.Reset();
}

// ============================================================
// 2. SERTechDividerLeaf & UERTechDivider
// ============================================================

class SERTechDividerLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERTechDividerLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	FLinearColor LineColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.16f);
	FLinearColor AnchorColor = FLinearColor(0.92f, 0.92f, 0.90f, 0.88f);
	float AnchorLength = 22.0f;
	float LineThickness = 1.0f;
	bool bDrawEndDiamond = true;
	float RevealAlpha = 1.0f;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(120.0f, 6.0f); }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FVector2D Sz = Geo.GetLocalSize();
		if (!White || Sz.X < 2.0 || RevealAlpha <= 0.005f) { return LayerId; }

		const float Rev = ERMotion::Clamp01(RevealAlpha);
		const float MidY = (float)Sz.Y * 0.5f;
		const float DrawW = (float)Sz.X * Rev;

		// 1px 主发丝线
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geo.ToPaintGeometry(FVector2D(DrawW, LineThickness), FSlateLayoutTransform(FVector2D(0.0, MidY - LineThickness * 0.5f))),
			White, ESlateDrawEffect::None, ERUI::Fade(LineColor, Rev));

		// 左侧实心锚块 (2px 高)
		if (AnchorLength > 0.5f)
		{
			const float AW = FMath::Min(AnchorLength, DrawW);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				Geo.ToPaintGeometry(FVector2D(AW, 2.0f), FSlateLayoutTransform(FVector2D(0.0, MidY - 1.0f))),
				White, ESlateDrawEffect::None, ERUI::Fade(AnchorColor, Rev));
		}

		// 右端 4px 菱形端头
		if (bDrawEndDiamond && DrawW > 12.0f)
		{
			const float EX = DrawW - 3.0f;
			TArray<FVector2D> Diamond = {
				FVector2D(EX, MidY - 2.5f),
				FVector2D(EX + 2.5f, MidY),
				FVector2D(EX, MidY + 2.5f),
				FVector2D(EX - 2.5f, MidY),
				FVector2D(EX, MidY - 2.5f)
			};
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geo.ToPaintGeometry(), Diamond,
				ESlateDrawEffect::None, ERUI::Fade(AnchorColor, 0.55f * Rev), true, 1.0f);
		}

		return LayerId + 2;
	}
};

TSharedRef<SWidget> UERTechDivider::RebuildWidget()
{
	MyLeaf = SNew(SERTechDividerLeaf);
	return MyLeaf.ToSharedRef();
}

void UERTechDivider::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyLeaf.IsValid()) { return; }
	MyLeaf->LineColor = LineColor;
	MyLeaf->AnchorColor = AnchorColor;
	MyLeaf->AnchorLength = AnchorLength;
	MyLeaf->LineThickness = LineThickness;
	MyLeaf->bDrawEndDiamond = bDrawEndDiamond;
	MyLeaf->RevealAlpha = RevealAlpha;
}

void UERTechDivider::SetRevealAlpha(float InAlpha)
{
	RevealAlpha = InAlpha;
	if (MyLeaf.IsValid()) { MyLeaf->RevealAlpha = RevealAlpha; }
}

void UERTechDivider::SetDividerColors(FLinearColor InLine, FLinearColor InAnchor)
{
	LineColor = InLine;
	AnchorColor = InAnchor;
	SynchronizeProperties();
}

void UERTechDivider::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyLeaf.Reset();
}

// ============================================================
// 3. SERPrismBarLeaf & UERPrismBar
// ============================================================

class SERPrismBarLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERPrismBarLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { ForceVolatile(true); }

	bool bVertical = true;
	FVector2D BarSize = FVector2D(3.0f, 44.0f);
	bool bAnimate = true;
	float RevealAlpha = 1.0f;

	virtual FVector2D ComputeDesiredSize(float) const override { return BarSize; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FVector2f Sz = FVector2f(Geo.GetLocalSize());
		if (Sz.X < 0.5f || Sz.Y < 0.5f || RevealAlpha <= 0.005f) { return LayerId; }

		const UERUITheme* T = ERUI::Theme();
		const float Pulse = bAnimate ? ERMotion::SolemnPulse((float)Args.GetCurrentTime(), 1.15f) : 0.5f;
		const float Bright = FMath::Lerp(0.86f, 1.0f, Pulse);

		const FLinearColor C0(T->PrismStart.R * Bright, T->PrismStart.G * Bright, T->PrismStart.B * Bright, RevealAlpha);
		const FLinearColor C1(T->PrismMid.R * Bright, T->PrismMid.G * Bright, T->PrismMid.B * Bright, RevealAlpha);
		const FLinearColor C2(T->PrismEnd.R * Bright, T->PrismEnd.G * Bright, T->PrismEnd.B * Bright, RevealAlpha);

		FVector2f Pos(0.0f, 0.0f);
		FVector2f DrawSz = Sz;
		if (bVertical)
		{
			DrawSz.Y *= RevealAlpha;
		}
		else
		{
			const float W = Sz.X * RevealAlpha;
			Pos.X = (Sz.X - W) * 0.5f;
			DrawSz.X = W;
		}
		DrawPrismRect(OutDrawElements, LayerId, Geo, Pos, DrawSz, bVertical, C0, C1, C2);
		return LayerId + 1;
	}
};

TSharedRef<SWidget> UERPrismBar::RebuildWidget()
{
	MyLeaf = SNew(SERPrismBarLeaf);
	return MyLeaf.ToSharedRef();
}

void UERPrismBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyLeaf.IsValid()) { return; }
	MyLeaf->bVertical = bVertical;
	MyLeaf->BarSize = BarSize;
	MyLeaf->bAnimate = bAnimateShimmer;
	MyLeaf->RevealAlpha = RevealAlpha;
}

void UERPrismBar::SetRevealAlpha(float InAlpha)
{
	RevealAlpha = InAlpha;
	if (MyLeaf.IsValid()) { MyLeaf->RevealAlpha = RevealAlpha; }
}

void UERPrismBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyLeaf.Reset();
}

// ============================================================
// 4. SERCornerReticleLeaf & UERCornerReticle
// ============================================================

class SERCornerReticleLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERCornerReticleLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	float Inset = 22.0f;
	float Arm = 18.0f;
	FLinearColor Color = FLinearColor(0.92f, 0.92f, 0.90f, 0.30f);
	FLinearColor Shadow = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);
	FString TopRightCode = TEXT("ER // 58.01");
	FString BottomRightCode = TEXT("SYS.NOMINAL");
	bool bDrawCenterTicks = true;
	float RevealAlpha = 1.0f;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0f, 1080.0f); }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FVector2D Sz = Geo.GetLocalSize();
		if (Sz.X < 64.0 || Sz.Y < 64.0 || RevealAlpha <= 0.005f) { return LayerId; }

		const float Rev = ERMotion::Clamp01(RevealAlpha);
		const float W = (float)Sz.X;
		const float H = (float)Sz.Y;
		const float M = Inset + (1.0f - Rev) * 14.0f;
		const float A = Arm * Rev;

		const FPaintGeometry PaintGeo = Geo.ToPaintGeometry();
		const FLinearColor MainC = ERUI::Fade(Color, Rev);
		const FLinearColor ShadC = ERUI::Fade(Shadow, Rev);

		auto DrawCorner = [&](const TArray<FVector2D>& Pts)
		{
			if (ShadC.A > 0.01f)
			{
				TArray<FVector2D> Off;
				Off.Reserve(Pts.Num());
				for (const FVector2D& P : Pts) { Off.Add(P + FVector2D(1.0, 1.0)); }
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Off, ESlateDrawEffect::None, ShadC, false, 1.2f);
			}
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, PaintGeo, Pts, ESlateDrawEffect::None, MainC, false, 1.2f);
		};

		// 四角 L 标尺
		DrawCorner({ FVector2D(M, M + A), FVector2D(M, M), FVector2D(M + A, M) });
		DrawCorner({ FVector2D(W - M - A, M), FVector2D(W - M, M), FVector2D(W - M, M + A) });
		DrawCorner({ FVector2D(W - M, H - M - A), FVector2D(W - M, H - M), FVector2D(W - M - A, H - M) });
		DrawCorner({ FVector2D(M + A, H - M), FVector2D(M, H - M), FVector2D(M, H - M - A) });

		// 左右边缘极细水平中线刻度
		if (bDrawCenterTicks)
		{
			const float MidY = H * 0.5f;
			const float Tick = 8.0f * Rev;
			DrawCorner({ FVector2D(M, MidY), FVector2D(M + Tick, MidY) });
			DrawCorner({ FVector2D(W - M - Tick, MidY), FVector2D(W - M, MidY) });
		}

		// 极小 Linux Biolinum 遥测编号 (仅在有文字时)
		if ((!TopRightCode.IsEmpty() || !BottomRightCode.IsEmpty())
			&& FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			FSlateFontInfo MicroFont = ERUI::Theme()->GetFont(EERTypeScale::Micro, true);
			MicroFont.Size = 8;
			MicroFont.LetterSpacing = 220;
			TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

			auto DrawCode = [&](const FString& Str, const FVector2D& Pos)
			{
				const FVector2D TS = Measure->Measure(Str, MicroFont);
				if (ShadC.A > 0.01f)
				{
					FSlateDrawElement::MakeText(OutDrawElements, LayerId,
						Geo.ToPaintGeometry(TS, FSlateLayoutTransform(Pos + FVector2D(1.0, 1.0))),
						Str, MicroFont, ESlateDrawEffect::None, ShadC);
				}
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
					Geo.ToPaintGeometry(TS, FSlateLayoutTransform(Pos)),
					Str, MicroFont, ESlateDrawEffect::None, ERUI::Fade(MainC, 1.15f));
			};

			if (!TopRightCode.IsEmpty())
			{
				const FVector2D TS = Measure->Measure(TopRightCode, MicroFont);
				DrawCode(TopRightCode, FVector2D(W - M - A - 10.0 - TS.X, M - 4.0));
			}
			if (!BottomRightCode.IsEmpty())
			{
				const FVector2D TS = Measure->Measure(BottomRightCode, MicroFont);
				DrawCode(BottomRightCode, FVector2D(W - M - A - 10.0 - TS.X, H - M - TS.Y + 4.0));
			}
		}

		return LayerId + 2;
	}
};

TSharedRef<SWidget> UERCornerReticle::RebuildWidget()
{
	MyLeaf = SNew(SERCornerReticleLeaf);
	return MyLeaf.ToSharedRef();
}

void UERCornerReticle::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyLeaf.IsValid()) { return; }
	MyLeaf->Inset = Inset;
	MyLeaf->Arm = ArmLength;
	MyLeaf->Color = ReticleColor;
	MyLeaf->Shadow = ShadowColor;
	MyLeaf->TopRightCode = TopRightCode;
	MyLeaf->BottomRightCode = BottomRightCode;
	MyLeaf->bDrawCenterTicks = bDrawCenterTicks;
	MyLeaf->RevealAlpha = RevealAlpha;
}

void UERCornerReticle::SetRevealAlpha(float InAlpha)
{
	RevealAlpha = InAlpha;
	if (MyLeaf.IsValid()) { MyLeaf->RevealAlpha = RevealAlpha; }
}

void UERCornerReticle::SetCodes(const FString& InTopRight, const FString& InBottomRight)
{
	TopRightCode = InTopRight;
	BottomRightCode = InBottomRight;
	SynchronizeProperties();
}

void UERCornerReticle::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyLeaf.Reset();
}

// ============================================================
// 5. SERControlSharePieLeaf & UERControlSharePie
// ============================================================

class SERControlSharePieLeaf : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SERControlSharePieLeaf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	TArray<FERControlShareSlice> Slices;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(420.0, 420.0); }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
	{
		const FVector2D Size = Geo.GetLocalSize();
		const FVector2D Center = Size * 0.5;
		const float Side = static_cast<float>(FMath::Min(Size.X, Size.Y));
		if (Side < 80.0f) { return LayerId; }

		float Total = 0.0f;
		int32 LastPositive = INDEX_NONE;
		for (int32 Index = 0; Index < Slices.Num(); ++Index)
		{
			const float Share = Slices[Index].Share;
			if (FMath::IsFinite(Share) && Share > 0.0f)
			{
				Total += Share;
				LastPositive = Index;
			}
		}
		if (LastPositive == INDEX_NONE || !FMath::IsFinite(Total)) { return LayerId; }

		const float Outer = Side * 0.408f;
		const float Inner = Outer * 0.39f;
		const float Start = -0.5f * PI;
		constexpr float FullTurn = 2.0f * PI;
		const auto Point = [Center](float Angle, float Radius)
		{
			return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		};

		const FPaintGeometry PaintGeo = Geo.ToPaintGeometry();

		// 克制的轨道基座与刻度, 不压制主体扇区。
		{
			TArray<FVector2D> Orbit;
			const float OrbitRadius = Outer + Side * 0.035f;
			constexpr int32 OrbitSteps = 72;
			Orbit.Reserve(OrbitSteps + 1);
			for (int32 Step = 0; Step <= OrbitSteps; ++Step)
			{
				Orbit.Add(Point(Start + FullTurn * Step / OrbitSteps, OrbitRadius));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Orbit,
				ESlateDrawEffect::None, FLinearColor(0.10f, 0.10f, 0.10f, 0.18f), true, 1.0f);

			for (int32 Tick = 0; Tick < 48; ++Tick)
			{
				const float Angle = Start + FullTurn * Tick / 48.0f;
				const float InnerTick = Outer + Side * 0.045f;
				const float OuterTick = Outer + Side * ((Tick % 4 == 0) ? 0.062f : 0.052f);
				TArray<FVector2D> Line = { Point(Angle, InnerTick), Point(Angle, OuterTick) };
				const bool bMajor = Tick % 4 == 0;
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Line,
					ESlateDrawEffect::None, FLinearColor(0.10f, 0.10f, 0.10f, bMajor ? 0.20f : 0.10f),
					true, bMajor ? 1.0f : 0.8f);
			}
		}

		struct FPaintedSlice
		{
			int32 SliceIndex = INDEX_NONE;
			float StartAngle = 0.0f;
			float EndAngle = 0.0f;
			FLinearColor Fill = FLinearColor::Black;
		};

		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		TArray<FPaintedSlice> Painted;
		float Angle = Start;
		const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
		for (int32 Index = 0; Index < Slices.Num(); ++Index)
		{
			const FERControlShareSlice& Slice = Slices[Index];
			const float Share = Slice.Share;
			if (!FMath::IsFinite(Share) || Share <= 0.0f)
			{
				continue;
			}

			const float End = Index == LastPositive ? Start + FullTurn : Angle + FullTurn * (Share / Total);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt((End - Angle) * (128.0f / FullTurn)));
			const int32 SliceBase = Verts.Num();
			const FColor VertexColor = Slice.Fill.ToFColor(true);

			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float SampleAngle = FMath::Lerp(Angle, End, static_cast<float>(Step) / Steps);
				const FVector2D OuterPoint = Point(SampleAngle, Outer);
				const FVector2D InnerPoint = Point(SampleAngle, Inner);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT,
					FVector2f(static_cast<float>(OuterPoint.X), static_cast<float>(OuterPoint.Y)),
					FVector2f(0.5f, 0.5f), VertexColor));
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT,
					FVector2f(static_cast<float>(InnerPoint.X), static_cast<float>(InnerPoint.Y)),
					FVector2f(0.5f, 0.5f), VertexColor));
			}
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const int32 Base = SliceBase + Step * 2;
				Indices.Add(static_cast<SlateIndex>(Base));
				Indices.Add(static_cast<SlateIndex>(Base + 2));
				Indices.Add(static_cast<SlateIndex>(Base + 1));
				Indices.Add(static_cast<SlateIndex>(Base + 2));
				Indices.Add(static_cast<SlateIndex>(Base + 3));
				Indices.Add(static_cast<SlateIndex>(Base + 1));
			}

			Painted.Add({ Index, Angle, End, Slice.Fill });
			Angle = End;
		}

		const FSlateResourceHandle WhiteHandle = GetWhiteResourceHandle();
		if (WhiteHandle.IsValid() && Verts.Num() > 0 && Indices.Num() > 0)
		{
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId + 1, WhiteHandle, Verts, Indices, nullptr, 0, 0);
		}

		if (Painted.Num() > 1)
		{
			const FLinearColor SeamColor(0.93f, 0.93f, 0.91f, 0.9f);
			for (const FPaintedSlice& Slice : Painted)
			{
				TArray<FVector2D> Seam = { Point(Slice.StartAngle, Inner), Point(Slice.StartAngle, Outer) };
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, PaintGeo, Seam,
					ESlateDrawEffect::None, SeamColor, true, 1.2f);
			}
		}

		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto DrawCentered = [&](const FString& Text, const FVector2D& TextCenter, const FSlateFontInfo& Font,
				const FLinearColor& Color)
			{
				const FVector2D TextSize = Measure->Measure(Text, Font);
				const FVector2D Pos = TextCenter - FVector2D(TextSize.X * 0.5, TextSize.Y * 0.5);
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3,
					Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(Pos)),
					Text, Font, ESlateDrawEffect::None, Color);
			};

			const FString TotalText = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Total));
			const FSlateFontInfo TotalFont = ERUI::FontBold(FMath::Clamp(FMath::RoundToInt(Side * 0.062f), 12, 26), 0);
			const FSlateFontInfo LabelFont = ERUI::FontRegular(FMath::Clamp(FMath::RoundToInt(Side * 0.035f), 8, 13), 40);
			DrawCentered(TotalText, Center + FVector2D(0.0, -Side * 0.026), TotalFont, ERUI::InkText());
			DrawCentered(TEXT("控制总量"), Center + FVector2D(0.0, Side * 0.025), LabelFont, ERUI::InkText(0.62f));

			if (Side >= 190.0f)
			{
				const FSlateFontInfo WedgeFont = ERUI::FontBold(FMath::Clamp(FMath::RoundToInt(Side * 0.038f), 11, 16), 40);
				const float LabelRadius = FMath::Lerp(Inner, Outer, 0.52f);
				for (const FPaintedSlice& Slice : Painted)
				{
					if (Slice.EndAngle - Slice.StartAngle < 0.27f)
					{
						continue;
					}
					const FVector2D LabelPos = Point((Slice.StartAngle + Slice.EndAngle) * 0.5f, LabelRadius);
					const FLinearColor LabelColor = Slice.Fill.GetLuminance() > 0.24f ? ERUI::InkText() : FLinearColor::White;
					DrawCentered(FString::Printf(TEXT("%02d"), Slice.SliceIndex + 1), LabelPos, WedgeFont, LabelColor);
				}
			}
		}

		return LayerId + 4;
	}
};

TSharedRef<SWidget> UERControlSharePie::RebuildWidget()
{
	MyLeaf = SNew(SERControlSharePieLeaf);
	MyLeaf->Slices = Slices;
	return MyLeaf.ToSharedRef();
}

void UERControlSharePie::SetSlices(TArray<FERControlShareSlice> InSlices)
{
	Slices = MoveTemp(InSlices);
	if (MyLeaf.IsValid())
	{
		MyLeaf->Slices = Slices;
		MyLeaf->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UERControlSharePie::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyLeaf.Reset();
}
