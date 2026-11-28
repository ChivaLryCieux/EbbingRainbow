// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERUIStatics.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "UI/Primitives/ERTechPrimitives.h"

namespace ERUI
{
	const UERUITheme* Theme()
	{
		return UERUITheme::Get();
	}

	FSlateFontInfo Font(EERTypeScale Scale, bool bLatin)
	{
		return Theme()->GetFont(Scale, bLatin);
	}

	FSlateFontInfo FontBold(int32 Size, int32 Tracking)
	{
		return Theme()->GetFontExplicit(Size, true, Tracking);
	}

	FSlateFontInfo FontRegular(int32 Size, int32 Tracking)
	{
		return Theme()->GetFontExplicit(Size, false, Tracking);
	}

	static UTexture2D* MakeTransientTexture(int32 W, int32 H, TFunctionRef<FColor(int32, int32)> PixelFn)
	{
		UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->Filter = TF_Bilinear;
#if WITH_EDITORONLY_DATA
		Tex->MipGenSettings = TMGS_NoMipmaps;
#endif

		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		void* Raw = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FColor* Pixels = static_cast<FColor*>(Raw);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				Pixels[Y * W + X] = PixelFn(X, Y);
			}
		}
		Mip.BulkData.Unlock();
		Tex->UpdateResource();
		Tex->AddToRoot();
		return Tex;
	}

	UTexture2D* GetWhiteTexture()
	{
		if (UTexture2D* Asset = Theme()->WhiteTextureAsset)
		{
			return Asset;
		}
		static UTexture2D* T = nullptr;
		if (!T)
		{
			T = MakeTransientTexture(4, 4, [](int32, int32) { return FColor::White; });
		}
		return T;
	}

	UTexture2D* GetRingTexture()
	{
		if (UTexture2D* Asset = Theme()->RingTextureAsset)
		{
			return Asset;
		}
		static UTexture2D* T = nullptr;
		if (!T)
		{
			T = MakeTransientTexture(9, 9, [](int32 X, int32 Y)
			{
				const bool bBorder = (X == 0 || Y == 0 || X == 8 || Y == 8);
				return bBorder ? FColor::White : FColor(255, 255, 255, 0);
			});
		}
		return T;
	}

	UTexture2D* GetScrimTexture()
	{
		if (UTexture2D* Asset = Theme()->ScrimTextureAsset)
		{
			return Asset;
		}
		static UTexture2D* T = nullptr;
		if (!T)
		{
			T = MakeTransientTexture(4, 64, [](int32, int32 Y)
			{
				// 顶部不透明 -> 底部全透, 带轻微 ease
				const float T01 = 1.0f - (float)Y / 63.0f;
				const uint8 A = (uint8)FMath::Clamp(FMath::RoundToInt(255.0f * T01 * T01), 0, 255);
				return FColor(255, 255, 255, A);
			});
		}
		return T;
	}

	FSlateBrush TransparentBrush()
	{
		FSlateBrush B;
		B.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
		return B;
	}

	FSlateBrush HairlineBoxBrush(FLinearColor Tint)
	{
		FSlateBoxBrush B(GetRingTexture(), 2.0f / 9.0f);
		B.TintColor = FSlateColor(Tint);
		return B;
	}

	FSlateBrush WhiteBrush()
	{
		FSlateBrush B;
		B.SetResourceObject(GetWhiteTexture());
		B.TintColor = FSlateColor(FLinearColor::White);
		return B;
	}

	FSlateBrush WhiteBrush(FVector2D Size)
	{
		FSlateBrush B = WhiteBrush();
		B.ImageSize = Size;
		return B;
	}

	FSlateBrush ScrimBrush()
	{
		FSlateBrush B;
		B.SetResourceObject(GetScrimTexture());
		B.TintColor = FSlateColor(FLinearColor::White);
		return B;
	}

	FSlateBrush RoundedBrush(float Radius, FLinearColor Fill, float OutlineWidth, FLinearColor Outline)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		return Brush;
	}

	UImage* MakeLogoImage(UUserWidget* Owner, UTexture2D* Tex, float Height)
	{
		if (!Owner || !Owner->WidgetTree || !Tex) { return nullptr; }
		UImage* Img = Owner->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const float TexW = FMath::Max(1.0f, (float)Tex->GetSizeX());
		const float TexH = FMath::Max(1.0f, (float)Tex->GetSizeY());
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.ImageSize = FVector2D(Height * TexW / TexH, Height);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Img->SetBrush(Brush);
		return Img;
	}

	UTextBlock* MakeText(UUserWidget* Owner, const FText& Text, EERTypeScale Scale,
		FLinearColor Color, bool bLatin)
	{
		UTextBlock* TB = Owner->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TB->SetFont(Font(Scale, bLatin));
		TB->SetColorAndOpacity(FSlateColor(Color));
		TB->SetText(Text);
		TB->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return TB;
	}

	FLinearColor InkText(float Alpha)
	{
		return Fade(Theme()->PureInk, Alpha);
	}

	void StyleBoneFrame(UERTechFrame* Frame, float Hover01, bool bEnabled, bool bEmphasis,
		const FLinearColor& Accent, bool bPrism)
	{
		if (!Frame) { return; }
		const float H = FMath::Clamp(Hover01, 0.0f, 1.0f);
		if (!bEnabled)
		{
			Frame->SetFrameColors(
				FLinearColor(0.66f, 0.66f, 0.64f, 0.82f),
				FLinearColor(0.02f, 0.02f, 0.03f, 0.22f),
				FLinearColor(0.01f, 0.01f, 0.02f, 0.30f),
				Fade(Accent, 0.45f));
			Frame->bUsePrismAccent = false;
			Frame->SetMotionState(1.0f, 0.0f, 1.0f);
			return;
		}
		const FLinearColor Fill = FMath::Lerp(
			FLinearColor(0.93f, 0.93f, 0.91f, 0.95f),
			FLinearColor(1.00f, 1.00f, 0.98f, 1.00f), H);
		const float BorderA = bEmphasis ? 0.92f : (0.35f + 0.45f * H);
		const FLinearColor Border(0.02f, 0.02f, 0.03f, BorderA);
		const FLinearColor Bracket(0.01f, 0.01f, 0.02f, bEmphasis ? 1.0f : 0.72f + 0.2f * H);
		Frame->SetFrameColors(Fill, Border, Bracket, Accent);
		Frame->bUsePrismAccent = bPrism;
		Frame->SetMotionState(1.0f, H, 1.0f);
	}

	FBoneCard MakeBoneCard(UUserWidget* Owner, const FLinearColor& Accent, bool bPrism,
		const FMargin& Padding, bool bDrawBrackets)
	{
		FBoneCard Card;
		UWidgetTree* Tree = Owner->WidgetTree;

		Card.Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Card.Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		Card.Frame = Tree->ConstructWidget<UERTechFrame>(UERTechFrame::StaticClass());
		Card.Frame->Chamfer = FVector4(0.0f, 12.0f, 0.0f, 12.0f);
		Card.Frame->BorderThickness = 1.0f;
		Card.Frame->bDrawCornerBrackets = bDrawBrackets;
		Card.Frame->BracketLength = 8.0f;
		Card.Frame->BracketThickness = 1.4f;
		Card.Frame->AccentEdge = EERAccentEdge::Left;
		Card.Frame->AccentThickness = 4.0f;
		Card.Frame->AccentColor = Accent;
		Card.Frame->bUsePrismAccent = bPrism;
		Card.Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		StyleBoneFrame(Card.Frame, 0.0f, true, false, Accent, bPrism);
		{
			UOverlaySlot* S = Card.Root->AddChildToOverlay(Card.Frame);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		Card.Content = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card.Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		{
			UOverlaySlot* S = Card.Root->AddChildToOverlay(Card.Content);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(Padding.Left + 6.0f, Padding.Top, Padding.Right, Padding.Bottom));
		}
		return Card;
	}

	UWidget* MakeSectionBar(UUserWidget* Owner, const FText& Title, int32 FontSize,
		const FLinearColor& Accent, bool bPrism)
	{
		FBoneCard Bar = MakeBoneCard(Owner, Accent, bPrism, FMargin(18.0f, 8.0f), false);
		Bar.Frame->AccentThickness = 4.0f;
		UTextBlock* TB = MakeText(Owner, Title, EERTypeScale::Title, InkText(), false);
		TB->SetFont(FontBold(FontSize, 80));
		Bar.Content->AddChildToVerticalBox(TB);
		return Bar.Root;
	}

	void ApplyResonantText(UTextBlock* TB, int32 FontSize, bool bBold, int32 Tracking,
		FLinearColor Color, FVector2D ShadowOffset, float ShadowAlpha)
	{
		if (!TB) { return; }
		TB->SetFont(bBold ? FontBold(FontSize, Tracking) : FontRegular(FontSize, Tracking));
		TB->SetColorAndOpacity(FSlateColor(Color));

		FVector2D ResolvedOffset = ShadowOffset;
		if (ResolvedOffset.X < 0.0f)
		{
			ResolvedOffset.X = FMath::Clamp(FontSize * 0.042f, 1.5f, 5.5f);
			ResolvedOffset.Y = FMath::Clamp(FontSize * 0.058f, 2.0f, 7.5f);
		}
		TB->SetShadowOffset(ResolvedOffset);
		TB->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, FMath::Clamp(ShadowAlpha, 0.0f, 1.0f)));
	}
}
