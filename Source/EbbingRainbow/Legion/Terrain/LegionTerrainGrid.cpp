// Copyright Epic Games, Inc. All Rights Reserved.

#include "Legion/Terrain/LegionTerrainGrid.h"

#include "Async/ParallelFor.h"
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
			C.bBaseWalkable = (C.SlopeDeg <= InMaxWalkableSlopeDeg);
			C.bWalkable = C.bBaseWalkable;
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

float FLegionTerrainGrid::ComputeElevationMultiplier(float AttackerZ, float DefenderZ)
{
	const float DiffM = (AttackerZ - DefenderZ) / 100.0f; // 换算米
	// 每高 1 米约 +3% 输出, 上限 +35%, 下限 -20%
	return FMath::Clamp(1.0f + DiffM * 0.03f, 0.80f, 1.35f);
}

void FLegionTerrainGrid::ResetTrees()
{
	for (FCell& C : Cells)
	{
		C.TreeCount = 0;
		C.bWalkable = C.bBaseWalkable;
	}
}

void FLegionTerrainGrid::AddTreeInfluence(const FVector& TreeWorld, float Radius)
{
	if (!IsBuilt()) { return; }
	const int32 X0 = FMath::Max(0, FMath::FloorToInt((TreeWorld.X - Radius - OriginXY.X) / CellSize));
	const int32 X1 = FMath::Min(GridWidth - 1, FMath::FloorToInt((TreeWorld.X + Radius - OriginXY.X) / CellSize));
	const int32 Y0 = FMath::Max(0, FMath::FloorToInt((TreeWorld.Y - Radius - OriginXY.Y) / CellSize));
	const int32 Y1 = FMath::Min(GridHeight - 1, FMath::FloorToInt((TreeWorld.Y + Radius - OriginXY.Y) / CellSize));
	const float R2 = Radius * Radius;
	for (int32 GY = Y0; GY <= Y1; ++GY)
	{
		for (int32 GX = X0; GX <= X1; ++GX)
		{
			const float CX = OriginXY.X + (GX + 0.5f) * CellSize;
			const float CY = OriginXY.Y + (GY + 0.5f) * CellSize;
			if (FMath::Square(CX - TreeWorld.X) + FMath::Square(CY - TreeWorld.Y) <= R2)
			{
				FCell& C = Cells[IndexOf(GX, GY)];
				C.TreeCount = (uint8)FMath::Min<int32>(255, C.TreeCount + 1);
			}
		}
	}
}

void FLegionTerrainGrid::ApplyTreeObstacles(int32 BlockThreshold)
{
	for (FCell& C : Cells)
	{
		C.bWalkable = C.bBaseWalkable && (int32)C.TreeCount < BlockThreshold;
	}
}

int32 FLegionTerrainGrid::GetTreeCountWorld(const FVector& WorldPos) const
{
	int32 GX = 0, GY = 0;
	if (!WorldToGrid(WorldPos, GX, GY)) { return 0; }
	return Cells[IndexOf(GX, GY)].TreeCount;
}

float FLegionTerrainGrid::GetRangedCoverMultiplier(const FVector& WorldPos) const
{
	const int32 N = GetTreeCountWorld(WorldPos);
	if (N <= 0) { return 1.0f; }
	// 疏林 (1 冠) 减伤 12%, 林深 (2+ 冠) 减伤 25%
	return N == 1 ? 0.88f : 0.75f;
}

void FLegionTerrainGrid::GetTreeStats(int32& OutBlocked, int32& OutCovered, int32& OutTotal) const
{
	OutBlocked = 0;
	OutCovered = 0;
	OutTotal = 0;
	for (const FCell& C : Cells)
	{
		if (!C.bValid) { continue; }
		++OutTotal;
		if (C.TreeCount > 0) { ++OutCovered; }
		if (C.bBaseWalkable && !C.bWalkable) { ++OutBlocked; }
	}
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
		OutWalk = Cells[IndexOf(NX, NY)].bBaseWalkable;
	};

	// 林地覆盖度 (0..1): 树冠数双线性平滑, 2 冠即视为满林
	auto SampleForest = [&](float U, float V) -> float
	{
		const float FX = FMath::Clamp(U * (float)GridWidth - 0.5f, 0.0f, (float)(GridWidth - 1));
		const float FY = FMath::Clamp(V * (float)GridHeight - 0.5f, 0.0f, (float)(GridHeight - 1));
		const int32 X0 = FMath::FloorToInt(FX);
		const int32 Y0 = FMath::FloorToInt(FY);
		const int32 X1 = FMath::Min(GridWidth - 1, X0 + 1);
		const int32 Y1 = FMath::Min(GridHeight - 1, Y0 + 1);
		const float TX = FX - (float)X0;
		const float TY = FY - (float)Y0;
		const float T0 = FMath::Lerp((float)Cells[IndexOf(X0, Y0)].TreeCount, (float)Cells[IndexOf(X1, Y0)].TreeCount, TX);
		const float T1 = FMath::Lerp((float)Cells[IndexOf(X0, Y1)].TreeCount, (float)Cells[IndexOf(X1, Y1)].TreeCount, TX);
		return FMath::Clamp(FMath::Lerp(T0, T1, TY) * 0.5f, 0.0f, 1.0f);
	};

	const float MinorStep = 500.0f;
	const float MajorStep = 2500.0f;
	const float InvPx = 1.0f / (float)SizePx;
	// 高分辨率纹理放粗线宽, 使屏幕上的等高线粗细与 1536px 版本一致
	const float ThickScale = FMath::Max(1.0f, (float)SizePx / 1536.0f);

	ParallelFor(SizePx, [&](int32 PY)
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
				// 平缓处的最小带宽随 ThickScale 反向缩小, 保证高度带宽度不随纹理分辨率变化
				const float Fw = FMath::Max(GradPx / Step, 0.025f / ThickScale);
				const float Dist = FMath::Abs(FMath::Frac(Val - 0.5f) - 0.5f) / Fw;
				return 1.0f - FMath::Clamp(Dist / PxThickness, 0.0f, 1.0f);
			};

			const float MinorA = ContourMask(MinorStep, 0.95f * ThickScale);
			const float MajorA = ContourMask(MajorStep, 1.35f * ThickScale);

			const float NormH = FMath::Clamp((H - MinZ) / RangeZ, 0.0f, 1.0f);
			float Luma = FMath::Lerp(16.0f, 64.0f, NormH);
			if (!bWalk)
			{
				Luma *= 0.62f; // 陡崖略压暗, 小地图一眼可辨不可通行山脊
			}

			// 林地: 压暗 + 45° 斜纹阴影线, 战略图上一眼可辨密林/疏林
			const float Forest = SampleForest(U, V);
			if (Forest > 0.05f)
			{
				Luma *= FMath::Lerp(1.0f, 0.55f, Forest);
				const int32 Period = FMath::Max(5, SizePx / 128);
				if (((PX + PY) % Period) * 10 < Period * 3) { Luma = FMath::Lerp(Luma, 92.0f, 0.55f * Forest); }
			}

			Luma = FMath::Lerp(Luma, 132.0f, MinorA * 0.68f);
			Luma = FMath::Lerp(Luma, 236.0f, MajorA * 0.92f);

			const uint8 V8 = (uint8)FMath::Clamp(FMath::RoundToInt(Luma), 0, 255);
			OutPixels[PY * SizePx + PX] = FColor(V8, V8, V8, 255);
		}
	});
}
