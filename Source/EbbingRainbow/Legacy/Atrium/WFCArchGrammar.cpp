// Copyright Epic Games, Inc. All Rights Reserved.

#include "Legacy/Atrium/WFCArchGrammar.h"
#include "Core/ERLog.h"
#include "Core/ERDebug.h"
#include "Legacy/Atrium/WFCKitTileTable.h"
#include "HAL/IConsoleManager.h"

/** WFC 竞技场布局风格 (0=旧房间图 1=十字中庭 2=Blackshard 大厅) */
static TAutoConsoleVariable<int32> CVarArchLayout(
	TEXT("ER.ArchLayout"), 2,
	TEXT("WFC 竞技场布局: 0=旧房间图 1=十字中庭 2=Blackshard 大厅"),
	ECVF_Cheat);

/** 把生成出来的平面图打到日志 (调试用) */
static TAutoConsoleVariable<int32> CVarArchDump(
	TEXT("ER.ArchDump"), 0,
	TEXT(">0 时把 WFC 竞技场的平面图打印到日志"),
	ECVF_Cheat);

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
				// Semi-open: the old continuous 4 m lid is gone. A small
				// number of rooms keep a service ceiling; everything else is
				// open to the upper level / sky.
				C.CeilTile = NAME_None;
				const bool bCorner = (X < 5 || X > 13) && (Y < 5 || Y > 13);
				if (C.Kind == EArchCellKind::Room && bCorner && H % 100 < 12)
				{
					C.CeilTile = (H % 100 < 4)
						? FName(TEXT("ceil_light")) : FName(TEXT("ceil_vent"));
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
					// No coffered/scaffold ceiling over the open cross. The old
					// ceil_open module was itself a large lattice and read as a
					// roof from the top-down camera.
					C.UCeil = NAME_None;
				}
				else
				{
					// Partially open mezzanine: aisles are always walkable
					// balconies, but only ~55% of the rooms get an upper slab.
					// The rest are double-height rooms, visible from above.
					if (C.Kind == EArchCellKind::Room)
					{
						C.UFloor = (H % 100 < 55)
							? ((H % 100 < 20) ? FName(TEXT("floor_plain_b")) : FName(TEXT("floor_plain")))
							: NAME_None;
					}
					else
					{
						C.UFloor = (H % 100 < 25) ? FName(TEXT("floor_plate_b")) : FName(TEXT("floor_plate"));
					}

					if (!C.UFloor.IsNone())
					{
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

						// Semi-open arena: no continuous 8 m roof. Only a few
						// corner rooms keep a small roof patch; the rest is open
						// sky, so the top-down camera never sees a large lattice.
						C.UCeil = NAME_None;
						const bool bCorner = (X < 5 || X > 13) && (Y < 5 || Y > 13);
						if (C.Kind == EArchCellKind::Room && bCorner && H % 100 < 18)
						{
							C.UCeil = (H % 2 == 0)
								? FName(TEXT("ceil_light"))
								: ((H % 5 == 0) ? FName(TEXT("ceil_vent")) : FName(TEXT("ceil_plain")));
						}
					}
					else
					{
						C.UCeil = NAME_None;
					}
				}

				C.bUColumn = C.bColumn && !C.UFloor.IsNone();

				if (!C.UFloor.IsNone())
				{
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

	// ---- room graph -------------------------------------------------------
	struct FGraphRoom
	{
		FArchRect Rect;
		int32 Id = INDEX_NONE;
		int32 Degree = 0;
	};

	static int32 SideFromTo(int32 X0, int32 Y0, int32 X1, int32 Y1)
	{
		if (X1 > X0) return 1;
		if (X1 < X0) return 3;
		if (Y1 > Y0) return 2;
		if (Y1 < Y0) return 0;
		return INDEX_NONE;
	}

	static FIntPoint PickDoorCell(const FArchRect& R, int32 Side, TArray<uint8>& Used, FRandomStream& Rng)
	{
		TArray<FIntPoint> Candidates;
		if (Side == 0)
		{
			for (int32 X = R.X0; X <= R.X1; ++X) Candidates.Add(FIntPoint(X, R.Y0));
		}
		else if (Side == 1)
		{
			for (int32 Y = R.Y0; Y <= R.Y1; ++Y) Candidates.Add(FIntPoint(R.X1, Y));
		}
		else if (Side == 2)
		{
			for (int32 X = R.X0; X <= R.X1; ++X) Candidates.Add(FIntPoint(X, R.Y1));
		}
		else
		{
			for (int32 Y = R.Y0; Y <= R.Y1; ++Y) Candidates.Add(FIntPoint(R.X0, Y));
		}

		TArray<FIntPoint> Free;
		for (const FIntPoint& P : Candidates)
		{
			if (Used[P.Y * 19 + P.X] == 0) Free.Add(P);
		}
		if (Free.Num() == 0) Free = Candidates;
		if (Free.Num() == 0) return FIntPoint(-1, -1);

		const FIntPoint P = Free[Rng.RandRange(0, Free.Num() - 1)];
		Used[P.Y * 19 + P.X] = 1;
		return P;
	}

	static bool FindGraphPath(
		const TArray<int32>& RoomIdOf,
		const TArray<uint8>& Blocked,
		int32 W, int32 H,
		FIntPoint Start, FIntPoint End,
		TArray<FIntPoint>& OutPath)
	{
		TArray<int32> Parent;
		Parent.Init(-1, W * H);
		TArray<int32> Queue;
		Queue.Reserve(W * H);

		const int32 S = Start.Y * W + Start.X;
		const int32 E = End.Y * W + End.X;
		Parent[S] = S;
		Queue.Add(S);

		int32 Head = 0;
		while (Head < Queue.Num() && Parent[E] == -1)
		{
			const int32 Cur = Queue[Head++];
			const int32 CX = Cur % W;
			const int32 CY = Cur / W;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = CX + DX[Side];
				const int32 NY = CY + DY[Side];
				if (NX < 0 || NY < 0 || NX >= W || NY >= H) continue;
				const int32 NI = NY * W + NX;
				if (Parent[NI] != -1) continue;
				if (NI != E)
				{
					if (RoomIdOf[NI] >= 0) continue;
					if (Blocked[NI]) continue;
				}
				Parent[NI] = Cur;
				Queue.Add(NI);
			}
		}

		if (Parent[E] == -1) return false;

		OutPath.Reset();
		for (int32 I = E; I != S; I = Parent[I])
		{
			OutPath.Add(FIntPoint(I % W, I / W));
		}
		OutPath.Add(Start);
		for (int32 I = 0; I < OutPath.Num() / 2; ++I)
		{
			Swap(OutPath[I], OutPath[OutPath.Num() - 1 - I]);
		}
		return true;
	}

	static bool BuildRoomGraphPlan(int32 Seed, FArchPlan& Plan, FRandomStream& Rng)
	{
		constexpr int32 W = 19;
		constexpr int32 H = 19;
		constexpr int32 Num = W * H;
		constexpr int32 Slots = 4;
		constexpr int32 SlotSize = 4;
		constexpr int32 Origin = 1;

		TArray<int32> RoomIdOf;
		RoomIdOf.Init(-1, Num);
		TArray<uint8> DoorBit;
		DoorBit.Init(0, Num);
		TArray<uint8> Blocked;
		Blocked.Init(0, Num);

		auto Hash = [](int32 X, int32 Y, int32 S)
		{
			return static_cast<uint32>(X * 73856093) ^ static_cast<uint32>(Y * 19349663)
				^ static_cast<uint32>(S * 83492791);
		};

		// One random central slot becomes an open courtyard. Keeping only one
		// void slot (instead of a 2x2 ring) lets rooms have 3-4 neighbours.
		const int32 CourtSX = Rng.RandRange(1, 2);
		const int32 CourtSY = Rng.RandRange(1, 2);
		const int32 CourtX0 = Origin + CourtSX * SlotSize;
		const int32 CourtY0 = Origin + CourtSY * SlotSize;
		for (int32 Y = CourtY0; Y < CourtY0 + SlotSize; ++Y)
		{
			for (int32 X = CourtX0; X < CourtX0 + SlotSize; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				C.Kind = EArchCellKind::Atrium;
				C.Space = 0;
				Blocked[Y * W + X] = 1;
			}
		}

		// All slots except the courtyard are available (15 of 16).
		TArray<FIntPoint> Available;
		for (int32 SY = 0; SY < Slots; ++SY)
		{
			for (int32 SX = 0; SX < Slots; ++SX)
			{
				if (SX == CourtSX && SY == CourtSY) continue;
				Available.Add(FIntPoint(SX, SY));
			}
		}

		bool Selected[16] = { false };
		auto SlotIndex = [&](int32 SX, int32 SY) { return SY * Slots + SX; };

		// Random connected subset of the 15 ring slots (10-12 rooms).
		FIntPoint First = Available[Rng.RandRange(0, Available.Num() - 1)];
		Selected[SlotIndex(First.X, First.Y)] = true;
		TArray<FIntPoint> Chosen;
		Chosen.Add(First);

		const int32 Target = Rng.RandRange(10, 12);
		while (Chosen.Num() < Target)
		{
			TArray<FIntPoint> Candidates;
			for (const FIntPoint& S : Chosen)
			{
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = S.X + DX[Side];
					const int32 NY = S.Y + DY[Side];
					if (NX < 0 || NX >= Slots || NY < 0 || NY >= Slots) continue;
					if (NX == CourtSX && NY == CourtSY) continue;
					if (Selected[SlotIndex(NX, NY)]) continue;
					Candidates.AddUnique(FIntPoint(NX, NY));
				}
			}
			if (Candidates.Num() == 0) break;
			const FIntPoint Pick = Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
			Selected[SlotIndex(Pick.X, Pick.Y)] = true;
			Chosen.Add(Pick);
		}

		// One 2x2 room per selected slot.
		TArray<FGraphRoom> Rooms;
		int32 RoomBySlot[16];
		for (int32& SlotRoom : RoomBySlot) SlotRoom = INDEX_NONE;
		for (const FIntPoint& S : Chosen)
		{
			const int32 SX0 = Origin + S.X * SlotSize;
			const int32 SY0 = Origin + S.Y * SlotSize;
			// 2x2 rooms leave a 1-2 cell skirt inside each 4x4 slot, which is
			// what lets the 1-cell corridor BFS find a path on every edge.
			constexpr int32 RW = 2;
			constexpr int32 RH = 2;
			const int32 OX = Rng.RandRange(1, SlotSize - RW - 1);
			const int32 OY = Rng.RandRange(1, SlotSize - RH - 1);

			FGraphRoom R;
			R.Rect = FArchRect(SX0 + OX, SY0 + OY, SX0 + OX + RW - 1, SY0 + OY + RH - 1);
			R.Id = Rooms.Num();
			Rooms.Add(R);

			for (int32 Y = R.Rect.Y0; Y <= R.Rect.Y1; ++Y)
			{
				for (int32 X = R.Rect.X0; X <= R.Rect.X1; ++X)
				{
					FArchCell& C = Plan.Cell(X, Y);
					C.Kind = EArchCellKind::Room;
					C.Space = R.Id;
					RoomIdOf[Y * W + X] = R.Id;
				}
			}

			FArchRoom Room;
			Room.Id = R.Id;
			Room.Rect = R.Rect;
			Room.Theme = (Rng.FRand() < 0.5f) ? FName(TEXT("GraphA")) : FName(TEXT("GraphB"));
			Room.Size = FName(*FString::Printf(TEXT("%dx%d"), RW, RH));
			Plan.Rooms.Add(Room);
			RoomBySlot[SlotIndex(S.X, S.Y)] = R.Id;
		}

		// Connect every adjacent pair of selected slots exactly once. Use all
		// four directions and normalize each pair to west/north -> east/south.
		int32 CorridorCount = 0;
		int32 EdgeCount = 0;
		for (const FIntPoint& S : Chosen)
		{
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = S.X + DX[Side];
				const int32 NY = S.Y + DY[Side];
				if (NX < 0 || NX >= Slots || NY < 0 || NY >= Slots) continue;
				if (!Selected[SlotIndex(NX, NY)]) continue;
				if (SlotIndex(NX, NY) < SlotIndex(S.X, S.Y)) continue;

				const int32 AIdx = RoomBySlot[SlotIndex(S.X, S.Y)];
				const int32 BIdx = RoomBySlot[SlotIndex(NX, NY)];
				if (AIdx == INDEX_NONE || BIdx == INDEX_NONE) continue;

				FGraphRoom* P = &Rooms[AIdx];
				FGraphRoom* Q = &Rooms[BIdx];
				if (NX < S.X || (NX == S.X && NY < S.Y))
				{
					Swap(P, Q);
				}

				if (P->Rect.X1 < Q->Rect.X0)
				{
					const int32 Y0 = FMath::Max(P->Rect.Y0, Q->Rect.Y0);
					const int32 Y1 = FMath::Min(P->Rect.Y1, Q->Rect.Y1);
					const int32 DoorY = (Y0 <= Y1) ? Rng.RandRange(Y0, Y1) : P->Rect.Y0;
					const FIntPoint PA(P->Rect.X1, DoorY);
					const FIntPoint PB(Q->Rect.X0, DoorY);
					DoorBit[PA.Y * W + PA.X] |= 1 << 1;
					DoorBit[PB.Y * W + PB.X] |= 1 << 3;
					for (int32 CX = P->Rect.X1 + 1; CX < Q->Rect.X0; ++CX)
					{
						FArchCell& C = Plan.Cell(CX, DoorY);
						if (C.Kind == EArchCellKind::Void)
						{
							C.Kind = EArchCellKind::Corridor;
							C.Space = INDEX_NONE;
							++CorridorCount;
						}
					}
					P->Degree++;
					Q->Degree++;
					++EdgeCount;
				}
				else if (P->Rect.Y1 < Q->Rect.Y0)
				{
					const int32 X0 = FMath::Max(P->Rect.X0, Q->Rect.X0);
					const int32 X1 = FMath::Min(P->Rect.X1, Q->Rect.X1);
					const int32 DoorX = (X0 <= X1) ? Rng.RandRange(X0, X1) : P->Rect.X0;
					const FIntPoint PA(DoorX, P->Rect.Y1);
					const FIntPoint PB(DoorX, Q->Rect.Y0);
					DoorBit[PA.Y * W + PA.X] |= 1 << 2;
					DoorBit[PB.Y * W + PB.X] |= 1 << 0;
					for (int32 CY = P->Rect.Y1 + 1; CY < Q->Rect.Y0; ++CY)
					{
						FArchCell& C = Plan.Cell(DoorX, CY);
						if (C.Kind == EArchCellKind::Void)
						{
							C.Kind = EArchCellKind::Corridor;
							C.Space = INDEX_NONE;
							++CorridorCount;
						}
					}
					P->Degree++;
					Q->Degree++;
					++EdgeCount;
				}
			}
		}

		// Link the central courtyard to 2-4 rooms so it becomes a hub instead
		// of an isolated platform. Each link is a short 1-cell corridor and
		// counts as one opening of the room (still capped at 4).
		const int32 CourtX1 = CourtX0 + SlotSize - 1;
		const int32 CourtY1 = CourtY0 + SlotSize - 1;
		auto LinkCourt = [&](int32 Dir)
		{
			const int32 TSX = CourtSX + DX[Dir];
			const int32 TSY = CourtSY + DY[Dir];
			if (TSX < 0 || TSX >= Slots || TSY < 0 || TSY >= Slots) return;
			if (!Selected[SlotIndex(TSX, TSY)]) return;
			const int32 RIdx = RoomBySlot[SlotIndex(TSX, TSY)];
			if (RIdx == INDEX_NONE || Rooms[RIdx].Degree >= 4) return;

			FGraphRoom& R = Rooms[RIdx];
			if (Dir == 0 || Dir == 2) // room north/south of the court
			{
				const int32 X0 = FMath::Max(R.Rect.X0, CourtX0);
				const int32 X1 = FMath::Min(R.Rect.X1, CourtX1);
				if (X0 > X1) return;
				const int32 DoorX = Rng.RandRange(X0, X1);
				if (Dir == 0)
				{
					DoorBit[R.Rect.Y1 * W + DoorX] |= 1 << 2;
					for (int32 CY = R.Rect.Y1 + 1; CY < CourtY0; ++CY)
					{
						FArchCell& C = Plan.Cell(DoorX, CY);
						if (C.Kind == EArchCellKind::Void) { C.Kind = EArchCellKind::Corridor; ++CorridorCount; }
					}
				}
				else
				{
					DoorBit[R.Rect.Y0 * W + DoorX] |= 1 << 0;
					for (int32 CY = CourtY1 + 1; CY < R.Rect.Y0; ++CY)
					{
						FArchCell& C = Plan.Cell(DoorX, CY);
						if (C.Kind == EArchCellKind::Void) { C.Kind = EArchCellKind::Corridor; ++CorridorCount; }
					}
				}
			}
			else // room east/west of the court
			{
				const int32 Y0 = FMath::Max(R.Rect.Y0, CourtY0);
				const int32 Y1 = FMath::Min(R.Rect.Y1, CourtY1);
				if (Y0 > Y1) return;
				const int32 DoorY = Rng.RandRange(Y0, Y1);
				if (Dir == 1)
				{
					DoorBit[DoorY * W + R.Rect.X0] |= 1 << 3;
					for (int32 CX = CourtX1 + 1; CX < R.Rect.X0; ++CX)
					{
						FArchCell& C = Plan.Cell(CX, DoorY);
						if (C.Kind == EArchCellKind::Void) { C.Kind = EArchCellKind::Corridor; ++CorridorCount; }
					}
				}
				else
				{
					DoorBit[DoorY * W + R.Rect.X1] |= 1 << 1;
					for (int32 CX = R.Rect.X1 + 1; CX < CourtX0; ++CX)
					{
						FArchCell& C = Plan.Cell(CX, DoorY);
						if (C.Kind == EArchCellKind::Void) { C.Kind = EArchCellKind::Corridor; ++CorridorCount; }
					}
				}
			}
			R.Degree++;
			++EdgeCount;
		};
		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			if (Rng.FRand() < 0.6f) LinkCourt(Dir);
		}

		// Resolve walls, doors, floors, props. No roof: the graph is open-air.
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				const int32 Idx = Y * W + X;

				if (C.Kind == EArchCellKind::Void)
				{
					continue;
				}
				if (C.Kind == EArchCellKind::Atrium)
				{
					const uint32 Hsh = Hash(X, Y, Seed);
					C.FloorTile = (Hsh % 100 < 25) ? FName(TEXT("floor_light")) : FName(TEXT("floor_plate"));
					C.CeilTile = NAME_None;
					continue;
				}

				const bool bCellRoom = C.Kind == EArchCellKind::Room;

				// Semi-open room graph: no interior partitions. Only the outer
				// silhouette gets a solid wall; interior room/corridor edges
				// stay open, and the door bits add visual door frames later.
				uint8 Mask = 0;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = X + DX[Side];
					const int32 NY = Y + DY[Side];
					if (NX < 0 || NY < 0 || NX >= W || NY >= H
						|| Plan.Cell(NX, NY).Kind == EArchCellKind::Void)
					{
						Mask |= 1 << Side;
					}
				}

				C.WallMask = Mask;
				C.Special[0] = C.Special[1] = C.Special[2] = C.Special[3] = 0;

				FName Tile;
				FString Pattern;
				if (FWFCArchGrammar::ResolveWallTile(Mask, INDEX_NONE, 0, Rng, Tile, Pattern))
				{
					C.WallTile = Tile;
				}

				const uint32 Hsh = Hash(X, Y, Seed + 3);
				C.FloorTile = bCellRoom
					? ((Hsh % 100 < 35) ? FName(TEXT("floor_plain_b")) : FName(TEXT("floor_plain")))
					: ((Hsh % 100 < 30) ? FName(TEXT("floor_plate_b")) : FName(TEXT("floor_plate")));
				C.CeilTile = NAME_None;

				if (bCellRoom && DoorBit[Idx] == 0 && Hsh % 100 < 20)
				{
					const TCHAR* Props[] = { TEXT("prop_crate"), TEXT("prop_barrel"),
						TEXT("prop_console"), TEXT("prop_beacon") };
					C.PropTile = FName(Props[Hsh % 4]);
				}
			}
		}

		int32 MinDeg = 99;
		int32 MaxDeg = 0;
		for (const FGraphRoom& R : Rooms)
		{
			MinDeg = FMath::Min(MinDeg, R.Degree);
			MaxDeg = FMath::Max(MaxDeg, R.Degree);
		}
		if (Rooms.Num() == 0)
		{
			MinDeg = MaxDeg = 0;
		}

		// Strong door opening: the per-side wall test above is subtle, so make
		// the DoorBit authoritative. Clear the wall bit on both sides of every
		// door and re-resolve the affected wall tiles with an explicit 'd'.
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const int32 Idx = Y * W + X;
				if (DoorBit[Idx] == 0) continue;
				FArchCell& C = Plan.Cell(X, Y);
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if ((DoorBit[Idx] & (1 << Side)) == 0) continue;
					C.WallMask &= ~(1 << Side);
					if (C.Kind == EArchCellKind::Room)
					{
						C.Special[Side] = TEXT('d');
					}
					const int32 NX = X + DX[Side];
					const int32 NY = Y + DY[Side];
					if (NX >= 0 && NY >= 0 && NX < W && NY < H)
					{
						FArchCell& N = Plan.Cell(NX, NY);
						N.WallMask &= ~(1 << ((Side + 2) % 4));
					}
				}
			}
		}

		// Re-resolve room/corridor wall tiles after the door clearing.
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				if (C.Kind != EArchCellKind::Room && C.Kind != EArchCellKind::Corridor)
				{
					continue;
				}
				int32 Spec = INDEX_NONE;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if (C.Special[Side] == TEXT('d'))
					{
						Spec = Side;
						break;
					}
				}
				FName Tile;
				FString Pat;
				if (FWFCArchGrammar::ResolveWallTile(
					C.WallMask, Spec, Spec != INDEX_NONE ? TEXT('d') : 0, Rng, Tile, Pat))
				{
					C.WallTile = Tile;
				}
			}
		}

		// Plan-level connectivity probe: follow open wall sides (doors) from
		// the first room. Large planOpen + small nav reachable points at the
		// explicit collision boxes, not at the door graph.
		int32 DoorCells = 0;
		for (int32 I = 0; I < Num; ++I)
		{
			DoorCells += DoorBit[I] != 0 ? 1 : 0;
		}

		int32 StartIdx = INDEX_NONE;
		for (int32 I = 0; I < Num; ++I)
		{
			if (Plan.Cells[I].Kind == EArchCellKind::Room)
			{
				StartIdx = I;
				break;
			}
		}

		int32 PlanOpen = 0;
		if (StartIdx != INDEX_NONE)
		{
			TArray<uint8> Vis;
			Vis.Init(0, Num);
			TArray<int32> Queue;
			Queue.Add(StartIdx);
			Vis[StartIdx] = 1;
			int32 Head = 0;
			while (Head < Queue.Num())
			{
				const int32 Cur = Queue[Head++];
				++PlanOpen;
				const int32 CX = Cur % W;
				const int32 CY = Cur / W;
				const FArchCell& C = Plan.Cells[Cur];
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if ((C.WallMask & (1 << Side)) != 0) continue;
					const int32 NX = CX + DX[Side];
					const int32 NY = CY + DY[Side];
					if (NX < 0 || NY < 0 || NX >= W || NY >= H) continue;
					const int32 NI = NY * W + NX;
					if (Vis[NI]) continue;
					Vis[NI] = 1;
					Queue.Add(NI);
				}
			}
		}

		Plan.AisleWidth = 0;
		UE_LOG(LogERWFC, Log,
			TEXT("[Arch] Built room graph seed=%d rooms=%d edges=%d corridors=%d degree=%d..%d doors=%d planOpen=%d."),
			Seed, Rooms.Num(), EdgeCount, CorridorCount, MinDeg, MaxDeg, DoorCells, PlanOpen);
		return true;
	}

	// ---- Blackshard 大厅布局 ------------------------------------------------
	//
	//   76 x 76 m 的整块基座, 由内到外是三层:
	//     · 中心 24-28 m 见方的双层挑空中庭, 地面铺深色金属, 正中是十字天桥
	//     · 中庭外 1 格宽、+4 m 高的环形回廊, 回廊外侧一圈 4 m 方柱柱廊
	//     · 四向各一部 8 m 宽的纪念性楼梯, 从大厅地面接上回廊
	//     · 最外圈是 24 m 见方的大厅/房间; 房间朝中庭的转角被削成阶梯状
	//       45° 斜面, 因此是 L 形而不是方块 (黑石建筑的体块感)
	//   楼梯用 FArchStair::Dir 表示上行方向 (0=+Y 1=+X 2=-Y 3=-X), 生成器按
	//   方向旋转模型, 并用一串台阶碰撞盒铺出可走坡道。

	/** 一条连续的可开门边段: 固定行/列 + 沿边起止 */
	struct FDoorRun
	{
		int32 Side = 0;
		int32 Fixed = 0;
		int32 Start = 0;
		int32 Len = 0;
	};

	/** 把房间的一格挖成大厅地面 (用于削角 / 开缺口) */
	static void CarveRoomCell(FArchPlan& Plan, int32 X, int32 Y, int32 RoomId)
	{
		if (!Plan.IsValid(X, Y))
		{
			return;
		}
		FArchCell& C = Plan.Cell(X, Y);
		if (C.Kind == EArchCellKind::Room && C.Space == RoomId)
		{
			C.Kind = EArchCellKind::Corridor;
			C.Space = INDEX_NONE;
		}
	}

	/** 沿房间临大厅的边开 1-3 个 2 格宽门洞, 返回实际门格数 */
	static int32 PlaceWideDoors(
		FArchPlan& Plan, const FArchRect& Rect, int32 RoomId, FRandomStream& Rng, int32 MaxDoors)
	{
		TMap<int32, TArray<int32>> ByKey;
		for (int32 Y = Rect.Y0; Y <= Rect.Y1; ++Y)
		{
			for (int32 X = Rect.X0; X <= Rect.X1; ++X)
			{
				const FArchCell& C = Plan.Cell(X, Y);
				if (C.Kind != EArchCellKind::Room || C.Space != RoomId)
				{
					continue;
				}
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = X + DX[Side];
					const int32 NY = Y + DY[Side];
					if (!Plan.IsValid(NX, NY))
					{
						continue;
					}
					if (Plan.Cell(NX, NY).Kind != EArchCellKind::Corridor)
					{
						continue;
					}
					const bool bHorizontal = (Side == SideN || Side == SideS);
					ByKey.FindOrAdd(Side * 100 + (bHorizontal ? Y : X))
						.Add(bHorizontal ? X : Y);
				}
			}
		}
		if (ByKey.Num() == 0)
		{
			return 0;
		}

		TArray<FDoorRun> Runs;
		for (const TPair<int32, TArray<int32>>& Pair : ByKey)
		{
			TArray<int32> List = Pair.Value;
			List.Sort();
			int32 RunStart = 0;
			for (int32 K = 1; K <= List.Num(); ++K)
			{
				if (K == List.Num() || List[K] != List[K - 1] + 1)
				{
					FDoorRun Run;
					Run.Side = Pair.Key / 100;
					Run.Fixed = Pair.Key % 100;
					Run.Start = List[RunStart];
					Run.Len = K - RunStart;
					Runs.Add(Run);
					RunStart = K;
				}
			}
		}
		for (int32 K = Runs.Num() - 1; K > 0; --K)
		{
			Runs.Swap(K, Rng.RandRange(0, K));
		}

		auto ApplyDoor = [&Plan](int32 X, int32 Y, int32 Side) -> bool
		{
			FArchCell& C = Plan.Cell(X, Y);
			if (C.Kind != EArchCellKind::Room || HasAnySpecial(C))
			{
				return false;
			}
			C.WallMask &= ~(1 << Side);
			C.Special[Side] = TEXT('d');
			const int32 NX = X + DX[Side];
			const int32 NY = Y + DY[Side];
			if (Plan.IsValid(NX, NY))
			{
				Plan.Cell(NX, NY).WallMask &= ~(1 << Opp[Side]);
			}
			return true;
		};

		const int32 WantDoors = (Runs.Num() >= 2)
			? Rng.RandRange(2, FMath::Max(2, FMath::Min(MaxDoors, Runs.Num())))
			: 1;
		int32 Placed = 0;
		for (int32 R = 0; R < Runs.Num() && Placed < WantDoors; ++R)
		{
			const FDoorRun& Run = Runs[R];
			const int32 Width = (Run.Len >= 2) ? 2 : 1;
			const int32 MaxStart = Run.Start + Run.Len - Width;
			const int32 Start = (MaxStart > Run.Start)
				? Rng.RandRange(Run.Start, MaxStart) : Run.Start;
			for (int32 K = 0; K < Width; ++K)
			{
				const bool bHorizontal = (Run.Side == SideN || Run.Side == SideS);
				const int32 X = bHorizontal ? (Start + K) : Run.Fixed;
				const int32 Y = bHorizontal ? Run.Fixed : (Start + K);
				if (ApplyDoor(X, Y, Run.Side))
				{
					++Placed;
				}
			}
		}
		return Placed;
	}

	static bool BuildBlackshardPlan(int32 Seed, FArchPlan& Plan, FRandomStream& Rng)
	{
		constexpr int32 W = 19;
		constexpr int32 H = 19;

		// ---------- 1) 十字大厅 + 中央中庭 ----------
		const int32 ArmW = Rng.RandRange(4, 5);          // 16 / 20 m 宽的大厅
		// 中庭 24-28 m 见方: 比大厅更宽, 是整场的视觉中心
		const int32 AtriumSize = FMath::Clamp(ArmW + 2 + Rng.RandRange(0, 1), 6, 7);
		const int32 Ax0 = 9 - AtriumSize / 2;
		const int32 Ax1 = Ax0 + AtriumSize - 1;
		const int32 Ay0 = Ax0;
		const int32 Ay1 = Ax1;

		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				C.Kind = EArchCellKind::Corridor;
				C.Space = INDEX_NONE;
			}
		}
		for (int32 Y = Ay0; Y <= Ay1; ++Y)
		{
			for (int32 X = Ax0; X <= Ax1; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				C.Kind = EArchCellKind::Atrium;
				C.Space = 0;
			}
		}

		// ---------- 2) 四个象限的大房间 ----------
		FArchRect Quads[4];
		Quads[0] = FArchRect(0, 0, Ax0 - 1, Ay0 - 1);
		Quads[1] = FArchRect(Ax1 + 1, 0, W - 1, Ay0 - 1);
		Quads[2] = FArchRect(0, Ay1 + 1, Ax0 - 1, H - 1);
		Quads[3] = FArchRect(Ax1 + 1, Ay1 + 1, W - 1, H - 1);

		int32 RoomCount = 0;
		TArray<FArchRect> Plazas;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			const FArchRect& R = Quads[Q];
			if (R.NumX() < 4 || R.NumY() < 4)
			{
				continue;
			}
			// 最多一个象限留作开放广场 (再多就显得空), 其余都是封闭大房间
			if (Plazas.Num() == 0 && Rng.FRand() > 0.78f)
			{
				Plazas.Add(R);
				continue;
			}

			const int32 Id = RoomCount++;
			for (int32 Y = R.Y0; Y <= R.Y1; ++Y)
			{
				for (int32 X = R.X0; X <= R.X1; ++X)
				{
					FArchCell& C = Plan.Cell(X, Y);
					C.Kind = EArchCellKind::Room;
					C.Space = Id;
				}
			}

			FArchRoom Room;
			Room.Id = Id;
			Room.Rect = R;
			Room.Theme = FName(ThemeNames[Rng.RandRange(0, 3)]);
			Room.Size = FName(*FString::Printf(TEXT("%dx%d"), R.NumX(), R.NumY()));
			Plan.Rooms.Add(Room);

			// 朝中庭的转角削成阶梯状 45° 斜面: 房间从方块变成 L 形
			const int32 InnerX = (R.X0 > Ax1) ? R.X0 : R.X1;
			const int32 InnerY = (R.Y0 > Ay1) ? R.Y0 : R.Y1;
			const int32 Sx = (InnerX == R.X0) ? 1 : -1;
			const int32 Sy = (InnerY == R.Y0) ? 1 : -1;
			const int32 MaxLevels = FMath::Clamp(FMath::Min(R.NumX(), R.NumY()) - 3, 1, 3);
			// 至少削两层, 否则缺口被中庭回廊吃掉, 看不出斜面
			const int32 Levels = (MaxLevels >= 2) ? Rng.RandRange(2, MaxLevels) : MaxLevels;
			for (int32 Level = 0; Level < Levels; ++Level)
			{
				for (int32 K = 0; K <= Level; ++K)
				{
					CarveRoomCell(Plan, InnerX + Sx * (Level - K), InnerY + Sy * K, Id);
				}
			}
		}

		// 中庭外圈留出 1 格宽的连续回廊带: 这样 +4 m 的回廊能绕着中庭走一圈,
		// 而不是被四角的房间切断。房间因此退到回廊后面。
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const int32 Dx = (X < Ax0) ? (Ax0 - X) : ((X > Ax1) ? (X - Ax1) : 0);
				const int32 Dy = (Y < Ay0) ? (Ay0 - Y) : ((Y > Ay1) ? (Y - Ay1) : 0);
				if (FMath::Max(Dx, Dy) != 1)
				{
					continue;
				}
				FArchCell& C = Plan.Cell(X, Y);
				if (C.Kind == EArchCellKind::Room)
				{
					C.Kind = EArchCellKind::Corridor;
					C.Space = INDEX_NONE;
				}
			}
		}

		// ---------- 3) 墙体: 只有房间画自己的墙 ----------
		// 大厅/中庭不画墙, 否则相邻两格会在同一条边上各画一片墙 (共面闪烁)。
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				C.WallMask = 0;
				C.Special[0] = C.Special[1] = C.Special[2] = C.Special[3] = 0;

				const bool bRoom = C.Kind == EArchCellKind::Room;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = X + DX[Side];
					const int32 NY = Y + DY[Side];
					if (!Plan.IsValid(NX, NY))
					{
						C.WallMask |= (1 << Side);
						continue;
					}
					const FArchCell& N = Plan.Cell(NX, NY);
					if (N.Kind == EArchCellKind::Void)
					{
						C.WallMask |= (1 << Side);
						continue;
					}
					if (bRoom && (N.Kind != EArchCellKind::Room || N.Space != C.Space))
					{
						C.WallMask |= (1 << Side);
					}
				}
			}
		}

		// ---------- 4) 2 格宽 (8 m) 门洞 ----------
		int32 TotalDoors = 0;
		for (FArchRoom& Room : Plan.Rooms)
		{
			const int32 Doors = PlaceWideDoors(Plan, Room.Rect, Room.Id, Rng, 3);
			TotalDoors += Doors;
			for (int32 Y = Room.Rect.Y0; Y <= Room.Rect.Y1 && Room.DoorSide == 0 && Room.DoorX == 0; ++Y)
			{
				for (int32 X = Room.Rect.X0; X <= Room.Rect.X1; ++X)
				{
					const FArchCell& C = Plan.Cell(X, Y);
					if (C.Kind != EArchCellKind::Room || C.Space != Room.Id)
					{
						continue;
					}
					for (int32 Side = 0; Side < 4; ++Side)
					{
						if (C.Special[Side] == TEXT('d'))
						{
							Room.DoorX = X;
							Room.DoorY = Y;
							Room.DoorSide = Side;
						}
					}
				}
			}
		}

		// ---------- 5) 回廊 / 天桥 / 楼梯 ----------
		auto DistToAtrium = [&](int32 X, int32 Y) -> int32
		{
			const int32 Dx = (X < Ax0) ? (Ax0 - X) : ((X > Ax1) ? (X - Ax1) : 0);
			const int32 Dy = (Y < Ay0) ? (Ay0 - Y) : ((Y > Ay1) ? (Y - Ay1) : 0);
			return FMath::Max(Dx, Dy);
		};

		// +4 m 回廊: 中庭外一圈
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				if (DistToAtrium(X, Y) != 1)
				{
					continue;
				}
				FArchCell& C = Plan.Cell(X, Y);
				if (C.Kind != EArchCellKind::Corridor)
				{
					continue;
				}
				C.UFloor = FName(TEXT("floor_plate"));
			}
		}

		// 天桥: 中庭正中 2-3 格宽, 十字交叉, 是全场的高点
		const int32 MidX = (Ax0 + Ax1) / 2;
		const int32 MidY = (Ay0 + Ay1) / 2;
		const int32 BridgeW = Rng.RandRange(2, 3);
		const int32 BridgeX0 = MidX - 1;
		const int32 BridgeY0 = MidY - 1;
		for (int32 Y = Ay0; Y <= Ay1; ++Y)
		{
			for (int32 X = BridgeX0; X < BridgeX0 + BridgeW; ++X)
			{
				if (Plan.IsValid(X, Y))
				{
					Plan.Cell(X, Y).UFloor = FName(TEXT("floor_plate_b"));
				}
			}
		}
		for (int32 X = Ax0; X <= Ax1; ++X)
		{
			for (int32 Y = BridgeY0; Y < BridgeY0 + BridgeW; ++Y)
			{
				if (Plan.IsValid(X, Y))
				{
					Plan.Cell(X, Y).UFloor = FName(TEXT("floor_plate_b"));
				}
			}
		}

		// 栏杆 / 女儿墙
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				if (C.UFloor.IsNone())
				{
					continue;
				}

				TArray<int32> VoidSides;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = X + DX[Side];
					const int32 NY = Y + DY[Side];
					if (!Plan.IsValid(NX, NY))
					{
						continue;
					}
					const FArchCell& N = Plan.Cell(NX, NY);
					// 只有挑空中庭要栏杆。临房间的一侧不加: 房间自己的整墙已经挡人,
					// 再加一片女儿墙既多余又会和相邻栏杆在转角处重合闪烁。
					if (N.Kind == EArchCellKind::Atrium && N.UFloor.IsNone())
					{
						VoidSides.Add(Side);
					}
				}
				for (int32 I = 0; I < VoidSides.Num() && I < 2; ++I)
				{
					FName Rail;
					FString Pat;
					if (FWFCArchGrammar::ResolveWallTile(0, VoidSides[I], TEXT('h'), Rng, Rail, Pat)
						&& Pat.Len() == 4 && Pat[VoidSides[I]] == TEXT('h'))
					{
						if (I == 0)
						{
							C.URailing = Rail;
						}
						else
						{
							C.URailing2 = Rail;
						}
					}
				}

				uint8 PerMask = 0;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if (IsPerimeterSide(Plan, X, Y, Side))
					{
						PerMask |= (1 << Side);
					}
				}
				if (PerMask != 0)
				{
					FName Tile;
					FString Pat;
					if (FWFCArchGrammar::ResolveWallTile(PerMask, INDEX_NONE, 0, Rng, Tile, Pat))
					{
						C.UWall = Tile;
					}
				}
			}
		}

		// 四向各一部 8 m 宽楼梯 (两格并排), 从大厅地面爬回廊
		auto AddStairPair = [&](int32 X, int32 Y, int32 Dir)
		{
			for (int32 K = 0; K < 2; ++K)
			{
				FArchStair S;
				S.Dir = Dir;
				S.X = (Dir == 0 || Dir == 2) ? (X + K) : X;
				S.Y = (Dir == 0 || Dir == 2) ? Y : (Y + K);
				S.Tile = FName(TEXT("stair_core"));
				if (Plan.IsValid(S.X, S.Y))
				{
					Plan.Stairs.Add(S);
				}
			}
		};
		// 南侧 (向上北行): 底 Ay0-3 -> 顶 Ay0-2, 最后一级正好与回廊楼板同高
		AddStairPair(MidX - 1, Ay0 - 3, 0);
		// 北侧 (向上南行): 底 Ay1+3 -> 顶 Ay1+2
		AddStairPair(MidX - 1, Ay1 + 3, 2);
		// 西侧 (向上东行): 底 Ax0-3 -> 顶 Ax0-2
		AddStairPair(Ax0 - 3, MidY - 1, 1);
		// 东侧 (向上西行): 底 Ax1+3 -> 顶 Ax1+2
		AddStairPair(Ax1 + 3, MidY - 1, 3);

		// 回廊外侧的柱廊: 立在离中庭 2 格的厅内, 不占回廊本身 (回廊要能走通)
		TArray<FIntPoint> StairCells;
		for (const FArchStair& S : Plan.Stairs)
		{
			StairCells.Add(FIntPoint(S.X, S.Y));
			StairCells.Add(FIntPoint(S.X + DX[S.Dir], S.Y + DY[S.Dir]));
		}
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				if (DistToAtrium(X, Y) != 2 || ((X + Y) % 2) != 0)
				{
					continue;
				}
				FArchCell& C = Plan.Cell(X, Y);
				if (C.Kind != EArchCellKind::Corridor || C.WallMask != 0 || HasAnySpecial(C))
				{
					continue;
				}
				if (StairCells.Contains(FIntPoint(X, Y)))
				{
					continue;
				}
				C.bColumn = true;
			}
		}

		// 广场象限: 铺一片柱林, 免得留出 24x24 m 的空地
		for (const FArchRect& R : Plazas)
		{
			for (int32 Y = R.Y0 + 1; Y <= R.Y1 - 1; Y += 2)
			{
				for (int32 X = R.X0 + 1; X <= R.X1 - 1; X += 2)
				{
					FArchCell& C = Plan.Cell(X, Y);
					if (C.Kind == EArchCellKind::Corridor && C.WallMask == 0 && !HasAnySpecial(C)
						&& !StairCells.Contains(FIntPoint(X, Y)))
					{
						C.bColumn = true;
					}
				}
			}
		}

		// 大房间内部也立柱 (列柱厅): 沿房间长边每两格一根, 跳过门口和房间正中
		for (const FArchRoom& Room : Plan.Rooms)
		{
			const FArchRect& R = Room.Rect;
			if (R.NumX() < 5 || R.NumY() < 5)
			{
				continue;
			}
			const bool bAlongX = R.NumX() >= R.NumY();
			const int32 Fixed = bAlongX ? ((R.Y0 + R.Y1) / 2) : ((R.X0 + R.X1) / 2);
			for (int32 Y = R.Y0; Y <= R.Y1; ++Y)
			{
				for (int32 X = R.X0; X <= R.X1; ++X)
				{
					const int32 Perp = bAlongX ? Y : X;
					const int32 Along = bAlongX ? X : Y;
					if (Perp != Fixed || (Along % 2) != 0)
					{
						continue;
					}
					FArchCell& C = Plan.Cell(X, Y);
					if (C.Kind != EArchCellKind::Room || C.Space != Room.Id)
					{
						continue;
					}
					if (C.WallMask != 0 || HasAnySpecial(C))
					{
						continue;
					}
					// 门口附近不立柱, 免得把 8 m 门洞堵住
					bool bNearDoor = false;
					for (int32 Side = 0; Side < 4 && !bNearDoor; ++Side)
					{
						const int32 NX = X + DX[Side];
						const int32 NY = Y + DY[Side];
						if (Plan.IsValid(NX, NY) && HasAnySpecial(Plan.Cell(NX, NY)))
						{
							bNearDoor = true;
						}
					}
					if (bNearDoor)
					{
						continue;
					}
					C.bColumn = true;
				}
			}
		}

		// ---------- 6) 地面 / 道具 ----------
		auto Hash = [](int32 X, int32 Y, int32 S)
		{
			return static_cast<uint32>(X * 73856093) ^ static_cast<uint32>(Y * 19349663)
				^ static_cast<uint32>(S * 83492791);
		};

		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				if (C.Kind == EArchCellKind::Void)
				{
					continue;
				}
				const uint32 Hsh = Hash(X, Y, Seed);
				C.CeilTile = NAME_None;

				// 楼梯网格自带底面并占满 2 格足迹, 下面再铺地板会共面闪烁
				if (StairCells.Contains(FIntPoint(X, Y)))
				{
					continue;
				}

				if (C.Kind == EArchCellKind::Room)
				{
					// 同一房间统一一种素色砖 (按 Space+Seed 二选一), 不再逐格随机
					C.FloorTile = (Hash(C.Space, 0, Seed) % 2 == 0)
						? FName(TEXT("floor_plain")) : FName(TEXT("floor_plain_b"));
				}
				else if (C.Kind == EArchCellKind::Atrium)
				{
					// 中庭地面统一素色, 把视觉焦点让给天桥与信标
					C.FloorTile = FName(TEXT("floor_plain_c"));
				}
				else
				{
					// 大厅/走廊统一素色板
					C.FloorTile = FName(TEXT("floor_plate"));
				}

				// 道具: 房间按主题摆 2-4 个; 大厅只在中庭边缘点缀
				if (HasAnySpecial(C) || C.bColumn || C.PropTile != NAME_None)
				{
					continue;
				}
				if (C.Kind == EArchCellKind::Room && Hsh % 100 < 10)
				{
					const TCHAR* Props[] = { TEXT("prop_crate"), TEXT("prop_console") };
					C.PropTile = FName(Props[Hsh % 2]);
				}
			}
		}

		// 中庭中心的仪式感布置: 四根信标
		const FIntPoint CenterProps[] =
		{
			FIntPoint(MidX, MidY + 1), FIntPoint(MidX + 1, MidY),
			FIntPoint(MidX, MidY - 1), FIntPoint(MidX - 1, MidY)
		};
		for (int32 I = 0; I < 4; ++I)
		{
			const FIntPoint& P = CenterProps[I];
			if (!Plan.IsValid(P.X, P.Y))
			{
				continue;
			}
			FArchCell& C = Plan.Cell(P.X, P.Y);
			if (C.Kind != EArchCellKind::Atrium || !C.UFloor.IsNone() || C.PropTile != NAME_None)
			{
				continue;
			}
			C.PropTile = FName(TEXT("prop_beacon"));
		}

		// ---------- 7) 解析成实际 kit 瓦片 ----------
		// 门/窗/栏杆可能没有精确匹配的墙板, ResolveWallTile 会退化成 p/o/w,
		// 所以要把实际落下的图案解析回来, 让墙体和碰撞保持一致。
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FArchCell& C = Plan.Cell(X, Y);
				if (C.bColumn)
				{
					C.WallTile = FName(TEXT("wall_oooo_column"));
					C.WallMask = 0;
					C.Special[0] = C.Special[1] = C.Special[2] = C.Special[3] = 0;
					continue;
				}

				// 界外/临 Void 的整面墙降为女儿墙 ('h'): 「漂浮沙盘」边缘
				if (!HasAnySpecial(C))
				{
					for (int32 Side = 0; Side < 4; ++Side)
					{
						if ((C.WallMask & (1 << Side)) == 0)
						{
							continue;
						}
						const int32 NX = X + DX[Side];
						const int32 NY = Y + DY[Side];
						if (Plan.IsValid(NX, NY) && Plan.Cell(NX, NY).Kind != EArchCellKind::Void)
						{
							continue;
						}
						C.WallMask &= ~(1 << Side);
						C.Special[Side] = TEXT('h');
						break;
					}
				}

				int32 SpecialSide = INDEX_NONE;
				TCHAR SpecialCode = 0;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					if (C.Special[Side])
					{
						SpecialSide = Side;
						SpecialCode = C.Special[Side];
						break;
					}
				}

				FName Tile;
				FString Pat;
				if (!FWFCArchGrammar::ResolveWallTile(
					C.WallMask, SpecialSide, SpecialCode, Rng, Tile, Pat))
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
					else if (Ch == TEXT('d') || Ch == TEXT('p')
						|| Ch == TEXT('n') || Ch == TEXT('h'))
					{
						C.Special[Side] = Ch;
					}
				}
			}
		}

		Plan.AisleWidth = 0;

		int32 DoorCells = 0;
		int32 RoomCells = 0;
		for (int32 I = 0; I < W * H; ++I)
		{
			const FArchCell& C = Plan.Cells[I];
			if (C.Kind == EArchCellKind::Room)
			{
				++RoomCells;
			}
			if (HasAnySpecial(C))
			{
				++DoorCells;
			}
		}

		UE_LOG(LogERWFC, Log,
			TEXT("[Arch] Blackshard seed=%d rooms=%d(%d cells) hall=%dx%d atrium=%dx%d doors=%d stairs=%d bridges=2."),
			Seed, Plan.Rooms.Num(), RoomCells, ArmW, ArmW,
			Ax1 - Ax0 + 1, Ay1 - Ay0 + 1, DoorCells, Plan.Stairs.Num());

		// 调试: ER.ArchDump 1 或命令行 -ArchDump 时把平面图打到日志
		if (CVarArchDump.GetValueOnGameThread() > 0
			|| ER::Debug::IsArchDump())
		{
			UE_LOG(LogERWFC, Log, TEXT("[ArchDump] 平面图 (Y 从北到南, X 从西到东):"));
			for (int32 Y = H - 1; Y >= 0; --Y)
			{
				FString Row;
				Row.Reserve(W);
				for (int32 X = 0; X < W; ++X)
				{
					const FArchCell& C = Plan.Cell(X, Y);
					TCHAR Ch = TEXT('.');
					if (C.Kind == EArchCellKind::Atrium)
					{
						Ch = C.UFloor.IsNone() ? TEXT(' ') : TEXT('=');
					}
					else if (C.Kind == EArchCellKind::Room)
					{
						Ch = HasAnySpecial(C) ? TEXT('D') : TEXT('#');
					}
					else if (C.bColumn)
					{
						Ch = TEXT('o');
					}
					else if (!C.UFloor.IsNone())
					{
						Ch = TEXT('~');
					}
					Row.AppendChar(Ch);
				}
				UE_LOG(LogERWFC, Log, TEXT("[ArchDump] %s"), *Row);
			}
			for (const FArchStair& S : Plan.Stairs)
			{
				UE_LOG(LogERWFC, Log, TEXT("[ArchDump] stair (%d,%d) dir=%d"), S.X, S.Y, S.Dir);
			}
		}
		return true;
	}

