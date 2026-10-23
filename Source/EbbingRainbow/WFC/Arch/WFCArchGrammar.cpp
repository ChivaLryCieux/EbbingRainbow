// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFCArchGrammar.h"
#include "WFCKitTileTable.h"

namespace
{
	constexpr int32 GSize = 19;
	constexpr int32 SideN = 0;
	constexpr int32 SideE = 1;
	constexpr int32 SideS = 2;
	constexpr int32 SideW = 3;

	const int32 DX[4] = { 0, 1, 0, -1 };
	const int32 DY[4] = { 1, 0, -1, 0 };
	const int32 Opp[4] = { 2, 3, 0, 1 };

	const TCHAR* ThemeNames[] =
	{
		TEXT("chapel"), TEXT("storage"), TEXT("control"), TEXT("barracks")
	};

	struct FCornerDef
	{
		FArchRect Rect;
	};

	const FCornerDef Corners[4] =
	{
		{ FArchRect(0, 0, 4, 4) },
		{ FArchRect(14, 0, 18, 4) },
		{ FArchRect(0, 14, 4, 18) },
		{ FArchRect(14, 14, 18, 18) }
	};

	struct FHallDef
	{
		FArchRect Rect;
		EArchCellKind Kind;
	};

	const FHallDef HallRects[] =
	{
		{ FArchRect(7, 0, 11, 4), EArchCellKind::Nave },
		{ FArchRect(7, 14, 11, 18), EArchCellKind::Apse },
		{ FArchRect(0, 7, 4, 11), EArchCellKind::Transept },
		{ FArchRect(14, 7, 18, 11), EArchCellKind::Transept },
		{ FArchRect(5, 0, 6, 4), EArchCellKind::Aisle },
		{ FArchRect(12, 0, 13, 4), EArchCellKind::Aisle },
		{ FArchRect(5, 14, 6, 18), EArchCellKind::Aisle },
		{ FArchRect(12, 14, 13, 18), EArchCellKind::Aisle },
		{ FArchRect(0, 5, 4, 6), EArchCellKind::Aisle },
		{ FArchRect(0, 12, 4, 13), EArchCellKind::Aisle },
		{ FArchRect(14, 5, 18, 6), EArchCellKind::Aisle },
		{ FArchRect(14, 12, 18, 13), EArchCellKind::Aisle }
	};

	const FIntPoint ColumnCells[] =
	{
		FIntPoint(5, 5), FIntPoint(7, 5), FIntPoint(11, 5), FIntPoint(13, 5),
		FIntPoint(5, 7), FIntPoint(13, 7),
		FIntPoint(5, 11), FIntPoint(13, 11),
		FIntPoint(5, 13), FIntPoint(7, 13), FIntPoint(11, 13), FIntPoint(13, 13),
		FIntPoint(6, 1), FIntPoint(6, 2), FIntPoint(6, 3),
		FIntPoint(12, 1), FIntPoint(12, 2), FIntPoint(12, 3),
		FIntPoint(6, 15), FIntPoint(6, 16), FIntPoint(6, 17),
		FIntPoint(12, 15), FIntPoint(12, 16), FIntPoint(12, 17),
		FIntPoint(1, 6), FIntPoint(2, 6), FIntPoint(3, 6),
		FIntPoint(1, 12), FIntPoint(2, 12), FIntPoint(3, 12),
		FIntPoint(15, 6), FIntPoint(16, 6), FIntPoint(17, 6),
		FIntPoint(15, 12), FIntPoint(16, 12), FIntPoint(17, 12)
	};

	struct FCornerPlan
	{
		bool bMerge = false;
		int32 DoorSide = SideN;
		int32 Axis = 0;
	};

	bool IsVoidKind(EArchCellKind K)
	{
		return K == EArchCellKind::Atrium || K == EArchCellKind::Nave
			|| K == EArchCellKind::Apse || K == EArchCellKind::Transept;
	}

	bool HasAnySpecial(const FArchCell& C)
	{
		return C.Special[0] || C.Special[1] || C.Special[2] || C.Special[3];
	}

