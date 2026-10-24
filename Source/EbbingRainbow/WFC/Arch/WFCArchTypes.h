// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Macro-space kind of a single 4 m cell. */
enum class EArchCellKind : uint8
{
	Void = 0,
	Atrium,
	Nave,
	Apse,
	Transept,
	Aisle,
	Room,
	Corridor
};

/** Inclusive integer rectangle in plan space (X east, Y north). */
struct FArchRect
{
	int32 X0 = 0;
	int32 Y0 = 0;
	int32 X1 = 0;
	int32 Y1 = 0;

	FArchRect() = default;
	FArchRect(int32 InX0, int32 InY0, int32 InX1, int32 InY1)
		: X0(InX0), Y0(InY0), X1(InX1), Y1(InY1)
	{
	}

	int32 NumX() const { return X1 - X0 + 1; }
	int32 NumY() const { return Y1 - Y0 + 1; }
	bool Contains(int32 X, int32 Y) const { return X >= X0 && X <= X1 && Y >= Y0 && Y <= Y1; }
};

/** A room carved out of a corner block. */
struct FArchRoom
{
	int32 Id = INDEX_NONE;
	FArchRect Rect;
	int32 DoorX = 0;
	int32 DoorY = 0;
	int32 DoorSide = 0; // 0=N, 1=E, 2=S, 3=W
	FName Theme = NAME_None;
	FName Size = NAME_None; // 2x5 / 3x5 / 5x5
};

/** One cell of the two-level plan. */
struct FArchCell
{
	EArchCellKind Kind = EArchCellKind::Void;
	int32 Space = INDEX_NONE;

	/** Wall bits, 1=N, 2=E, 4=S, 8=W. Special sides are open and carry a code. */
	uint8 WallMask = 0;

	/** 0 = none, otherwise 'd','p','n','h'. */
	TCHAR Special[4] = { 0, 0, 0, 0 };

	FName WallTile = NAME_None;
	FName FloorTile = NAME_None;
	FName CeilTile = NAME_None;
	FName PropTile = NAME_None;
	bool bColumn = false;

	// Mezzanine layer (4 m above the lower floor).
	FName UFloor = NAME_None;
	FName UWall = NAME_None;
	FName UCeil = NAME_None;
	FName URailing = NAME_None;
	FName URailing2 = NAME_None;
	bool bUColumn = false;
};

/** A monumental stair core placed on the plan (occupies 1x2 cells). */
struct FArchStair
{
	int32 X = 0;
	int32 Y = 0;
	FName Tile = NAME_None;
};

/** Complete macro plan produced by the space grammar. */
struct FArchPlan
{
	int32 W = 19;
	int32 H = 19;
	int32 Seed = 0;
	int32 AisleWidth = 2;
	TArray<FArchCell> Cells;
	TArray<FArchRoom> Rooms;
	TArray<FArchStair> Stairs;

	void Init(int32 InW, int32 InH, int32 InSeed)
	{
		W = InW;
		H = InH;
		Seed = InSeed;
		Cells.Reset();
		Cells.SetNumZeroed(W * H);
		Rooms.Reset();
		Stairs.Reset();
	}

	FORCEINLINE int32 Index(int32 X, int32 Y) const { return Y * W + X; }
	FORCEINLINE bool IsValid(int32 X, int32 Y) const { return X >= 0 && X < W && Y >= 0 && Y < H; }
	FORCEINLINE FArchCell& Cell(int32 X, int32 Y) { return Cells[Index(X, Y)]; }
	FORCEINLINE const FArchCell& Cell(int32 X, int32 Y) const { return Cells[Index(X, Y)]; }

	FORCEINLINE bool IsVoid(int32 X, int32 Y) const
	{
		if (!IsValid(X, Y))
		{
			return true;
		}
		const EArchCellKind K = Cell(X, Y).Kind;
		return K == EArchCellKind::Atrium || K == EArchCellKind::Nave
			|| K == EArchCellKind::Apse || K == EArchCellKind::Transept;
	}
};
