// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionTerrainGrid.h"

#include "Core/ERLog.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

void FLegionTerrainGrid::Build(UWorld* World, const FVector& InCenter, float InHalfExtent,
	float InCellSize, float InMaxWalkableSlopeDeg)
{
	Cells.Reset();
	GridWidth = 0;
	GridHeight = 0;

	if (!World || InHalfExtent <= 100.0f || InCellSize <= 10.0f) { return; }

	CellSize = InCellSize;
	const int32 SideCount = FMath::Clamp(FMath::CeilToInt((InHalfExtent * 2.0f) / CellSize), 16, 220);
	GridWidth = SideCount;
	GridHeight = SideCount;
	OriginXY = FVector(InCenter.X - (GridWidth * 0.5f) * CellSize, InCenter.Y - (GridHeight * 0.5f) * CellSize, 0.0f);

	const int32 Total = GridWidth * GridHeight;
	Cells.SetNum(Total);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LegionTerrainGridBuild), false);

	constexpr float TraceHalfZ = 25000.0f;
	for (int32 GY = 0; GY < GridHeight; ++GY)
	{
		for (int32 GX = 0; GX < GridWidth; ++GX)
		{
			const FVector W = GridToWorld(GX, GY);
			FHitResult Hit;
			if (World->LineTraceSingleByObjectType(Hit,
				FVector(W.X, W.Y, TraceHalfZ), FVector(W.X, W.Y, -TraceHalfZ), ObjParams, QueryParams))
			{
				FCell& C = Cells[IndexOf(GX, GY)];
				C.HeightZ = Hit.Location.Z;
				C.bValid = true;
			}
		}
	}

	// 计算坡度并标记可通行区域
	int32 WalkableCount = 0;
	for (int32 GY = 0; GY < GridHeight; ++GY)
	{
		for (int32 GX = 0; GX < GridWidth; ++GX)
		{
			FCell& C = Cells[IndexOf(GX, GY)];
			if (!C.bValid) { continue; }

			const int32 XL = FMath::Max(0, GX - 1);
			const int32 XR = FMath::Min(GridWidth - 1, GX + 1);
			const int32 YD = FMath::Max(0, GY - 1);
			const int32 YU = FMath::Min(GridHeight - 1, GY + 1);

			const FCell& CL = Cells[IndexOf(XL, GY)];
			const FCell& CR = Cells[IndexOf(XR, GY)];
			const FCell& CD = Cells[IndexOf(GX, YD)];
			const FCell& CU = Cells[IndexOf(GX, YU)];

			const float DZx = (CL.bValid && CR.bValid && XR != XL)
				? (CR.HeightZ - CL.HeightZ) / ((XR - XL) * CellSize) : 0.0f;
			const float DZy = (CD.bValid && CU.bValid && YU != YD)
				? (CU.HeightZ - CD.HeightZ) / ((YU - YD) * CellSize) : 0.0f;

			const float Grad = FMath::Sqrt(DZx * DZx + DZy * DZy);
			C.SlopeDeg = FMath::RadiansToDegrees(FMath::Atan(Grad));
			C.bWalkable = (C.SlopeDeg <= InMaxWalkableSlopeDeg);
			if (C.bWalkable) { ++WalkableCount; }
		}
	}

	UE_LOG(LogERLegion, Log, TEXT("[LegionGrid] Built %dx%d terrain grid (cell=%.0fcm, walkable=%d/%d)"),
		GridWidth, GridHeight, CellSize, WalkableCount, Total);
}

bool FLegionTerrainGrid::WorldToGrid(const FVector& Pos, int32& OutGX, int32& OutGY) const
{
	if (!IsBuilt()) { return false; }
	OutGX = FMath::FloorToInt((Pos.X - OriginXY.X) / CellSize);
	OutGY = FMath::FloorToInt((Pos.Y - OriginXY.Y) / CellSize);
	return OutGX >= 0 && OutGX < GridWidth && OutGY >= 0 && OutGY < GridHeight;
}

