// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ERUITheme.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"

const UFont* UERUITheme::GetRuntimeFont() const
{
	if (CompositeFontAsset)
	{
		return CompositeFontAsset;
	}
	if (RuntimeFont)
	{
		return RuntimeFont;
	}

	UFont* Font = NewObject<UFont>(GetTransientPackage(), NAME_None, RF_Transient);
	Font->FontCacheType = EFontCacheType::Runtime;

	FCompositeFont& CompositeFont = Font->GetMutableInternalCompositeFont();
	auto AppendFace = [](FTypeface& Typeface, FName StyleName, const UFontFace* Face)
	{
		if (!Face)
		{
			return;
		}
		FTypefaceEntry& Entry = Typeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = StyleName;
		Entry.Font = FFontData(Face);
	};

	// 默认 typeface = Linux Biolinum (拉丁/数字, Regular & Bold 均走 Biolinum),
	// Fallback = Noto Sans SC (Regular + Bold 双字重回落)。
	const UFontFace* Primary = FontLatin ? FontLatin.Get() : FontBody.Get();
	const UFontFace* CjkReg = FontBody ? FontBody.Get() : Primary;
	const UFontFace* CjkBold = FontBodyBold ? FontBodyBold.Get() : CjkReg;

	AppendFace(CompositeFont.DefaultTypeface, FName(TEXT("Regular")), Primary);
	AppendFace(CompositeFont.DefaultTypeface, FName(TEXT("Bold")), Primary);
	AppendFace(CompositeFont.FallbackTypeface.Typeface, FName(TEXT("Regular")), CjkReg);
	AppendFace(CompositeFont.FallbackTypeface.Typeface, FName(TEXT("Bold")), CjkBold);

	RuntimeFont = Font;
	return RuntimeFont;
}

FSlateFontInfo UERUITheme::GetFont(EERTypeScale Scale, bool bLatin) const
{
	int32 Size = BodySize;
	int32 Tracking = 0;
	bool bBold = false;
	switch (Scale)
	{
	case EERTypeScale::Micro:    Size = MicroSize;    Tracking = MicroTracking;     bBold = false; break;
	case EERTypeScale::Caption:  Size = CaptionSize;  Tracking = MicroTracking / 2; bBold = false; break;
	case EERTypeScale::Body:     Size = BodySize;     Tracking = 20;                bBold = false; break;
	case EERTypeScale::Title:    Size = TitleSize;    Tracking = 40;                bBold = true;  break;
	case EERTypeScale::Display:  Size = DisplaySize;  Tracking = -10;               bBold = true;  break;
	case EERTypeScale::Hero:     Size = HeroSize;     Tracking = 80;                bBold = true;  break;
	}
	return GetFontExplicit(Size, bBold, Tracking);
}

FSlateFontInfo UERUITheme::GetFontExplicit(int32 Size, bool bBold, int32 Tracking) const
{
	const UObject* FontObj = GetRuntimeFont();
	FSlateFontInfo Info(FontObj, Size, bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	Info.LetterSpacing = Tracking;
	return Info;
}

const UERUITheme* UERUITheme::Get()
{
	static TWeakObjectPtr<UERUITheme> Cached;
	if (const UERUITheme* T = Cached.Get())
	{
		return T;
	}

	if (const UERUISettings* Settings = UERUISettings::Get())
	{
		if (!Settings->Theme.IsNull())
		{
			if (UERUITheme* T = Settings->Theme.LoadSynchronous())
			{
				Cached = T;
				return T;
			}
		}
	}
	return GetDefault<UERUITheme>();
}