	const TMap<FString, TArray<const FWFCKitWallEntry*>>& WallIndex()
	{
		static TMap<FString, TArray<const FWFCKitWallEntry*>> Index;
		static bool bBuilt = false;
		if (!bBuilt)
		{
			for (const FWFCKitWallEntry& Entry : GWFCKitWallEntries)
			{
				Index.FindOrAdd(FString(Entry.Pattern)).Add(&Entry);
			}
			bBuilt = true;
		}
		return Index;
	}

	FString BuildPattern(uint8 Mask, int32 SpecialSide, TCHAR SpecialCode)
	{
		TCHAR Pat[5] = { 0, 0, 0, 0, 0 };
		for (int32 Side = 0; Side < 4; ++Side)
		{
			if (Side == SpecialSide && SpecialCode != 0)
			{
				Pat[Side] = SpecialCode;
			}
			else
			{
				Pat[Side] = (Mask & (1 << Side)) ? TEXT('w') : TEXT('o');
			}
		}
		return FString(Pat);
	}

	bool FindWallTile(const FString& Pattern, FRandomStream& Rng, FName& OutTile)
	{
		const TArray<const FWFCKitWallEntry*>* List = WallIndex().Find(Pattern);
		if (!List || List->Num() == 0)
		{
			return false;
		}

		TArray<const FWFCKitWallEntry*> Pool;
		TArray<const FWFCKitWallEntry*> Base;
		for (const FWFCKitWallEntry* Entry : *List)
		{
			const FString Id(Entry->Id);
			if (Id.Contains(TEXT("_column")) || Id.Contains(TEXT("_stair_")))
			{
				continue;
			}
			Pool.Add(Entry);
			if (!Id.EndsWith(TEXT("_b")))
			{
				Base.Add(Entry);
			}
		}
		if (Pool.Num() == 0)
		{
			return false;
		}

		const TArray<const FWFCKitWallEntry*>& Use =
			(Base.Num() > 0 && Rng.FRand() < 0.72f) ? Base : Pool;

		float Total = 0.0f;
		for (const FWFCKitWallEntry* Entry : Use)
		{
			Total += FMath::Max(0.01f, Entry->Weight);
		}
		float Pick = Rng.FRandRange(0.0f, Total);
		for (const FWFCKitWallEntry* Entry : Use)
		{
			Pick -= FMath::Max(0.01f, Entry->Weight);
			if (Pick <= 0.0f || Entry == Use.Last())
			{
				OutTile = FName(Entry->Id);
				return true;
			}
		}
		return false;
	}

	void GetHallSides(const FArchRect& Rect, TArray<int32>& Out)
	{
		if (Rect.X0 == 0) { Out.Add(SideE); }
		if (Rect.X1 == GSize - 1) { Out.Add(SideW); }
		if (Rect.Y0 == 0) { Out.Add(SideN); }
		if (Rect.Y1 == GSize - 1) { Out.Add(SideS); }
	}

	int32 MirrorSide(int32 Side)
	{
		if (Side == SideE) { return SideW; }
		if (Side == SideW) { return SideE; }
		return Side;
	}

	void EdgeDoor(const FArchRect& R, int32 Side, int32& OutX, int32& OutY)
	{
		switch (Side)
		{
		case SideN: OutX = (R.X0 + R.X1) / 2; OutY = R.Y1; break;
		case SideE: OutX = R.X1; OutY = (R.Y0 + R.Y1) / 2; break;
		case SideS: OutX = (R.X0 + R.X1) / 2; OutY = R.Y0; break;
		default:    OutX = R.X0; OutY = (R.Y0 + R.Y1) / 2; break;
		}
	}

	void AddRoom(FArchPlan& Plan, const FArchRect& Rect, int32 DoorX, int32 DoorY,
		int32 DoorSide, const TCHAR* Size, int32 RoomId, FRandomStream& Rng)
	{
		FArchRoom Room;
		Room.Id = RoomId;
		Room.Rect = Rect;
		Room.DoorX = DoorX;
		Room.DoorY = DoorY;
		Room.DoorSide = DoorSide;
		Room.Theme = FName(ThemeNames[Rng.RandRange(0, 3)]);
		Room.Size = FName(Size);
		Plan.Rooms.Add(Room);

		for (int32 Y = Rect.Y0; Y <= Rect.Y1; ++Y)
		{
			for (int32 X = Rect.X0; X <= Rect.X1; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				C.Kind = EArchCellKind::Room;
				C.Space = RoomId;
			}
		}
	}

