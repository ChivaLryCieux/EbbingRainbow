// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_SpiralGarden.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"

AWFC3DArenaGenerator_SpiralGarden::AWFC3DArenaGenerator_SpiralGarden()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_SpiralGarden::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->AirSocketName = FName(TEXT("Air"));
	Library->bIncludeAirModule = true;
	Library->AirModuleWeight = 1.0f;
	Library->CustomCompatibilityPairs.Empty();
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Room")), FName(TEXT("Air")), true });

	UStaticMesh* CubeMesh = Cast<UStaticMesh>(
		StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/WFC_Modules/Cube.Cube")));

	if (!CubeMesh)
	{
		// WFC_Modules 目录被重新用于 Style 资产后，旧 Cube 可能已不在；
		// 用项目自带的 100cm SM_Cube 兜底，ComputedCubeEdge 会自动适配。
		CubeMesh = Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube")));
	}

	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC SpiralGarden] No cube mesh found (WFC_Modules/Cube or LevelPrototyping/SM_Cube)."));
		return false;
	}

	ComputedCubeEdge = 200.0f;
	{
		const FVector BoxExtent = CubeMesh->GetBounds().BoxExtent;
		const float MaxExtent = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
		if (MaxExtent > KINDA_SMALL_NUMBER)
		{
			ComputedCubeEdge = MaxExtent * 2.0f;
		}
	}

	// 装饰形态按 200cm 格校准，再随实际格宽等比缩放
	const float DecorUnit = ComputedCubeEdge / 200.0f;
	auto DO = [DecorUnit](float X, float Y, float Z)
	{
		return FVector(X * DecorUnit, Y * DecorUnit, Z * DecorUnit);
	};

	auto SetGroundSockets = [](FWFC3DModuleDefinition& Mod)
	{
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
	};

	auto AddCubeModule = [&](FName Id, FName PosZSocket, FName NegZSocket, float Weight)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = Id;
		Mod.Mesh = CubeMesh;
		Mod.Weight = Weight;
		SetGroundSockets(Mod);
		Mod.SetSocket(EWFC3DDirection::PosZ, PosZSocket);
		Mod.SetSocket(EWFC3DDirection::NegZ, NegZSocket);
		Library->Modules.Add(Mod);
	};

	// 1) 缎带地面
	AddCubeModule(FName(TEXT("Floor_Flat")), FName(TEXT("Air")), FName(TEXT("Solid_Ground")), 1.0f);

	// 2) 栏杆格
	AddCubeModule(FName(TEXT("Railing_Window")), FName(TEXT("Air")), FName(TEXT("Solid_Ground")), 1.0f);

	// 3) 螺旋爬升段
	AddCubeModule(FName(TEXT("Stair_Up")), FName(TEXT("Stair_Shaft")), FName(TEXT("Solid_Ground")), 1.0f);
	AddCubeModule(FName(TEXT("Stair_Well")), FName(TEXT("Air")), FName(TEXT("Stair_Shaft")), 1.0f);
	AddCubeModule(FName(TEXT("Ramp_Up")), FName(TEXT("Ramp_Top")), FName(TEXT("Solid_Ground")), 1.0f);
	AddCubeModule(FName(TEXT("Ramp_Well")), FName(TEXT("Air")), FName(TEXT("Ramp_Top")), 1.0f);

	// 4) 拱门（门）
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Gate_Arch"));
		Mod.Mesh = CubeMesh;
		Mod.Weight = 1.0f;
		SetGroundSockets(Mod);
		Mod.bAutoGenerateRotations = true;

		FWFC3DMeshPart PillarA;
		PillarA.Mesh = CubeMesh;
		PillarA.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, -38.0f, 185.0f), FVector(0.14f, 0.14f, 0.85f));
		Mod.AdditionalParts.Add(PillarA);

		FWFC3DMeshPart PillarB;
		PillarB.Mesh = CubeMesh;
		PillarB.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 38.0f, 185.0f), FVector(0.14f, 0.14f, 0.85f));
		Mod.AdditionalParts.Add(PillarB);

		FWFC3DMeshPart Lintel;
		Lintel.Mesh = CubeMesh;
		Lintel.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 0.0f, 276.0f), FVector(0.18f, 0.86f, 0.14f));
		Mod.AdditionalParts.Add(Lintel);

		FWFC3DMeshPart Capstone;
		Capstone.Mesh = CubeMesh;
		Capstone.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 0.0f, 308.0f), FVector(0.22f, 0.22f, 0.18f));
		Mod.AdditionalParts.Add(Capstone);

		Library->Modules.Add(Mod);
	}

	// 5) 花坛
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("Flower_Bed"));
		Mod.Mesh = CubeMesh;
		Mod.Weight = 1.0f;
		SetGroundSockets(Mod);
		Mod.bAutoGenerateRotations = false;

		FWFC3DMeshPart Soil;
		Soil.Mesh = CubeMesh;
		Soil.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 0.0f, 113.0f), FVector(0.62f, 0.62f, 0.26f));
		Mod.AdditionalParts.Add(Soil);

		FWFC3DMeshPart RimA;
		RimA.Mesh = CubeMesh;
		RimA.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, -33.0f, 147.0f), FVector(0.66f, 0.07f, 0.14f));
		Mod.AdditionalParts.Add(RimA);

		FWFC3DMeshPart RimB;
		RimB.Mesh = CubeMesh;
		RimB.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(0.0f, 33.0f, 147.0f), FVector(0.66f, 0.07f, 0.14f));
		Mod.AdditionalParts.Add(RimB);

		FWFC3DMeshPart RimC;
		RimC.Mesh = CubeMesh;
		RimC.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(-33.0f, 0.0f, 147.0f), FVector(0.07f, 0.66f, 0.14f));
		Mod.AdditionalParts.Add(RimC);

		FWFC3DMeshPart RimD;
		RimD.Mesh = CubeMesh;
		RimD.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(33.0f, 0.0f, 147.0f), FVector(0.07f, 0.66f, 0.14f));
		Mod.AdditionalParts.Add(RimD);

		FWFC3DMeshPart BloomA;
		BloomA.Mesh = CubeMesh;
		BloomA.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(-16.0f, 12.0f, 163.0f), FVector(0.11f, 0.11f, 0.16f));
		Mod.AdditionalParts.Add(BloomA);

		FWFC3DMeshPart BloomB;
		BloomB.Mesh = CubeMesh;
		BloomB.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(14.0f, -14.0f, 161.0f), FVector(0.11f, 0.11f, 0.16f));
		Mod.AdditionalParts.Add(BloomB);

		FWFC3DMeshPart BloomC;
		BloomC.Mesh = CubeMesh;
		BloomC.RelativeTransform = FTransform(FRotator::ZeroRotator, DO(6.0f, 20.0f, 166.0f), FVector(0.11f, 0.11f, 0.16f));
		Mod.AdditionalParts.Add(BloomC);

		Library->Modules.Add(Mod);
	}

	return Library->CompileLibrary(OutModules, OutCompatibility);
}