FVector FLegionTerrainGrid::GridToWorld(int32 GX, int32 GY) const
{
	const float WX = OriginXY.X + (GX + 0.5f) * CellSize;
	const float WY = OriginXY.Y + (GY + 0.5f) * CellSize;
	float WZ = 0.0f;
	if (GX >= 0 && GX < GridWidth && GY >= 0 && GY < GridHeight && Cells.IsValidIndex(IndexOf(GX, GY)))
	{
		WZ = Cells[IndexOf(GX, GY)].HeightZ;
	}
	return FVector(WX, WY, WZ);
}

bool FLegionTerrainGrid::SampleHeightAndSlope(const FVector& WorldPos, float& OutZ, float& OutSlopeDeg) const
{
	int32 GX = 0, GY = 0;
	if (!WorldToGrid(WorldPos, GX, GY)) { return false; }
	const FCell& C = Cells[IndexOf(GX, GY)];
	if (!C.bValid) { return false; }
	OutZ = C.HeightZ;
	OutSlopeDeg = C.SlopeDeg;
	return true;
}

bool FLegionTerrainGrid::IsWalkableWorld(const FVector& WorldPos) const
{
	int32 GX = 0, GY = 0;
	if (!WorldToGrid(WorldPos, GX, GY)) { return false; }
	return Cells[IndexOf(GX, GY)].bWalkable;
}

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
			for (int32 DX = -R; DX <= R; ++DX)
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
	GScore.Init(MAX_FLT, Total);
	Parent.Init(INDEX_NONE, Total);
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
			if (CandG < GScore[NIdx])
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

	TArray<FIntPoint> Smoothed;
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
			Smoothed.Add(GridPath[Furthest]);
			Anchor = Furthest;
		}
	}

	for (int32 i = 0; i < Smoothed.Num(); ++i)
	{
		OutWaypoints.Add(GridToWorld(Smoothed[i].X, Smoothed[i].Y));
	}
	if (OutWaypoints.Num() > 0 && Cells[GoalIdx].bWalkable)
	{
		OutWaypoints.Last() = FVector(GoalWorld.X, GoalWorld.Y, Cells[GoalIdx].HeightZ);
	}
	return OutWaypoints.Num() > 0;
}

float FLegionTerrainGrid::ComputeElevationMultiplier(float AttackerZ, float DefenderZ)
{
	const float DiffM = (AttackerZ - DefenderZ) / 100.0f; // 换算米
	// 每高 1 米约 +3% 输出, 上限 +35%, 下限 -20%
	return FMath::Clamp(1.0f + DiffM * 0.03f, 0.80f, 1.35f);
}

