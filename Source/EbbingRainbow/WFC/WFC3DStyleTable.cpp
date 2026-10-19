// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DStyleTable.h"

TSubclassOf<AWFC3DArenaGeneratorBase> UWFC3DStyleTable::Resolve(EWFCBuildingStyle InStyle) const
{
	for (const FWFCStyleEntry& Entry : Styles)
	{
		if (Entry.Style != InStyle)
		{
			continue;
		}

		UClass* LoadedClass = Entry.GeneratorClass.LoadSynchronous();
		if (!LoadedClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WFC] StyleTable: style %d has no valid generator class (soft path '%s')."),
				static_cast<int32>(InStyle), *Entry.GeneratorClass.ToString());
			return nullptr;
		}

		if (!LoadedClass->IsChildOf(AWFC3DArenaGeneratorBase::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[WFC] StyleTable: class '%s' for style %d is not an arena generator."),
				*LoadedClass->GetName(), static_cast<int32>(InStyle));
			return nullptr;
		}

		return LoadedClass;
	}

	return nullptr;
}

FIntVector UWFC3DStyleTable::GetDefaultScale(EWFCBuildingStyle InStyle) const
{
	for (const FWFCStyleEntry& Entry : Styles)
	{
		if (Entry.Style == InStyle)
		{
			return Entry.DefaultScale;
		}
	}
	return FIntVector(14, 14, 1);
}