	bool IsCleanDoorCell(const FArchPlan& Plan, int32 X, int32 Y, int32 Side)
	{
		if (!Plan.IsValid(X, Y))
		{
			return false;
		}
		const FArchCell& C = Plan.Cell(X, Y);
		if (HasAnySpecial(C) || C.WallMask != (1 << Side))
		{
			return false;
		}
		const int32 NX = X + DX[Side];
		const int32 NY = Y + DY[Side];
		if (!Plan.IsValid(NX, NY))
		{
			return false;
		}
		return !HasAnySpecial(Plan.Cell(NX, NY));
	}

	bool FindCleanDoorCell(const FArchPlan& Plan, const FArchRoom& Room, int32& OutX, int32& OutY, int32& OutSide)
	{
		for (int32 Y = Room.Rect.Y0; Y <= Room.Rect.Y1; ++Y)
		{
			for (int32 X = Room.Rect.X0; X <= Room.Rect.X1; ++X)
			{
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if (IsCleanDoorCell(Plan, X, Y, Side))
					{
						const int32 NX = X + DX[Side];
						const int32 NY = Y + DY[Side];
						if (Plan.IsValid(NX, NY) && Plan.Cell(NX, NY).Space != Room.Id)
						{
							OutX = X; OutY = Y; OutSide = Side;
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	bool SetOpening(FArchPlan& Plan, int32 X, int32 Y, int32 Side, TCHAR Code)
	{
		if (!Plan.IsValid(X, Y))
		{
			return false;
		}

		const int32 NX = X + DX[Side];
		const int32 NY = Y + DY[Side];
		FArchCell& C = Plan.Cell(X, Y);
		if (C.Special[Side] != 0)
		{
			return false;
		}
		if (Plan.IsValid(NX, NY) && Plan.Cell(NX, NY).Special[Opp[Side]] != 0)
		{
			return false;
		}

		C.WallMask &= ~(1 << Side);
		C.Special[Side] = Code;
		if (Plan.IsValid(NX, NY))
		{
			FArchCell& N = Plan.Cell(NX, NY);
			N.WallMask &= ~(1 << Opp[Side]);
			N.Special[Opp[Side]] = Code;
		}
		return true;
	}

	bool IsPerimeterSide(const FArchPlan& Plan, int32 X, int32 Y, int32 Side)
	{
		return !Plan.IsValid(X + DX[Side], Y + DY[Side]);
	}

	uint32 HashCell(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = static_cast<uint32>(X * 374761393 + Y * 668265263 + Seed * 2246822519);
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}

	void DressFloor(FArchPlan& Plan)
	{
		for (int32 Y = 0; Y < Plan.H; ++Y)
		{
			for (int32 X = 0; X < Plan.W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				const uint32 H = HashCell(X, Y, Plan.Seed);

				if (X == 9 && Y == 9)
				{
					C.FloorTile = FName(TEXT("floor_tech_b"));
					continue;
				}
				if ((X == 9 && (Y <= 4 || Y >= 14)) || (Y == 9 && (X <= 4 || X >= 14)))
				{
					C.FloorTile = FName(TEXT("floor_hazard"));
					continue;
				}
				if (HasAnySpecial(C))
				{
					C.FloorTile = FName(TEXT("floor_plate"));
					continue;
				}

				if (C.Kind == EArchCellKind::Room)
				{
					switch (H % 4)
					{
					case 0: C.FloorTile = FName(TEXT("floor_plain")); break;
					case 1: C.FloorTile = FName(TEXT("floor_plain_b")); break;
					case 2: C.FloorTile = FName(TEXT("floor_plain_c")); break;
					default: C.FloorTile = FName(TEXT("floor_plate")); break;
					}
				}
				else if (C.Kind == EArchCellKind::Aisle)
				{
					switch (H % 5)
					{
					case 0: C.FloorTile = FName(TEXT("floor_plate")); break;
					case 1: C.FloorTile = FName(TEXT("floor_plate_b")); break;
					case 2: C.FloorTile = FName(TEXT("floor_plain")); break;
					case 3: C.FloorTile = FName(TEXT("floor_plain_b")); break;
					default: C.FloorTile = FName(TEXT("floor_plate")); break;
					}
				}
				else
				{
					switch (H % 5)
					{
					case 0: C.FloorTile = FName(TEXT("floor_plate")); break;
					case 1: C.FloorTile = FName(TEXT("floor_plate_b")); break;
					case 2: C.FloorTile = FName(TEXT("floor_plain")); break;
					case 3: C.FloorTile = FName(TEXT("floor_plain_b")); break;
					default: C.FloorTile = FName(TEXT("floor_plate")); break;
					}
				}
			}
		}
	}

	void DressCeiling(FArchPlan& Plan)
	{
		for (int32 Y = 0; Y < Plan.H; ++Y)
		{
			for (int32 X = 0; X < Plan.W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				if (IsVoidKind(C.Kind))
				{
					continue;
				}
				const uint32 H = HashCell(X, Y, Plan.Seed + 17);
				const bool bArm = C.Kind == EArchCellKind::Nave
					|| C.Kind == EArchCellKind::Apse || C.Kind == EArchCellKind::Transept;
				if (bArm && (X % 3 == 0 && Y % 3 == 0))
				{
					C.CeilTile = FName(TEXT("ceil_light"));
				}
				else if (H % 100 < 7)
				{
					C.CeilTile = FName(TEXT("ceil_vent"));
				}
				else if (C.Kind == EArchCellKind::Room && H % 100 < 18)
				{
					C.CeilTile = (H % 100 < 10) ? FName(TEXT("ceil_light")) : FName(TEXT("ceil_vent"));
				}
				else
				{
					C.CeilTile = (H % 100 < 20) ? FName(TEXT("ceil_plain_b")) : FName(TEXT("ceil_plain"));
				}
			}
		}
	}

	bool TryProp(FArchPlan& Plan, int32 X, int32 Y, const TCHAR* PropId)
	{
		if (!Plan.IsValid(X, Y))
		{
			return false;
		}
		FArchCell& C = Plan.Cell(X, Y);
		if (C.Kind == EArchCellKind::Void || C.bColumn || C.PropTile != NAME_None || HasAnySpecial(C))
		{
			return false;
		}
		C.PropTile = FName(PropId);
		return true;
	}

	void AddThemeProps(FArchPlan& Plan, const FArchRoom& Room, FRandomStream& Rng)
	{
		TArray<FIntPoint> Cells;
		for (int32 Y = Room.Rect.Y0; Y <= Room.Rect.Y1; ++Y)
		{
			for (int32 X = Room.Rect.X0; X <= Room.Rect.X1; ++X)
			{
				if (X == Room.DoorX && Y == Room.DoorY)
				{
					continue;
				}
				Cells.Add(FIntPoint(X, Y));
			}
		}
		for (int32 I = Cells.Num() - 1; I > 0; --I)
		{
			const int32 J = Rng.RandRange(0, I);
			Cells.Swap(I, J);
		}

		TArray<FString> Props;
		if (Room.Theme == FName(TEXT("chapel")))
		{
			Props.Reset(); Props.Add(TEXT("prop_planter")); Props.Add(TEXT("prop_planter")); Props.Add(TEXT("prop_beacon"));
		}
		else if (Room.Theme == FName(TEXT("storage")))
		{
			Props.Reset(); Props.Add(TEXT("prop_crate")); Props.Add(TEXT("prop_barrel")); Props.Add(TEXT("prop_crate")); Props.Add(TEXT("prop_pipes"));
		}
		else if (Room.Theme == FName(TEXT("control")))
		{
			Props.Reset(); Props.Add(TEXT("prop_console")); Props.Add(TEXT("prop_pipes")); Props.Add(TEXT("prop_console"));
		}
		else
		{
			Props.Reset(); Props.Add(TEXT("prop_crate")); Props.Add(TEXT("prop_console")); Props.Add(TEXT("prop_barrel"));
		}
		if (Room.Size == FName(TEXT("5x5")))
		{
			// Copy first: Add() may reallocate Props and invalidate the element
			// reference if it points into the same container.
			const FString Extra0 = Props[0];
			const FString Extra1 = Props[1];
			Props.Add(Extra0);
			Props.Add(Extra1);
		}

		int32 Cursor = 0;
		for (const FString& Prop : Props)
		{
			while (Cursor < Cells.Num())
			{
				const FIntPoint P = Cells[Cursor++];
				if (TryProp(Plan, P.X, P.Y, *Prop))
				{
					break;
				}
			}
		}
	}

	void DressProps(FArchPlan& Plan, FRandomStream& Rng)
	{
		TryProp(Plan, 9, 9, TEXT("prop_console"));
		TryProp(Plan, 9, 8, TEXT("prop_beacon"));
		TryProp(Plan, 10, 9, TEXT("prop_beacon"));
		TryProp(Plan, 9, 10, TEXT("prop_beacon"));
		TryProp(Plan, 8, 9, TEXT("prop_beacon"));

		TryProp(Plan, 6, 6, TEXT("prop_planter"));
		TryProp(Plan, 12, 6, TEXT("prop_planter"));
		TryProp(Plan, 6, 12, TEXT("prop_planter"));
		TryProp(Plan, 12, 12, TEXT("prop_planter"));

		TryProp(Plan, 9, 1, TEXT("prop_beacon"));
		TryProp(Plan, 9, 17, TEXT("prop_beacon"));
		TryProp(Plan, 1, 9, TEXT("prop_beacon"));
		TryProp(Plan, 17, 9, TEXT("prop_beacon"));

		for (const FArchRoom& Room : Plan.Rooms)
		{
			AddThemeProps(Plan, Room, Rng);
		}

		const FIntPoint Cover[] =
		{
			FIntPoint(5, 2), FIntPoint(12, 2), FIntPoint(2, 5), FIntPoint(2, 12),
			FIntPoint(16, 5), FIntPoint(16, 12), FIntPoint(5, 16), FIntPoint(12, 16)
		};
		for (const FIntPoint& P : Cover)
		{
			TryProp(Plan, P.X, P.Y, (Rng.FRand() < 0.5f) ? TEXT("prop_crate") : TEXT("prop_barrel"));
		}
	}

	void BuildUpper(FArchPlan& Plan, FRandomStream& Rng)
	{
		for (int32 Y = 0; Y < Plan.H; ++Y)
		{
			for (int32 X = 0; X < Plan.W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				const bool bVoid = IsVoidKind(C.Kind);
				const uint32 H = HashCell(X, Y, Plan.Seed + 41);

				if (bVoid)
				{
					C.UCeil = FName(TEXT("ceil_open"));
				}
				else
				{
					C.UFloor = (C.Kind == EArchCellKind::Room)
						? ((H % 100 < 35) ? FName(TEXT("floor_plain_b")) : FName(TEXT("floor_plain")))
						: ((H % 100 < 25) ? FName(TEXT("floor_plate_b")) : FName(TEXT("floor_plate")));

					TArray<int32> VoidSides;
					for (int32 Side = 0; Side < 4; ++Side)
					{
						if (Plan.IsValid(X + DX[Side], Y + DY[Side])
							&& IsVoidKind(Plan.Cell(X + DX[Side], Y + DY[Side]).Kind))
						{
							VoidSides.Add(Side);
						}
					}
					if (VoidSides.Num() > 0)
					{
						C.UFloor = FName(TEXT("floor_light"));
						FName Rail;
						FString RailPat;
						if (FWFCArchGrammar::ResolveWallTile(0, VoidSides[0], TEXT('h'), Rng, Rail, RailPat)
							&& RailPat.Len() == 4 && RailPat[VoidSides[0]] == TEXT('h'))
						{
							C.URailing = Rail;
						}
						if (VoidSides.Num() > 1)
						{
							FName Rail2;
							FString Rail2Pat;
							if (FWFCArchGrammar::ResolveWallTile(0, VoidSides[1], TEXT('h'), Rng, Rail2, Rail2Pat)
								&& Rail2Pat.Len() == 4 && Rail2Pat[VoidSides[1]] == TEXT('h'))
							{
								C.URailing2 = Rail2;
							}
						}
					}

					if (X % 3 == 1 && Y % 3 == 1)
					{
						C.UCeil = FName(TEXT("ceil_light"));
					}
					else if (H % 100 < 6)
					{
						C.UCeil = FName(TEXT("ceil_vent"));
					}
					else
					{
						C.UCeil = FName(TEXT("ceil_plain"));
					}
				}

				if (C.bColumn)
				{
					C.bUColumn = true;
				}

				TArray<int32> Per;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if (IsPerimeterSide(Plan, X, Y, Side))
					{
						Per.Add(Side);
					}
				}
				if (Per.Num() > 0)
				{
					uint8 Mask = 0;
					for (int32 Side : Per)
					{
						Mask |= (1 << Side);
					}
					int32 SpecialSide = -1;
					TCHAR SpecialCode = 0;
					if (Per.Num() == 1 && ((X + Y + Plan.Seed) % 2 == 0))
					{
						SpecialSide = Per[0];
						SpecialCode = TEXT('n');
					}
					FName Tile;
					FString Pat;
					if (FWFCArchGrammar::ResolveWallTile(Mask, SpecialSide, SpecialCode, Rng, Tile, Pat))
					{
						C.UWall = Tile;
					}
				}
			}
		}
	}
}

bool FWFCArchGrammar::HasWallPattern(const FString& Pattern)
{
	return WallIndex().Contains(Pattern);
}

bool FWFCArchGrammar::ResolveWallTile(
	uint8 WallMask,
	int32 SpecialSide,
	TCHAR SpecialCode,
	FRandomStream& Rng,
	FName& OutTile,
	FString& OutPattern,
	bool bAllowColumn)
{
	if (SpecialSide >= 0 && SpecialCode != 0)
	{
		const FString Exact = BuildPattern(WallMask, SpecialSide, SpecialCode);
		if (FindWallTile(Exact, Rng, OutTile))
		{
			OutPattern = Exact;
			return true;
		}

		TArray<TCHAR, TInlineAllocator<3>> Fallbacks;
		if (SpecialCode == TEXT('d') || SpecialCode == TEXT('g') || SpecialCode == TEXT('a'))
		{
			Fallbacks.Add(TEXT('p'));
			Fallbacks.Add(TEXT('o'));
		}
		else if (SpecialCode == TEXT('n') || SpecialCode == TEXT('h'))
		{
			Fallbacks.Add(TEXT('w'));
		}
		else
		{
			Fallbacks.Add(TEXT('o'));
		}

		for (TCHAR Code : Fallbacks)
		{
			const FString Try = BuildPattern(WallMask, SpecialSide, Code);
			if (FindWallTile(Try, Rng, OutTile))
			{
				OutPattern = Try;
				return true;
			}
		}
	}

	const FString Plain = BuildPattern(WallMask, -1, 0);
	if (FindWallTile(Plain, Rng, OutTile))
	{
		OutPattern = Plain;
		return true;
	}

	if (FindWallTile(TEXT("oooo"), Rng, OutTile))
	{
		OutPattern = TEXT("oooo");
		return true;
	}
	if (FindWallTile(TEXT("wwww"), Rng, OutTile))
	{
		OutPattern = TEXT("wwww");
		return true;
	}
	return false;
}

bool FWFCArchGrammar::Build(int32 Seed, FArchPlan& OutPlan)
{
	OutPlan.Init(GSize, GSize, Seed);
	FRandomStream Rng(Seed ^ 0x9E3779B9);

	// 1) Open hall: crossing + four arms + side aisles.
	for (int32 Y = 5; Y <= 13; ++Y)
	{
		for (int32 X = 5; X <= 13; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, Y);
			C.Kind = EArchCellKind::Atrium;
			C.Space = 0;
		}
	}
	for (const FHallDef& Def : HallRects)
	{
		for (int32 Y = Def.Rect.Y0; Y <= Def.Rect.Y1; ++Y)
		{
			for (int32 X = Def.Rect.X0; X <= Def.Rect.X1; ++X)
			{
				FArchCell& C = OutPlan.Cell(X, Y);
				C.Kind = Def.Kind;
				C.Space = 0;
			}
		}
	}

	// 2) Corner rooms: one 5x5 hall or a 2x5 + 3x5 pair.
	FCornerPlan Plans[4];
	for (int32 I = 0; I < 4; ++I)
	{
		if (I == 1 || I == 3)
		{
			Plans[I] = Plans[I - 1];
			if (Plans[I].bMerge)
			{
				Plans[I].DoorSide = MirrorSide(Plans[I].DoorSide);
			}
		}
		else
		{
			Plans[I].bMerge = Rng.FRand() < 0.34f;
			if (Plans[I].bMerge)
			{
				TArray<int32> Hall;
				GetHallSides(Corners[I].Rect, Hall);
				Plans[I].DoorSide = Hall[Rng.RandRange(0, Hall.Num() - 1)];
			}
			else
			{
				Plans[I].Axis = Rng.RandRange(0, 1);
			}
		}
	}

	int32 NextRoomId = 1;
	for (int32 I = 0; I < 4; ++I)
	{
		const FArchRect& Block = Corners[I].Rect;
		TArray<int32> Hall;
		GetHallSides(Block, Hall);

		if (Plans[I].bMerge)
		{
			int32 MergedDoorX = 0;
			int32 MergedDoorY = 0;
			EdgeDoor(Block, Plans[I].DoorSide, MergedDoorX, MergedDoorY);
			AddRoom(OutPlan, Block, MergedDoorX, MergedDoorY, Plans[I].DoorSide, TEXT("5x5"), NextRoomId++, Rng);
			continue;
		}

		int32 LongSide = 0;
		int32 ShortSide = 0;
		FArchRect LongRect;
		FArchRect ShortRect;

		if (Plans[I].Axis == 0)
		{
			const bool bEast = Hall.Contains(SideE);
			LongSide = bEast ? SideE : SideW;
			ShortSide = Hall.Contains(SideN) ? SideN : SideS;
			if (bEast)
			{
				LongRect = FArchRect(Block.X1 - 1, Block.Y0, Block.X1, Block.Y1);
				ShortRect = FArchRect(Block.X0, Block.Y0, Block.X1 - 2, Block.Y1);
			}
			else
			{
				LongRect = FArchRect(Block.X0, Block.Y0, Block.X0 + 1, Block.Y1);
				ShortRect = FArchRect(Block.X0 + 2, Block.Y0, Block.X1, Block.Y1);
			}
		}
		else
		{
			const bool bNorth = Hall.Contains(SideN);
			LongSide = bNorth ? SideN : SideS;
			ShortSide = Hall.Contains(SideE) ? SideE : SideW;
			if (bNorth)
			{
				LongRect = FArchRect(Block.X0, Block.Y1 - 1, Block.X1, Block.Y1);
				ShortRect = FArchRect(Block.X0, Block.Y0, Block.X1, Block.Y1 - 2);
			}
			else
			{
				LongRect = FArchRect(Block.X0, Block.Y0, Block.X1, Block.Y0 + 1);
				ShortRect = FArchRect(Block.X0, Block.Y0 + 2, Block.X1, Block.Y1);
			}
		}

		int32 LX = 0, LY = 0, SX = 0, SY = 0;
		EdgeDoor(LongRect, LongSide, LX, LY);
		EdgeDoor(ShortRect, ShortSide, SX, SY);
		AddRoom(OutPlan, LongRect, LX, LY, LongSide, TEXT("2x5"), NextRoomId++, Rng);
		AddRoom(OutPlan, ShortRect, SX, SY, ShortSide, TEXT("3x5"), NextRoomId++, Rng);
	}

	// 3) Boundary walls from the space graph.
	for (int32 Y = 0; Y < OutPlan.H; ++Y)
	{
		for (int32 X = 0; X < OutPlan.W; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, Y);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = X + DX[Side];
				const int32 NY = Y + DY[Side];
				if (!OutPlan.IsValid(NX, NY) || OutPlan.Cell(NX, NY).Space != C.Space)
				{
					C.WallMask |= (1 << Side);
				}
			}
		}
	}

	// 4) Room doors. Only the dooo/pooo family is needed: every door sits in
	//    the middle of a 5-cell or 3-cell frontage with no other walls.
	for (FArchRoom& Room : OutPlan.Rooms)
	{
		const TCHAR Code = (Room.Size == FName(TEXT("2x5"))) ? TEXT('d') : TEXT('p');
		int32 X = Room.DoorX;
		int32 Y = Room.DoorY;
		int32 Side = Room.DoorSide;
		if (!IsCleanDoorCell(OutPlan, X, Y, Side))
		{
			if (!FindCleanDoorCell(OutPlan, Room, X, Y, Side))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Arch] Room %d has no clean door cell."), Room.Id);
			}
		}
		if (!SetOpening(OutPlan, X, Y, Side, Code))
		{
			if (!SetOpening(OutPlan, X, Y, Side, TEXT('p')))
			{
				SetOpening(OutPlan, X, Y, Side, TEXT('o'));
			}
		}
		Room.DoorX = X;
		Room.DoorY = Y;
		Room.DoorSide = Side;
	}

	// 5) Axial gates and facade windows.
	SetOpening(OutPlan, 9, 0, SideS, TEXT('p'));
	SetOpening(OutPlan, 0, 9, SideW, TEXT('p'));
	SetOpening(OutPlan, 18, 9, SideE, TEXT('p'));
	SetOpening(OutPlan, 9, 18, SideN, TEXT('n'));

	for (int32 Y = 0; Y < OutPlan.H; ++Y)
	{
		for (int32 X = 0; X < OutPlan.W; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, Y);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				if (!IsPerimeterSide(OutPlan, X, Y, Side))
				{
					continue;
				}
				if ((X == 9 && Y == 0 && Side == SideS)
					|| (X == 0 && Y == 9 && Side == SideW)
					|| (X == 18 && Y == 9 && Side == SideE)
					|| (X == 9 && Y == 18 && Side == SideN))
				{
					continue;
				}
				if (HasAnySpecial(C) || C.Special[Side] != 0)
				{
					continue;
				}
				if ((C.WallMask & ~(1 << Side)) != 0)
				{
					continue;
				}
				if (((X + Y + OutPlan.Seed) % 3) > 1)
				{
					continue;
				}
				if (Rng.FRand() < 0.22f)
				{
					continue;
				}
				const FString Pat = BuildPattern(C.WallMask, Side, TEXT('n'));
				if (!HasWallPattern(Pat))
				{
					continue;
				}
				SetOpening(OutPlan, X, Y, Side, TEXT('n'));
			}
		}
	}

	// 6) Colonnade: only cells that are fully open (all four sides).
	for (const FIntPoint& P : ColumnCells)
	{
		if (!OutPlan.IsValid(P.X, P.Y))
		{
			continue;
		}
		FArchCell& C = OutPlan.Cell(P.X, P.Y);
		if (C.Kind == EArchCellKind::Room || IsVoidKind(C.Kind))
		{
			continue;
		}
		if (C.WallMask != 0 || HasAnySpecial(C))
		{
			continue;
		}
		C.bColumn = true;
	}

	// 7) Resolve every cell to an actual kit tile; parse the actual pattern back
	//    so any fallback (door -> passage -> open) stays consistent.
	for (int32 Y = 0; Y < OutPlan.H; ++Y)
	{
		for (int32 X = 0; X < OutPlan.W; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, Y);
			if (C.bColumn)
			{
				C.WallTile = FName(TEXT("wall_oooo_column"));
				C.WallMask = 0;
				C.Special[0] = C.Special[1] = C.Special[2] = C.Special[3] = 0;
				continue;
			}

			FName Tile;
			FString Pat;
			if (!ResolveWallTile(C.WallMask, -1, 0, Rng, Tile, Pat))
			{
				Tile = FName(TEXT("wall_wwww"));
				Pat = TEXT("wwww");
			}
			C.WallTile = Tile;
			C.WallMask = 0;
			C.Special[0] = C.Special[1] = C.Special[2] = C.Special[3] = 0;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const TCHAR Ch = (Side < Pat.Len()) ? Pat[Side] : TEXT('o');
				if (Ch == TEXT('w'))
				{
					C.WallMask |= (1 << Side);
				}
				else if (Ch == TEXT('d') || Ch == TEXT('p') || Ch == TEXT('n') || Ch == TEXT('h'))
				{
					C.Special[Side] = Ch;
				}
			}
		}
	}

	// 8) Floors, lower ceilings, props.
	DressFloor(OutPlan);
	DressCeiling(OutPlan);
	DressProps(OutPlan, Rng);

	// 9) Mezzanine / cross-shaped skylight.
	BuildUpper(OutPlan, Rng);

	UE_LOG(LogTemp, Log, TEXT("[Arch] Built plan seed=%d rooms=%d columns=%d."),
		Seed, OutPlan.Rooms.Num(), 36);
	return true;
}