void FLegionTerrainGrid::BuildMinimapPixels(int32 SizePx, TArray<FColor>& OutPixels) const
{
	OutPixels.Reset();
	if (!IsBuilt() || SizePx <= 0) { return; }

	OutPixels.SetNumUninitialized(SizePx * SizePx);

	float MinZ = MAX_FLT;
	float MaxZ = -MAX_FLT;
	for (const FCell& C : Cells)
	{
		if (!C.bValid) { continue; }
		MinZ = FMath::Min(MinZ, C.HeightZ);
		MaxZ = FMath::Max(MaxZ, C.HeightZ);
	}
	if (MinZ >= MaxZ)
	{
		MinZ = -1000.0f;
		MaxZ = 1000.0f;
	}
	const float RangeZ = FMath::Max(100.0f, MaxZ - MinZ);

	auto SampleBilinear = [&](float U, float V, float& OutH, float& OutSlope, bool& OutWalk)
	{
		const float FX = FMath::Clamp(U * (float)GridWidth - 0.5f, 0.0f, (float)(GridWidth - 1));
		const float FY = FMath::Clamp(V * (float)GridHeight - 0.5f, 0.0f, (float)(GridHeight - 1));
		const int32 X0 = FMath::FloorToInt(FX);
		const int32 Y0 = FMath::FloorToInt(FY);
		const int32 X1 = FMath::Min(GridWidth - 1, X0 + 1);
		const int32 Y1 = FMath::Min(GridHeight - 1, Y0 + 1);
		const float TX = FX - (float)X0;
		const float TY = FY - (float)Y0;

		const FCell& C00 = Cells[IndexOf(X0, Y0)];
		const FCell& C10 = Cells[IndexOf(X1, Y0)];
		const FCell& C01 = Cells[IndexOf(X0, Y1)];
		const FCell& C11 = Cells[IndexOf(X1, Y1)];

		const float H0 = FMath::Lerp(C00.HeightZ, C10.HeightZ, TX);
		const float H1 = FMath::Lerp(C01.HeightZ, C11.HeightZ, TX);
		OutH = FMath::Lerp(H0, H1, TY);

		const float S0 = FMath::Lerp(C00.SlopeDeg, C10.SlopeDeg, TX);
		const float S1 = FMath::Lerp(C01.SlopeDeg, C11.SlopeDeg, TX);
		OutSlope = FMath::Lerp(S0, S1, TY);

		const int32 NX = FMath::RoundToInt(FX);
		const int32 NY = FMath::RoundToInt(FY);
		OutWalk = Cells[IndexOf(NX, NY)].bWalkable;
	};

	const float MinorStep = 500.0f;
	const float MajorStep = 2500.0f;
	const float InvPx = 1.0f / (float)SizePx;

	for (int32 PY = 0; PY < SizePx; ++PY)
	{
		// 主相机 Yaw=90° 面朝 +Y: 上=+Y(北), 左=+X, 右=-X
		const float V = 1.0f - ((float)PY + 0.5f) * InvPx;
		for (int32 PX = 0; PX < SizePx; ++PX)
		{
			const float U = 1.0f - ((float)PX + 0.5f) * InvPx;

			float H = 0.0f, Slope = 0.0f;
			bool bWalk = true;
			SampleBilinear(U, V, H, Slope, bWalk);

			float HL = H, HR = H, HD = H, HU = H, DummyS = 0.0f;
			bool DummyW = true;
			SampleBilinear(FMath::Min(1.0f, U + InvPx), V, HL, DummyS, DummyW);
			SampleBilinear(FMath::Max(0.0f, U - InvPx), V, HR, DummyS, DummyW);
			SampleBilinear(U, FMath::Max(0.0f, V - InvPx), HD, DummyS, DummyW);
			SampleBilinear(U, FMath::Min(1.0f, V + InvPx), HU, DummyS, DummyW);

			const float GradPx = FMath::Sqrt(FMath::Square((HR - HL) * 0.5f) + FMath::Square((HU - HD) * 0.5f));

			auto ContourMask = [&](float Step, float PxThickness) -> float
			{
				const float Val = H / Step;
				const float Fw = FMath::Max(GradPx / Step, 0.025f);
				const float Dist = FMath::Abs(FMath::Frac(Val - 0.5f) - 0.5f) / Fw;
				return 1.0f - FMath::Clamp(Dist / PxThickness, 0.0f, 1.0f);
			};

			const float MinorA = ContourMask(MinorStep, 0.95f);
			const float MajorA = ContourMask(MajorStep, 1.35f);

			const float NormH = FMath::Clamp((H - MinZ) / RangeZ, 0.0f, 1.0f);
			float Luma = FMath::Lerp(16.0f, 64.0f, NormH);
			if (!bWalk)
			{
				Luma *= 0.62f; // 陡崖略压暗, 小地图一眼可辨不可通行山脊
			}

			Luma = FMath::Lerp(Luma, 132.0f, MinorA * 0.68f);
			Luma = FMath::Lerp(Luma, 236.0f, MajorA * 0.92f);

			const uint8 V8 = (uint8)FMath::Clamp(FMath::RoundToInt(Luma), 0, 255);
			OutPixels[PY * SizePx + PX] = FColor(V8, V8, V8, 255);
		}
	}
}