bool FWFCArchGrammar::Build(int32 Seed, FArchPlan& OutPlan)
{
	OutPlan.Init(GSize, GSize, Seed);
	FRandomStream Rng(Seed ^ 0x9E3779B9);

	// 布局风格:
	//   0 = 旧的「房间图」(10-12 个 2x2 小房间 + 1 格宽走廊)
	//   1 = 旧的「十字中庭」(四角房间 + 中庭 + 夹层)
	//   2 = Blackshard 大厅 (默认: 宽十字大厅 + 大房间 + 回廊天桥 + 纪念性楼梯)
	const int32 Layout = CVarArchLayout.GetValueOnGameThread();

	if (Layout == 0)
	{
		return BuildRoomGraphPlan(Seed, OutPlan, Rng);
	}
	if (Layout == 1)
	{
		// fall through to the legacy cross atrium below
	}
	else
	{
		return BuildBlackshardPlan(Seed, OutPlan, Rng);
	}

	// 1) Open hall: crossing + four arms + side aisles.
	// The aisle width is the main macro knob: 1..3 cells (4/8/12 m).
	// It changes the nave/transept width, the mezzanine ring and where the
	// stairs can land, while the fixed corner blocks keep the room grammar.
	const int32 AisleW = Rng.RandRange(1, 3);
	OutPlan.AisleWidth = AisleW;
	constexpr int32 C0 = 5;
	constexpr int32 C1 = 13;
	const int32 NaveX0 = C0 + AisleW;
	const int32 NaveX1 = C1 - AisleW;
	const int32 WestAisle0 = C0;
	const int32 WestAisle1 = C0 + AisleW - 1;
	const int32 EastAisle0 = C1 - AisleW + 1;
	const int32 EastAisle1 = C1;

	for (int32 Y = 5; Y <= 13; ++Y)
	{
		for (int32 X = 5; X <= 13; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, Y);
			C.Kind = EArchCellKind::Atrium;
			C.Space = 0;
		}
	}

	TArray<FHallDef> Halls;
	Halls.Add({ FArchRect(NaveX0, 0, NaveX1, 4), EArchCellKind::Nave });
	Halls.Add({ FArchRect(NaveX0, 14, NaveX1, 18), EArchCellKind::Apse });
	Halls.Add({ FArchRect(0, NaveX0, 4, NaveX1), EArchCellKind::Transept });
	Halls.Add({ FArchRect(14, NaveX0, 18, NaveX1), EArchCellKind::Transept });
	Halls.Add({ FArchRect(WestAisle0, 0, WestAisle1, 4), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(EastAisle0, 0, EastAisle1, 4), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(WestAisle0, 14, WestAisle1, 18), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(EastAisle0, 14, EastAisle1, 18), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(0, WestAisle0, 4, WestAisle1), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(0, EastAisle0, 4, EastAisle1), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(14, WestAisle0, 18, WestAisle1), EArchCellKind::Aisle });
	Halls.Add({ FArchRect(14, EastAisle0, 18, EastAisle1), EArchCellKind::Aisle });

	for (const FHallDef& Def : Halls)
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
				UE_LOG(LogERWFC, Warning, TEXT("[Arch] Room %d has no clean door cell."), Room.Id);
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
		// Random colonnade: ~1/4 of the potential bay columns are omitted,
		// which changes the rhythm of the hall from seed to seed.
		if (Rng.FRand() < 0.24f)
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

	// 10) Random mezzanine bridges across the open atrium. One or two rows of
	// the central void get an upper catwalk with railings and support columns;
	// the roof above stays open. This is the main seed-to-seed macro variation
	// on top of the fixed circulation backbone.
	auto AddBridge = [&](int32 RowY)
	{
		for (int32 X = 5; X <= 13; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, RowY);
			if (C.Kind != EArchCellKind::Atrium)
			{
				continue;
			}
			C.UFloor = FName(TEXT("floor_plate"));
			C.UCeil = NAME_None;

			FName Rail;
			FString RailPattern;
			if (ResolveWallTile(0, 0, TEXT('h'), Rng, Rail, RailPattern))
			{
				C.URailing = Rail;
			}
			if (ResolveWallTile(0, 2, TEXT('h'), Rng, Rail, RailPattern))
			{
				C.URailing2 = Rail;
			}
		}

		// Three support columns under the catwalk (the bridge is at +4 m).
		for (int32 X : { 5, 9, 13 })
		{
			FArchCell& C = OutPlan.Cell(X, RowY);
			C.WallTile = FName(TEXT("wall_oooo_column"));
			C.WallMask = 0;
			C.Special[0] = C.Special[1] = C.Special[2] = C.Special[3] = 0;
			C.bColumn = true;
			C.bUColumn = false;
		}

		// Small landings in the flanking rooms so the bridge has something to
		// land on even when the room itself was chosen as double-height.
		for (int32 EX : { 4, 14 })
		{
			FArchCell& E = OutPlan.Cell(EX, RowY);
			if (E.UFloor.IsNone())
			{
				E.UFloor = FName(TEXT("floor_plain"));
				E.UCeil = NAME_None;
			}
		}
	};

	const int32 BridgeA = Rng.RandRange(7, 11);
	AddBridge(BridgeA);
	if (Rng.FRand() < 0.45f)
	{
		const int32 BridgeB = Rng.RandRange(7, 11);
		if (BridgeB != BridgeA)
		{
			AddBridge(BridgeB);
		}
	}

	// Guard every upper slab edge that faces a double-height room / opening.
	// (Void edges were already handled in BuildUpper.)
	for (int32 Y = 0; Y < OutPlan.H; ++Y)
	{
		for (int32 X = 0; X < OutPlan.W; ++X)
		{
			FArchCell& C = OutPlan.Cell(X, Y);
			if (C.UFloor.IsNone())
			{
				continue;
			}
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = X + DX[Side];
				const int32 NY = Y + DY[Side];
				if (!OutPlan.IsValid(NX, NY))
				{
					continue;
				}
				const FArchCell& N = OutPlan.Cell(NX, NY);
				if (!N.UFloor.IsNone())
				{
					continue;
				}
				FName Rail;
				FString RailPattern;
				if (!FWFCArchGrammar::ResolveWallTile(0, Side, TEXT('h'), Rng, Rail, RailPattern))
				{
					continue;
				}
				if (C.URailing.IsNone())
				{
					C.URailing = Rail;
				}
				else if (C.URailing2.IsNone())
				{
					C.URailing2 = Rail;
				}
			}
		}
	}

	// 11) Two monumental stairs in the nave, one against each side mezzanine.
	// They occupy a 1x2 cell footprint (X,Y and Y+1 in world; Y-1/Y in plan),
	// rise 4 m, and land on the aisle cells at X6/X12, Y4. The L1 railing on
	// the landing edge is removed so the stair actually connects to the walkway.
	auto AddStair = [&](int32 X, int32 Y, int32 ClearX, int32 ClearY)
	{
		FArchStair S;
		S.X = X;
		S.Y = Y;
		S.Tile = FName(TEXT("stair_core"));
		OutPlan.Stairs.Add(S);

		if (OutPlan.IsValid(ClearX, ClearY))
		{
			FArchCell& C = OutPlan.Cell(ClearX, ClearY);
			C.URailing = NAME_None;
			C.URailing2 = NAME_None;
		}
	};
	AddStair(NaveX0, 3, NaveX0 - 1, 4);
	AddStair(NaveX1, 3, NaveX1 + 1, 4);

	int32 ColumnCount = 0;
	int32 BridgeCells = 0;
	for (const FArchCell& C : OutPlan.Cells)
	{
		ColumnCount += C.bColumn ? 1 : 0;
		if (C.Kind == EArchCellKind::Atrium && !C.UFloor.IsNone())
		{
			++BridgeCells;
		}
	}

	UE_LOG(LogERWFC, Log,
		TEXT("[Arch] Built plan seed=%d aisle=%d rooms=%d columns=%d bridges=%d stairs=%d."),
		Seed, OutPlan.AisleWidth, OutPlan.Rooms.Num(), ColumnCount, BridgeCells, OutPlan.Stairs.Num());
	return true;
}


