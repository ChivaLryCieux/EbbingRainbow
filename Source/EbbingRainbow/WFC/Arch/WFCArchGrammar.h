// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "WFCArchTypes.h"

/**
 * Space grammar for the 19x19 two-level "cross atrium" arena.
 *
 * The grammar owns the global composition (rooms, doors, windows, columns,
 * mezzanine, skylight); socket resolution only chooses the kit tile that
 * matches each cell boundary.  This keeps socket validity at 100% while the
 * building stays legible at the macro scale.
 */
class FWFCArchGrammar
{
public:
	static bool Build(int32 Seed, FArchPlan& OutPlan);

	/** Resolve one wall cell to a kit tile id. Returns the pattern actually used. */
	static bool ResolveWallTile(
		uint8 WallMask,
		int32 SpecialSide,
		TCHAR SpecialCode,
		FRandomStream& Rng,
		FName& OutTile,
		FString& OutPattern,
		bool bAllowColumn = false);

	static bool HasWallPattern(const FString& Pattern);
};
