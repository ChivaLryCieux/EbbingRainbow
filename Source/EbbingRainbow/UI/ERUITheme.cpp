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
	auto AppendFace = [](FTypeface& Typeface, const UFontFace* Face)
	{
		if (!Face)
		{
			return;
		}
		FTypefaceEntry& Entry = Typeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TEXT("Regular");
		Entry.Font = FFontData(Face);
	};

	// 默认 typeface = Inter(拉丁/数字), Fallback = Noto(中文自动回落, 跳过字形检查)。
	// 只配了一个面时两边都指向它, 保证总有可用的主字体。
	const UFontFace* Primary = FontLatin ? FontLatin.Get() : FontBody.Get();
	const UFontFace* Secondary = FontBody ? FontBody.Get() : FontLatin.Get();
	AppendFace(CompositeFont.DefaultTypeface, Primary);
	AppendFace(CompositeFont.FallbackTypeface.Typeface, Secondary);

	RuntimeFont = Font;
	return RuntimeFont;
}

FSlateFontInfo UERUITheme::GetFont(EERTypeScale Scale, bool bLatin) const
{
	const UObject* FontObj = GetRuntimeFont();

	int32 Size = BodySize;
	int32 Tracking = 0;
	switch (Scale)
	{
	case EERTypeScale::Micro:    Size = MicroSize;    Tracking = MicroTracking; break;
	case EERTypeScale::Caption:  Size = CaptionSize;  Tracking = MicroTracking / 2; break;
	case EERTypeScale::Body:     Size = BodySize;     break;
	case EERTypeScale::Title:    Size = TitleSize;    break;
	case EERTypeScale::Display:  Size = DisplaySize;  break;
	case EERTypeScale::Hero:     Size = HeroSize;     break;
	}

	FSlateFontInfo Info(FontObj, Size);
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
