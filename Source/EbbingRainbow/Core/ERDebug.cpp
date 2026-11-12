// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERDebug.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ER::Debug
{
	/** 整条命令行只解析一次 (原实现每处调用都重新解析一遍) */
	struct FCmdlineFlags
	{
		int32 ArenaSeed = 0;
		int32 ArenaGenerator = -1;
		bool bAtmo = false;
		bool bArenaForceReady = false;
		bool bAutoShot = false;
		bool bAutoShotTop = false;
		bool bShowNav = false;
		bool bRouteShot = false;
		bool bNatureCube = false;
		bool bNatureNavBox = false;
		bool bNatureFlat = false;
		bool bHallShowcase = false;
		bool bArchDump = false;

		FCmdlineFlags()
		{
			const TCHAR* Cmd = FCommandLine::Get();
			if (!Cmd)
			{
				return;
			}
			FParse::Value(Cmd, TEXT("ArenaSeed="), ArenaSeed);
			FParse::Value(Cmd, TEXT("ArenaGenerator="), ArenaGenerator);
			bAtmo          = FParse::Param(Cmd, TEXT("Atmo"));
			bArenaForceReady = FParse::Param(Cmd, TEXT("ArenaForceReady"));
			bAutoShot      = FParse::Param(Cmd, TEXT("AutoShot"));
			bAutoShotTop   = FParse::Param(Cmd, TEXT("AutoShotTop"));
			bShowNav       = FParse::Param(Cmd, TEXT("ShowNav"));
			bRouteShot     = FParse::Param(Cmd, TEXT("RouteShot"));
			bNatureCube    = FParse::Param(Cmd, TEXT("NatureCube"));
			bNatureNavBox  = FParse::Param(Cmd, TEXT("NatureNavBox"));
			bNatureFlat    = FParse::Param(Cmd, TEXT("NatureFlat"));
			bHallShowcase  = FParse::Param(Cmd, TEXT("HallShowcase"));
			bArchDump      = FParse::Param(Cmd, TEXT("ArchDump"));
		}
	};

	static const FCmdlineFlags& Flags()
	{
		static const FCmdlineFlags Cached;
		return Cached;
	}

	bool IsAtmoEnabled()           { return Flags().bAtmo; }
	int32 GetArenaSeed()           { return Flags().ArenaSeed; }
	int32 GetArenaGeneratorOverride() { return Flags().ArenaGenerator; }
	bool IsArenaForceReady()       { return Flags().bArenaForceReady; }
	bool IsAutoShot()              { return Flags().bAutoShot; }
	bool IsAutoShotTop()           { return Flags().bAutoShotTop; }
	bool IsShowNav()               { return Flags().bShowNav; }
	bool IsRouteShot()             { return Flags().bRouteShot; }
	bool IsNatureCube()            { return Flags().bNatureCube; }
	bool IsNatureNavBox()          { return Flags().bNatureNavBox; }
	bool IsNatureFlat()            { return Flags().bNatureFlat; }
	bool IsHallShowcase()          { return Flags().bHallShowcase; }
	bool IsArchDump()              { return Flags().bArchDump; }
}