bool AWFC3DArenaGenerator_SpiralGarden::BuildConstraints(
	const FIntVector& GridSize,
	int32 Seed,
	TMap<int32, int32>& OutClamps)
{
	const int32 FloorModuleIndex = FindModuleIndex(FName(TEXT("Floor_Flat")));
	const int32 AirModuleIndex = FindModuleIndex(FName(TEXT("Air")));
	const int32 RailingModuleIndex = FindModuleIndex(FName(TEXT("Railing_Window")));
	const int32 StairModuleIndex = FindModuleIndex(FName(TEXT("Stair_Up")));
	const int32 WellholeModuleIndex = FindModuleIndex(FName(TEXT("Stair_Well")));
	const int32 RampModuleIndex = FindModuleIndex(FName(TEXT("Ramp_Up")));
	const int32 LandingModuleIndex = FindModuleIndex(FName(TEXT("Ramp_Well")));
	const int32 GateVariantRot0 = FindModuleVariantIndex(FName(TEXT("Gate_Arch")), 0);
	const int32 GateVariantRot90 = FindModuleVariantIndex(FName(TEXT("Gate_Arch")), 90);
	const int32 PlanterModuleIndex = FindModuleIndex(FName(TEXT("Flower_Bed")));

	if (FloorModuleIndex == INDEX_NONE
		|| AirModuleIndex == INDEX_NONE
		|| RailingModuleIndex == INDEX_NONE
		|| StairModuleIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC SpiralGarden] Missing required ribbon modules (Floor/Railing/Stair/Air)."));
		return false;
	}

	const int32 GX = GridSize.X;
	const int32 GY = GridSize.Y;
	const int32 GZ = GridSize.Z;
	if (GZ <= 0)
	{
		return true;
	}

	const float CX = (GX - 1) * 0.5f;
	const float CY = (GY - 1) * 0.5f;

	const float RIn = static_cast<float>(SpiralRibbonInnerRadius);
	const float ROut = static_cast<float>(SpiralRibbonOuterRadius);
	const float ArcWidthDeg = SpiralTwistPerLayer + SpiralArcOverlapDeg;
	const float WidenExtent = 5.0f;
	const float CutDeg = 4.0f;
	const float EdgeBandDeg = 4.0f;

	struct FRibbonLayer
	{
		float StartDeg = 0.0f;
		float CenterDeg = 0.0f;
		float OuterThis = 0.0f;
	};

	FRandomStream LayoutRnd(Seed);
	TArray<FRibbonLayer> RibbonLayers;
	RibbonLayers.SetNum(GZ);
	for (int32 Z = 0; Z < GZ; ++Z)
	{
		RibbonLayers[Z].StartDeg = Z * SpiralTwistPerLayer + LayoutRnd.FRandRange(-EdgeBandDeg, EdgeBandDeg);
		RibbonLayers[Z].CenterDeg = RibbonLayers[Z].StartDeg + ArcWidthDeg * 0.5f;
		RibbonLayers[Z].OuterThis = (LayoutRnd.FRand() < SpiralRibbonWidenChance) ? (ROut + WidenExtent) : ROut;
	}

	auto InArcWindow = [](float AngleDeg, float CenterDeg, float WidthDeg) -> bool
	{
		return FMath::Abs(FMath::FindDeltaAngleDegrees(AngleDeg, CenterDeg)) <= WidthDeg * 0.5f;
	};

	auto IsCoreCell = [&](int32 X, int32 Y, const FRibbonLayer& L) -> bool
	{
		const float Dx = X - CX;
		const float Dy = Y - CY;
		const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
		if (R < RIn || R > L.OuterThis)
		{
			return false;
		}
		const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
		if (!InArcWindow(Ang, L.CenterDeg, ArcWidthDeg))
		{
			return false;
		}
		if (R <= RIn + 0.7f || R >= L.OuterThis - 0.7f)
		{
			return false;
		}
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg)) <= CutDeg ||
			FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg + ArcWidthDeg)) <= CutDeg)
		{
			return false;
		}
		return true;
	};

	int32 ClampedVoid = 0;
	int32 ClampedRailing = 0;
	int32 ClampedWiden = 0;
	int32 ClampedStair = 0;
	int32 ClampedRamp = 0;

	// 1) 虚空：缎带之外的一切
	for (int32 Z = 0; Z < GZ; ++Z)
	{
		const FRibbonLayer& L = RibbonLayers[Z];
		for (int32 X = 0; X < GX; ++X)
		{
			for (int32 Y = 0; Y < GY; ++Y)
			{
				const float Dx = X - CX;
				const float Dy = Y - CY;
				const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
				bool bVoid = (R < RIn) || (R > L.OuterThis);
				if (!bVoid)
				{
					const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
					if (!InArcWindow(Ang, L.CenterDeg, ArcWidthDeg))
					{
						bVoid = true;
					}
				}
				if (bVoid)
				{
					AddClamp(OutClamps, X, Y, Z, AirModuleIndex);
					++ClampedVoid;
				}
			}
		}
	}

	// 2) 缎带栏杆：内缘 / 外缘 / 径向端头
	for (int32 Z = 0; Z < GZ; ++Z)
	{
		const FRibbonLayer& L = RibbonLayers[Z];
		for (int32 X = 0; X < GX; ++X)
		{
			for (int32 Y = 0; Y < GY; ++Y)
			{
				const float Dx = X - CX;
				const float Dy = Y - CY;
				const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
				if (R < RIn || R > L.OuterThis)
				{
					continue;
				}

				const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
				if (!InArcWindow(Ang, L.CenterDeg, ArcWidthDeg))
				{
					continue;
				}

				bool bRailing = false;
				if (R <= RIn + 0.7f)
				{
					bRailing = true;
				}
				else if (R >= L.OuterThis - 0.7f)
				{
					bRailing = true;
				}
				else if (FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg)) <= CutDeg ||
					FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, L.StartDeg + ArcWidthDeg)) <= CutDeg)
				{
					bRailing = true;
				}

				if (bRailing)
				{
					AddClamp(OutClamps, X, Y, Z, RailingModuleIndex);
					++ClampedRailing;
					if (R > ROut)
					{
						++ClampedWiden;
					}
				}
			}
		}
	}

	// 2.5) 缎带核心：全部由约束直接铺设为地面
	for (int32 Z = 0; Z < GZ; ++Z)
	{
		const FRibbonLayer& L = RibbonLayers[Z];
		for (int32 X = 0; X < GX; ++X)
		{
			for (int32 Y = 0; Y < GY; ++Y)
			{
				if (IsCoreCell(X, Y, L))
				{
					AddClamp(OutClamps, X, Y, Z, FloorModuleIndex);
				}
			}
		}
	}

	// 3) 层间爬升段：楼梯/坡道落在相邻层缎带重叠区
	struct FClimberSpot
	{
		FIntPoint Pos = FIntPoint::ZeroValue;
		int32 LowerZ = 0;
		int32 LowerModIdx = INDEX_NONE;
		int32 UpperModIdx = INDEX_NONE;
	};

	TArray<FClimberSpot> ClimberSpots;
	ClimberSpots.Reserve(GZ - 1);

	for (int32 Z = 0; Z < GZ - 1; ++Z)
	{
		const FRibbonLayer& Lz = RibbonLayers[Z];
		const FRibbonLayer& Lz1 = RibbonLayers[Z + 1];
		const float OverlapCenter = (Lz1.StartDeg + (Lz.StartDeg + ArcWidthDeg)) * 0.5f;

		FIntPoint BestPos(-1, -1);
		float BestDev = MAX_FLT;
		for (int32 X = 0; X < GX; ++X)
		{
			for (int32 Y = 0; Y < GY; ++Y)
			{
				if (!IsCoreCell(X, Y, Lz) || !IsCoreCell(X, Y, Lz1))
				{
					continue;
				}
				if (ClimberSpots.Num() > 0 && FIntPoint(X, Y) == ClimberSpots.Last().Pos)
				{
					continue;
				}
				const float Dx = X - CX;
				const float Dy = Y - CY;
				const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
				const float Dev = FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, OverlapCenter));
				if (Dev < BestDev)
				{
					BestDev = Dev;
					BestPos = FIntPoint(X, Y);
				}
			}
		}

		if (BestPos.X < 0)
		{
			continue;
		}

		FClimberSpot Spot;
		Spot.Pos = BestPos;
		Spot.LowerZ = Z;

		const bool bCanRamp = (RampModuleIndex != INDEX_NONE && LandingModuleIndex != INDEX_NONE);
		const bool bCanStair = (StairModuleIndex != INDEX_NONE && WellholeModuleIndex != INDEX_NONE);
		if (bCanStair && (!bCanRamp || LayoutRnd.FRand() < 0.55f))
		{
			Spot.LowerModIdx = StairModuleIndex;
			Spot.UpperModIdx = WellholeModuleIndex;
		}
		else if (bCanRamp)
		{
			Spot.LowerModIdx = RampModuleIndex;
			Spot.UpperModIdx = LandingModuleIndex;
		}

		ClimberSpots.Add(Spot);

		if (Spot.LowerModIdx == INDEX_NONE || Spot.UpperModIdx == INDEX_NONE)
		{
			continue;
		}

		AddClamp(OutClamps, Spot.Pos.X, Spot.Pos.Y, Spot.LowerZ, Spot.LowerModIdx);
		AddClamp(OutClamps, Spot.Pos.X, Spot.Pos.Y, Spot.LowerZ + 1, Spot.UpperModIdx);

		if (Spot.LowerModIdx == StairModuleIndex)
		{
			++ClampedStair;
		}
		else
		{
			++ClampedRamp;
		}
	}

	// 4) 概率点缀：拱门与花坛
	int32 DecorArchwayCount = 0;
	int32 DecorPlanterCount = 0;

	const bool bCanArchway = bGenerateSpiralArchways && (GateVariantRot0 != INDEX_NONE || GateVariantRot90 != INDEX_NONE);
	const bool bCanPlanter = bGenerateSpiralPlanters && (PlanterModuleIndex != INDEX_NONE);

	if (bCanArchway || bCanPlanter)
	{
		FRandomStream DecorRnd(Seed + 9091);

		auto GetClampedModule = [&](int32 X, int32 Y, int32 Z) -> int32
		{
			if (X < 0 || X >= GX || Y < 0 || Y >= GY || Z < 0 || Z >= GZ)
			{
				return INDEX_NONE;
			}
			const int32* Found = OutClamps.Find(Solver.CoordToIndex(X, Y, Z));
			return Found ? *Found : INDEX_NONE;
		};

		TArray<FIntVector> PlacedArchways;
		TArray<FIntVector> PlacedPlanters;

		for (int32 Z = 0; Z < GZ; ++Z)
		{
			for (int32 X = 0; X < GX; ++X)
			{
				for (int32 Y = 0; Y < GY; ++Y)
				{
					if (GetClampedModule(X, Y, Z) != FloorModuleIndex)
					{
						continue;
					}

					// 拱门
					if (bCanArchway && DecorRnd.FRand() < SpiralArchwayChance)
					{
						bool bTooCloseToArchway = false;
						for (const FIntVector& Placed : PlacedArchways)
						{
							if (FMath::Max3(
									FMath::Abs(Placed.X - X),
									FMath::Abs(Placed.Y - Y),
									FMath::Abs(Placed.Z - Z)) < SpiralArchwayMinSpacing)
							{
								bTooCloseToArchway = true;
								break;
							}
						}

						if (!bTooCloseToArchway)
						{
							const float Dx = X - CX;
							const float Dy = Y - CY;
							int32 ArchwayModule = INDEX_NONE;
							if (FMath::Abs(Dy) >= FMath::Abs(Dx))
							{
								ArchwayModule = (GateVariantRot0 != INDEX_NONE) ? GateVariantRot0 : GateVariantRot90;
							}
							else
							{
								ArchwayModule = (GateVariantRot90 != INDEX_NONE) ? GateVariantRot90 : GateVariantRot0;
							}

							if (ArchwayModule != INDEX_NONE)
							{
								AddClamp(OutClamps, X, Y, Z, ArchwayModule);
								PlacedArchways.Add(FIntVector(X, Y, Z));
								++DecorArchwayCount;
								continue;
							}
						}
					}

					// 花坛
					if (bCanPlanter)
					{
						const bool bAtRibbonEdge =
							GetClampedModule(X + 1, Y, Z) != FloorModuleIndex ||
							GetClampedModule(X - 1, Y, Z) != FloorModuleIndex ||
							GetClampedModule(X, Y + 1, Z) != FloorModuleIndex ||
							GetClampedModule(X, Y - 1, Z) != FloorModuleIndex;

						const float PlanterChance = bAtRibbonEdge ? SpiralPlanterEdgeChance : SpiralPlanterChance;
						if (DecorRnd.FRand() < PlanterChance)
						{
							bool bTooCloseToPlanter = false;
							for (const FIntVector& Placed : PlacedPlanters)
							{
								if (FMath::Max3(
										FMath::Abs(Placed.X - X),
										FMath::Abs(Placed.Y - Y),
										FMath::Abs(Placed.Z - Z)) < SpiralPlanterMinSpacing)
								{
									bTooCloseToPlanter = true;
									break;
								}
							}

							if (!bTooCloseToPlanter)
							{
								AddClamp(OutClamps, X, Y, Z, PlanterModuleIndex);
								PlacedPlanters.Add(FIntVector(X, Y, Z));
								++DecorPlanterCount;
							}
						}
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WFC SpiralGarden] Ribbon constraints: Void=%d, Railing=%d (widen %d), Stair=%d, Ramp=%d, Archway=%d, Planter=%d, TotalClamps=%d."),
		ClampedVoid, ClampedRailing, ClampedWiden, ClampedStair, ClampedRamp, DecorArchwayCount, DecorPlanterCount, OutClamps.Num());

	return true;
}

FVector AWFC3DArenaGenerator_SpiralGarden::GetCellWorldSize() const
{
	return FVector(ComputedCubeEdge, ComputedCubeEdge, ComputedCubeEdge);
}
