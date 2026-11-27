// Copyright Epic Games, Inc. All Rights Reserved.

#include "Legion/Terrain/LegionTerrainGrid.h"

#include "Algo/Reverse.h"

bool FLegionTerrainGrid::FindNearestWalkableCell(int32 StartGX, int32 StartGY, int32 MaxRadius,
	int32& OutGX, int32& OutGY) const
{
	StartGX = FMath::Clamp(StartGX, 0, GridWidth - 1);
	StartGY = FMath::Clamp(StartGY, 0, GridHeight - 1);
	if (Cells[IndexOf(StartGX, StartGY)].bWalkable)
	{
		OutGX = StartGX;
		OutGY = StartGY;
		return true;
	}

	float BestD2 = MAX_FLT;
	bool bFound = false;
	for (int32 R = 1; R <= MaxRadius; ++R)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			// Inner cells were all checked in earlier rings; preserve Y/X tie order.
			const int32 StepX = (DY == -R || DY == R) ? 1 : 2 * R;
			for (int32 DX = -R; DX <= R; DX += StepX)
			{
				const int32 X = StartGX + DX;
				const int32 Y = StartGY + DY;
				if (X < 0 || X >= GridWidth || Y < 0 || Y >= GridHeight) { continue; }
				if (!Cells[IndexOf(X, Y)].bWalkable) { continue; }
				const float D2 = (float)(DX * DX + DY * DY);
				if (D2 < BestD2)
				{
					BestD2 = D2;
					OutGX = X;
					OutGY = Y;
					bFound = true;
				}
			}
		}
		if (bFound) { return true; }
	}
	return false;
}

FVector FLegionTerrainGrid::ClampToWalkable(const FVector& WorldPos) const
{
	if (!IsBuilt()) { return WorldPos; }
	const int32 GX = FMath::Clamp(FMath::FloorToInt((WorldPos.X - OriginXY.X) / CellSize), 0, GridWidth - 1);
	const int32 GY = FMath::Clamp(FMath::FloorToInt((WorldPos.Y - OriginXY.Y) / CellSize), 0, GridHeight - 1);
	int32 Wx = GX, Wy = GY;
	if (FindNearestWalkableCell(GX, GY, 10, Wx, Wy))
	{
		return GridToWorld(Wx, Wy);
	}
	return WorldPos;
}

bool FLegionTerrainGrid::HasWalkableLineOfSight(int32 X0, int32 Y0, int32 X1, int32 Y1) const
{
	const int32 DX = FMath::Abs(X1 - X0);
	const int32 DY = FMath::Abs(Y1 - Y0);
	const int32 SX = (X0 < X1) ? 1 : -1;
	const int32 SY = (Y0 < Y1) ? 1 : -1;
	int32 Err = DX - DY;
	int32 X = X0;
	int32 Y = Y0;

	while (true)
	{
		if (X < 0 || X >= GridWidth || Y < 0 || Y >= GridHeight) { return false; }
		if (!Cells[IndexOf(X, Y)].bWalkable) { return false; }
		if (X == X1 && Y == Y1) { return true; }
		const int32 E2 = 2 * Err;
		if (E2 > -DY)
		{
			Err -= DY;
			X += SX;
		}
		if (E2 < DX)
		{
			Err += DX;
			Y += SY;
		}
	}
}

