// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERUIStatics.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

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

	static UTexture2D* MakeTransientTexture(int32 W, int32 H, TFunctionRef<FColor(int32, int32)> PixelFn)
	{
		UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->Filter = TF_Bilinear;
		Tex->MipGenSettings = TMGS_NoMipmaps;

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
		static UTexture2D* T = nullptr;
		if (!T)
		{
			T = MakeTransientTexture(4, 4, [](int32, int32) { return FColor::White; });
		}
		return T;
	}

	UTexture2D* GetRingTexture()
	{
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
}