bool FLegionTerrainGrid::FindPath(const FVector& StartWorld, const FVector& GoalWorld, TArray<FVector>& OutWaypoints) const
{
	OutWaypoints.Reset();
	if (!IsBuilt())
	{
		OutWaypoints.Add(GoalWorld);
		return false;
	}

	int32 RawSX = FMath::Clamp(FMath::FloorToInt((StartWorld.X - OriginXY.X) / CellSize), 0, GridWidth - 1);
	int32 RawSY = FMath::Clamp(FMath::FloorToInt((StartWorld.Y - OriginXY.Y) / CellSize), 0, GridHeight - 1);
	int32 RawGX = FMath::Clamp(FMath::FloorToInt((GoalWorld.X - OriginXY.X) / CellSize), 0, GridWidth - 1);
	int32 RawGY = FMath::Clamp(FMath::FloorToInt((GoalWorld.Y - OriginXY.Y) / CellSize), 0, GridHeight - 1);

	int32 SX = RawSX, SY = RawSY, GX = RawGX, GY = RawGY;
	if (!FindNearestWalkableCell(RawSX, RawSY, 12, SX, SY) ||
		!FindNearestWalkableCell(RawGX, RawGY, 12, GX, GY))
	{
		OutWaypoints.Add(GoalWorld);
		return false;
	}

	if (SX == GX && SY == GY)
	{
		OutWaypoints.Add(GridToWorld(GX, GY));
		return true;
	}

	// 快速直线检测: 无悬崖阻挡直接走直线
	if (HasWalkableLineOfSight(SX, SY, GX, GY))
	{
		OutWaypoints.Add(FVector(GoalWorld.X, GoalWorld.Y, Cells[IndexOf(GX, GY)].HeightZ));
		return true;
	}

	const int32 Total = GridWidth * GridHeight;
	TArray<float> GScore;
	TArray<int32> Parent;
	TArray<uint8> State; // 0=unvisited, 1=open, 2=closed
	// State guards the scores; a node gets its parent when first queued.
	GScore.SetNumUninitialized(Total);
	Parent.SetNumUninitialized(Total);
	State.Init(0, Total);

	struct FHeapNode
	{
		int32 Idx;
		float F;
		bool operator<(const FHeapNode& Other) const { return F < Other.F; }
	};
	TArray<FHeapNode> Open;
	Open.Reserve(512);

	auto Heur = [GX, GY](int32 X, int32 Y) -> float
	{
		const float DX = (float)FMath::Abs(X - GX);
		const float DY = (float)FMath::Abs(Y - GY);
		return (DX + DY) + (1.41421356f - 2.0f) * FMath::Min(DX, DY);
	};

	const int32 StartIdx = IndexOf(SX, SY);
	const int32 GoalIdx = IndexOf(GX, GY);
	GScore[StartIdx] = 0.0f;
	Parent[StartIdx] = INDEX_NONE;
	State[StartIdx] = 1;
	Open.HeapPush(FHeapNode{ StartIdx, Heur(SX, SY) });

	static const int32 DX8[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int32 DY8[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	static const float Cost8[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.4142f, 1.4142f, 1.4142f, 1.4142f };

	bool bReached = false;
	int32 Iter = 0;
	constexpr int32 MaxIters = 18000;

	while (Open.Num() > 0 && Iter++ < MaxIters)
	{
		FHeapNode Cur;
		Open.HeapPop(Cur, EAllowShrinking::No);
		if (State[Cur.Idx] == 2) { continue; }
		State[Cur.Idx] = 2;

		if (Cur.Idx == GoalIdx)
		{
			bReached = true;
			break;
		}

		const int32 CX = Cur.Idx % GridWidth;
		const int32 CY = Cur.Idx / GridWidth;

		for (int32 k = 0; k < 8; ++k)
		{
			const int32 NX = CX + DX8[k];
			const int32 NY = CY + DY8[k];
			if (NX < 0 || NX >= GridWidth || NY < 0 || NY >= GridHeight) { continue; }
			const int32 NIdx = IndexOf(NX, NY);
			if (State[NIdx] == 2 || !Cells[NIdx].bWalkable) { continue; }

			// 禁止斜向穿悬崖角
			if (k >= 4)
			{
				if (!Cells[IndexOf(CX + DX8[k], CY)].bWalkable ||
					!Cells[IndexOf(CX, CY + DY8[k])].bWalkable)
				{
					continue;
				}
			}

			// 坡度代价加权: 平缓谷道优于陡坡擦边
			const float SlopePenalty = 1.0f + (Cells[NIdx].SlopeDeg / 45.0f) * 0.45f;
			const float CandG = GScore[Cur.Idx] + Cost8[k] * SlopePenalty;
			if (CandG < (State[NIdx] == 0 ? MAX_FLT : GScore[NIdx]))
			{
				GScore[NIdx] = CandG;
				Parent[NIdx] = Cur.Idx;
				State[NIdx] = 1;
				Open.HeapPush(FHeapNode{ NIdx, CandG + Heur(NX, NY) });
			}
		}
	}

	if (!bReached)
	{
		OutWaypoints.Add(GoalWorld);
		return false;
	}

	// 回溯栅格路径并做视线拉直 (String-pulling)
	TArray<FIntPoint> GridPath;
	for (int32 C = GoalIdx; C != INDEX_NONE; C = Parent[C])
	{
		GridPath.Add(FIntPoint(C % GridWidth, C / GridWidth));
	}
	Algo::Reverse(GridPath);

	if (GridPath.Num() > 0)
	{
		int32 Anchor = 0;
		while (Anchor < GridPath.Num() - 1)
		{
			int32 Furthest = Anchor + 1;
			for (int32 Probe = GridPath.Num() - 1; Probe > Anchor + 1; --Probe)
			{
				if (HasWalkableLineOfSight(GridPath[Anchor].X, GridPath[Anchor].Y, GridPath[Probe].X, GridPath[Probe].Y))
				{
					Furthest = Probe;
					break;
				}
			}
			const FIntPoint& Point = GridPath[Furthest];
			OutWaypoints.Add(GridToWorld(Point.X, Point.Y));
			Anchor = Furthest;
		}
	}

	if (OutWaypoints.Num() > 0 && Cells[GoalIdx].bWalkable)
	{
		OutWaypoints.Last() = FVector(GoalWorld.X, GoalWorld.Y, Cells[GoalIdx].HeightZ);
	}
	return OutWaypoints.Num() > 0;
}
