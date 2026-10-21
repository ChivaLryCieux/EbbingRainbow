// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styles/WFC3DArenaGenerator_SciFi01.h"
#include "WFC3DModuleLibrary.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Math/RandomStream.h"

namespace
{
	constexpr float SciFi01CellX = 400.0f;
	constexpr float SciFi01CellY = 400.0f;
	constexpr float SciFi01CellZ = 300.0f;

	// Wall mask side order: 0=+X, 1=+Y, 2=-X, 3=-Y.
	const FIntPoint GWallDir[4] = {
		FIntPoint(1, 0),
		FIntPoint(0, 1),
		FIntPoint(-1, 0),
		FIntPoint(0, -1)
	};
	const uint8 GWallBit[4] = { 1, 2, 4, 8 };
	const int32 GYawForSide[4] = { 0, 90, 180, 270 };

	struct FSciFi01Stair
	{
		int32 Cell = INDEX_NONE;
		int32 Yaw = 0;
		int32 OpenSide = 0;
	};
}

static bool GWFCSciFi01FlipWalls = false;
static FAutoConsoleVariableRef CVarWFCSciFi01FlipWalls(
	TEXT("wfc.SciFi.FlipWalls"),
	GWFCSciFi01FlipWalls,
	TEXT("Flip the visible side of one-sided SciFi walls by 180 degrees (0/1)."),
	ECVF_Default);

static int32 GWFCSciFi01CornerYaw = 180;
static FAutoConsoleVariableRef CVarWFCSciFi01CornerYaw(
	TEXT("wfc.SciFi.CornerYaw"),
	GWFCSciFi01CornerYaw,
	TEXT("Yaw of the 1m corner piece (0/90/180/270)."),
	ECVF_Default);

static float GWFCSciFi01CornerInset = 50.0f;
static FAutoConsoleVariableRef CVarWFCSciFi01CornerInset(
	TEXT("wfc.SciFi.CornerInset"),
	GWFCSciFi01CornerInset,
	TEXT("Move the 1m corner piece inward by this many cm."),
	ECVF_Default);

static bool GWFCSciFi01UseCornerPost = true;
static FAutoConsoleVariableRef CVarWFCSciFi01UseCornerPost(
	TEXT("wfc.SciFi.CornerPost"),
	GWFCSciFi01UseCornerPost,
	TEXT("Enable/disable the 1m corner piece (0/1)."),
	ECVF_Default);

static float GWFCSciFi01StairYaw = 0.0f;
static FAutoConsoleVariableRef CVarWFCSciFi01StairYaw(
	TEXT("wfc.SciFi.StairYaw"),
	GWFCSciFi01StairYaw,
	TEXT("Extra yaw applied to the stair mesh (use 180 if the stairs ascend the wrong way)."),
	ECVF_Default);

AWFC3DArenaGenerator_SciFi01::AWFC3DArenaGenerator_SciFi01()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWFC3DArenaGenerator_SciFi01::BuildModules(
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	FloorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SciFiInteriorBundle1/SciFiInterior01/Meshes/SM_Floor_A_4x4m.SM_Floor_A_4x4m"));
	WallMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SciFiInteriorBundle1/SciFiInterior01/Meshes/SM_Wall_A_4x3m.SM_Wall_A_4x3m"));
	CornerPostMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SciFiInteriorBundle1/SciFiInterior01/Meshes/SM_Corner_1x1x3m.SM_Corner_1x1x3m"));
	StairMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SciFiInteriorBundle1/SciFiInterior01/Meshes/SM_Stairs_A.SM_Stairs_A"));
	RailingMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SciFiInteriorBundle1/SciFiInterior01/Meshes/SM_Railing.SM_Railing"));
	ColumnMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SciFiInteriorBundle1/SciFiInterior01/Meshes/SM_Column_3x3x3m.SM_Column_3x3x3m"));
	TreeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/WFC_Modules/0/Meshes/Trees/SM_Fir_Tree_09.SM_Fir_Tree_09"));

	if (!FloorMesh || !WallMesh || !CornerPostMesh || !StairMesh || !RailingMesh || !ColumnMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC SciFi01] Missing required SciFiInterior01 meshes."));
		return false;
	}

	UWFC3DModuleLibrary* Library = NewObject<UWFC3DModuleLibrary>(this);
	Library->Modules.Empty();
	Library->CustomCompatibilityPairs.Empty();
	Library->bIncludeAirModule = false;

	// Vertical: upper solid slab/wall over lower open room.
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Solid_Ground")), FName(TEXT("Air")), true });
	// Horizontal: a wall can face a room or the outside void.
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Wall")), FName(TEXT("Room")), true });
	Library->CustomCompatibilityPairs.Add({ FName(TEXT("Wall")), FName(TEXT("Void")), true });

	const FBoxSphereBounds FloorBounds = FloorMesh->GetBounds();
	const FVector FloorLocation(
		-FloorBounds.Origin.X,
		-FloorBounds.Origin.Y,
		-(FloorBounds.Origin.Z + FloorBounds.BoxExtent.Z));

	const FBoxSphereBounds WallBounds = WallMesh->GetBounds();
	const bool bWallLongAxisX = WallBounds.BoxExtent.X >= WallBounds.BoxExtent.Y;
	const float WallLengthYaw = bWallLongAxisX ? 90.0f : 0.0f;
	const float WallFlipYaw = GWFCSciFi01FlipWalls ? 180.0f : 0.0f;
	const FRotator WallRot(0.0f, WallLengthYaw + WallFlipYaw, 0.0f);
	const FVector WallCenterOffset = WallRot.RotateVector(FVector(WallBounds.Origin.X, WallBounds.Origin.Y, 0.0f));

	FTransform WallTransform;
	WallTransform.SetLocation(FVector(
		SciFi01CellX * 0.5f - WallCenterOffset.X,
		-WallCenterOffset.Y,
		-(WallBounds.Origin.Z - WallBounds.BoxExtent.Z)));
	WallTransform.SetRotation(WallRot.Quaternion());
	WallTransform.SetScale3D(FVector::OneVector);

	const FBoxSphereBounds PostBounds = CornerPostMesh->GetBounds();
	const FRotator PostRot(0.0f, static_cast<float>(GWFCSciFi01CornerYaw), 0.0f);
	const FVector PostCenterOffset = PostRot.RotateVector(FVector(PostBounds.Origin.X, PostBounds.Origin.Y, 0.0f));
	FTransform PostTransform;
	PostTransform.SetLocation(FVector(
		SciFi01CellX * 0.5f - GWFCSciFi01CornerInset - PostCenterOffset.X,
		SciFi01CellY * 0.5f - GWFCSciFi01CornerInset - PostCenterOffset.Y,
		-(PostBounds.Origin.Z - PostBounds.BoxExtent.Z)));
	PostTransform.SetRotation(PostRot.Quaternion());
	PostTransform.SetScale3D(FVector::OneVector);

	const FBoxSphereBounds StairBounds = StairMesh->GetBounds();
	const FRotator StairRot(0.0f, GWFCSciFi01StairYaw, 0.0f);
	const FVector StairCenterOffset = StairRot.RotateVector(FVector(StairBounds.Origin.X, StairBounds.Origin.Y, 0.0f));
	FTransform StairTransform;
	StairTransform.SetLocation(FVector(
		-StairCenterOffset.X,
		-StairCenterOffset.Y,
		-(StairBounds.Origin.Z - StairBounds.BoxExtent.Z)));
	StairTransform.SetRotation(StairRot.Quaternion());
	StairTransform.SetScale3D(FVector::OneVector);

	// 0) Plain floor (mask 0).
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("SciFi01_Floor"));
		Mod.Mesh = FloorMesh;
		Mod.AdditionalOffset = FloorLocation;
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	// 1..15) Four-sided wall masks: one straight wall, corners, T-junctions, enclosures.
	const FTransform Yaw90(FRotator(0.0f, 90.0f, 0.0f));
	const FTransform Yaw180(FRotator(0.0f, 180.0f, 0.0f));
	const FTransform Yaw270(FRotator(0.0f, 270.0f, 0.0f));

	for (int32 Mask = 1; Mask < 16; ++Mask)
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(*FString::Printf(TEXT("SciFi01_WallMask_%d"), Mask));
		Mod.Mesh = FloorMesh;
		Mod.AdditionalOffset = FloorLocation;
		Mod.Weight = 1.0f;

		auto AddWallPart = [&Mod](UStaticMesh* Mesh, const FTransform& Transform)
		{
			FWFC3DMeshPart Part;
			Part.Mesh = Mesh;
			Part.RelativeTransform = Transform;
			Mod.AdditionalParts.Add(Part);
		};

		if (Mask & 1) { AddWallPart(WallMesh, WallTransform); }
		if (Mask & 2) { AddWallPart(WallMesh, Yaw90 * WallTransform); }
		if (Mask & 4) { AddWallPart(WallMesh, Yaw180 * WallTransform); }
		if (Mask & 8) { AddWallPart(WallMesh, Yaw270 * WallTransform); }

		if (GWFCSciFi01UseCornerPost)
		{
			if ((Mask & 1) && (Mask & 2)) { AddWallPart(CornerPostMesh, PostTransform); }
			if ((Mask & 2) && (Mask & 4)) { AddWallPart(CornerPostMesh, Yaw90 * PostTransform); }
			if ((Mask & 4) && (Mask & 8)) { AddWallPart(CornerPostMesh, Yaw180 * PostTransform); }
			if ((Mask & 8) && (Mask & 1)) { AddWallPart(CornerPostMesh, Yaw270 * PostTransform); }
		}

		Mod.SetSocket(EWFC3DDirection::PosX, (Mask & 1) ? FName(TEXT("Wall")) : FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, (Mask & 2) ? FName(TEXT("Wall")) : FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, (Mask & 4) ? FName(TEXT("Wall")) : FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, (Mask & 8) ? FName(TEXT("Wall")) : FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	// 16) Stair. Base ascends +X; variant yaw selects open direction.
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("SciFi01_Stair"));
		Mod.Mesh = FloorMesh;
		Mod.AdditionalOffset = FloorLocation;
		Mod.Weight = 1.0f;

		FWFC3DMeshPart StairPart;
		StairPart.Mesh = StairMesh;
		StairPart.RelativeTransform = StairTransform;
		Mod.AdditionalParts.Add(StairPart);

		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Stair_Shaft")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Solid_Ground")));
		Mod.bAutoGenerateRotations = true;
		Library->Modules.Add(Mod);
	}

	// 17) Upper stair well: no floor.
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("SciFi01_StairWell"));
		Mod.Mesh = nullptr;
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Stair_Shaft")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	// 18) Upper atrium over the hall: no floor.
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("SciFi01_Atrium"));
		Mod.Mesh = nullptr;
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Room")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Air")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Air")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	// 19) Outside void: no mesh; stacks vertically.
	{
		FWFC3DModuleDefinition Mod;
		Mod.ModuleId = FName(TEXT("SciFi01_Void"));
		Mod.Mesh = nullptr;
		Mod.Weight = 1.0f;
		Mod.SetSocket(EWFC3DDirection::PosX, FName(TEXT("Void")));
		Mod.SetSocket(EWFC3DDirection::NegX, FName(TEXT("Void")));
		Mod.SetSocket(EWFC3DDirection::PosY, FName(TEXT("Void")));
		Mod.SetSocket(EWFC3DDirection::NegY, FName(TEXT("Void")));
		Mod.SetSocket(EWFC3DDirection::PosZ, FName(TEXT("Void")));
		Mod.SetSocket(EWFC3DDirection::NegZ, FName(TEXT("Void")));
		Mod.bAutoGenerateRotations = false;
		Library->Modules.Add(Mod);
	}

	UE_LOG(LogTemp, Log, TEXT("[WFC SciFi01] Modules: floor + 15 wall masks + stair/well/atrium/void; wall flip=%d."),
		GWFCSciFi01FlipWalls ? 1 : 0);

	return Library->CompileLibrary(OutModules, OutCompatibility);
}

bool AWFC3DArenaGenerator_SciFi01::BuildConstraints(
	const FIntVector& GridSize,
	int32 Seed,
	TMap<int32, int32>& OutClamps)
{
	if (GridSize.Z != 2)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC SciFi01] Two-story layout expects Scale.Z = 2, got %d."), GridSize.Z);
		return false;
	}

	const int32 W = GridSize.X;
	const int32 H = GridSize.Y;
	if (W < 12 || H < 10)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC SciFi01] Need at least 12x10 cells, got %dx%d."), W, H);
		return false;
	}

	const int32 N = W * H;
	const int32 Margin = 2;
	const int32 MinX = Margin;
	const int32 MaxX = W - Margin - 1;
	const int32 MinY = Margin;
	const int32 MaxY = H - Margin - 1;
	if (MaxX - MinX < 4 || MaxY - MinY < 4)
	{
		return false;
	}

	auto Idx = [W](int32 X, int32 Y) { return Y * W + X; };
	auto InGrid = [W, H](int32 X, int32 Y) { return X >= 0 && X < W && Y >= 0 && Y < H; };
	auto InPlan = [MinX, MaxX, MinY, MaxY](int32 X, int32 Y)
	{
		return X >= MinX && X <= MaxX && Y >= MinY && Y <= MaxY;
	};
	auto OppositeSide = [](int32 Side) { return (Side + 2) % 4; };

	FRandomStream Rng(Seed ^ 0x9E3779B9);

	TArray<bool> Walk;
	Walk.SetNumZeroed(N);
	TArray<int32> Room;
	Room.Init(-1, N);
	TArray<uint8> Door;
	Door.Init(0, N);

	// --- 1) Winding corridor: random walk + branches ---
	int32 CX = (MinX + MaxX) / 2;
	int32 CY = (MinY + MaxY) / 2;
	Walk[Idx(CX, CY)] = true;

	const int32 MainSteps = FMath::Max(200, (MaxX - MinX + 1) * (MaxY - MinY + 1));
	for (int32 Step = 0; Step < MainSteps; ++Step)
	{
		const int32 Dir = Rng.RandRange(0, 3);
		const int32 NX = CX + GWallDir[Dir].X;
		const int32 NY = CY + GWallDir[Dir].Y;
		if (InPlan(NX, NY))
		{
			CX = NX;
			CY = NY;
		}
		Walk[Idx(CX, CY)] = true;
	}

	for (int32 Branch = 0; Branch < 3; ++Branch)
	{
		int32 BX = 0;
		int32 BY = 0;
		for (int32 Try = 0; Try < 64; ++Try)
		{
			const int32 Candidate = Rng.RandRange(0, N - 1);
			if (Walk[Candidate])
			{
				BX = Candidate % W;
				BY = Candidate / W;
				break;
			}
		}
		for (int32 Step = 0; Step < 90; ++Step)
		{
			const int32 Dir = Rng.RandRange(0, 3);
			const int32 NX = BX + GWallDir[Dir].X;
			const int32 NY = BY + GWallDir[Dir].Y;
			if (InPlan(NX, NY))
			{
				BX = NX;
				BY = NY;
			}
			Walk[Idx(BX, BY)] = true;
		}
	}

	// --- 2) Random rooms stamped on the corridor ---
	int32 NextRoomId = 0;
	for (int32 RoomIndex = 0; RoomIndex < 7; ++RoomIndex)
	{
		const int32 RW = Rng.RandRange(2, 4);
		const int32 RH = Rng.RandRange(2, 4);
		int32 SX = Rng.RandRange(MinX, FMath::Max(MinX, MaxX - RW + 1));
		int32 SY = Rng.RandRange(MinY, FMath::Max(MinY, MaxY - RH + 1));

		for (int32 Y = SY; Y < SY + RH; ++Y)
		{
			for (int32 X = SX; X < SX + RW; ++X)
			{
				if (InPlan(X, Y))
				{
					Walk[Idx(X, Y)] = true;
					Room[Idx(X, Y)] = NextRoomId;
				}
			}
		}
		++NextRoomId;
	}

	// --- 3) Central hall, always a 5x5 room near the middle ---
	const int32 HallSize = FMath::Min(5, FMath::Min(MaxX - MinX + 1, MaxY - MinY + 1));
	const int32 HallX = FMath::Clamp((MinX + MaxX + 1) / 2 - HallSize / 2, MinX, MaxX - HallSize + 1);
	const int32 HallY = FMath::Clamp((MinY + MaxY + 1) / 2 - HallSize / 2, MinY, MaxY - HallSize + 1);
	PlanHallRoomId = NextRoomId++;
	for (int32 Y = HallY; Y < HallY + HallSize; ++Y)
	{
		for (int32 X = HallX; X < HallX + HallSize; ++X)
		{
			Walk[Idx(X, Y)] = true;
			Room[Idx(X, Y)] = PlanHallRoomId;
		}
	}

	int32 WalkableCount = 0;
	for (int32 I = 0; I < N; ++I)
	{
		WalkableCount += Walk[I] ? 1 : 0;
	}
	if (WalkableCount < 60)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC SciFi01] Seed %d produced only %d walkable cells; retrying."), Seed, WalkableCount);
		return false;
	}

	// --- 4) Doors: connect each room to the corridor; fallback to a neighbouring room ---
	TArray<int32> RoomIds;
	for (int32 I = 0; I < N; ++I)
	{
		if (Room[I] >= 0 && !RoomIds.Contains(Room[I]))
		{
			RoomIds.Add(Room[I]);
		}
	}

	for (int32 RoomId : RoomIds)
	{
		TArray<TPair<int32, int32>> Candidates;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 Cell = Idx(X, Y);
				if (Room[Cell] != RoomId)
				{
					continue;
				}
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = X + GWallDir[Side].X;
					const int32 NY = Y + GWallDir[Side].Y;
					if (InGrid(NX, NY) && Walk[Idx(NX, NY)] && Room[Idx(NX, NY)] == -1)
					{
						Candidates.Add(TPair<int32, int32>(Cell, Side));
					}
				}
			}
		}

		// No corridor boundary: open into a neighbouring room instead.
		if (Candidates.Num() == 0)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const int32 Cell = Idx(X, Y);
					if (Room[Cell] != RoomId)
					{
						continue;
					}
					for (int32 Side = 0; Side < 4; ++Side)
					{
						const int32 NX = X + GWallDir[Side].X;
						const int32 NY = Y + GWallDir[Side].Y;
						if (InGrid(NX, NY) && Walk[Idx(NX, NY)])
						{
							Candidates.Add(TPair<int32, int32>(Cell, Side));
						}
					}
				}
			}
		}

		const int32 DoorCount = FMath::Min(2, Candidates.Num());
		for (int32 D = 0; D < DoorCount; ++D)
		{
			const int32 Pick = Rng.RandRange(0, Candidates.Num() - 1);
			const TPair<int32, int32> DoorSpec = Candidates[Pick];
			Candidates.RemoveAtSwap(Pick);

			const int32 Cell = DoorSpec.Key;
			const int32 Side = DoorSpec.Value;
			Door[Cell] |= GWallBit[Side];

			const int32 X = Cell % W;
			const int32 Y = Cell / W;
			const int32 NX = X + GWallDir[Side].X;
			const int32 NY = Y + GWallDir[Side].Y;
			if (InGrid(NX, NY))
			{
				const int32 Neighbor = Idx(NX, NY);
				if (Room[Neighbor] >= 0)
				{
					Door[Neighbor] |= GWallBit[OppositeSide(Side)];
				}
			}
		}
	}

	// --- 5) Ground wall masks ---
	TArray<uint8> GroundMask;
	GroundMask.Init(0, N);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const int32 Cell = Idx(X, Y);
			if (!Walk[Cell])
			{
				continue;
			}

			uint8 Mask = 0;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = X + GWallDir[Side].X;
				const int32 NY = Y + GWallDir[Side].Y;
				if (!InGrid(NX, NY) || !Walk[Idx(NX, NY)])
				{
					Mask |= GWallBit[Side];
					continue;
				}

				if (Room[Cell] >= 0)
				{
					const int32 NeighborRoom = Room[Idx(NX, NY)];
					if (NeighborRoom != Room[Cell])
					{
						if (NeighborRoom == -1)
						{
							if ((Door[Cell] & GWallBit[Side]) == 0)
							{
								Mask |= GWallBit[Side];
							}
						}
						else
						{
							Mask |= GWallBit[Side];
						}
					}
				}
			}
			GroundMask[Cell] = Mask;
		}
	}

	// --- 6) Exactly three stairs on corridor cells, all four neighbours walkable ---
	TArray<int32> StairCandidates;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 Cell = Idx(X, Y);
			if (!Walk[Cell] || Room[Cell] != -1)
			{
				continue;
			}

			bool bAllWalkable = true;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = X + GWallDir[Side].X;
				const int32 NY = Y + GWallDir[Side].Y;
				if (!InGrid(NX, NY) || !Walk[Idx(NX, NY)])
				{
					bAllWalkable = false;
					break;
				}
			}
			if (bAllWalkable)
			{
				StairCandidates.Add(Cell);
			}
		}
	}

	for (int32 I = StairCandidates.Num() - 1; I > 0; --I)
	{
		const int32 J = Rng.RandRange(0, I);
		StairCandidates.Swap(I, J);
	}

	PlanStairCells.Empty();
	PlanStairYaws.Empty();
	PlanStairOpenEdges.Empty();

	for (int32 Cell : StairCandidates)
	{
		const int32 X = Cell % W;
		const int32 Y = Cell / W;

		bool bSpaced = true;
		for (int32 Existing : PlanStairCells)
		{
			const int32 EX = Existing % W;
			const int32 EY = Existing / W;
			if (FMath::Abs(X - EX) + FMath::Abs(Y - EY) < 3)
			{
				bSpaced = false;
				break;
			}
		}
		if (!bSpaced)
		{
			continue;
		}

		TArray<int32> OpenSides;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const int32 NX = X + GWallDir[Side].X;
			const int32 NY = Y + GWallDir[Side].Y;
			if (!InGrid(NX, NY) || !Walk[Idx(NX, NY)] || Room[Idx(NX, NY)] != -1)
			{
				continue;
			}
			const int32 NeighborCell = Idx(NX, NY);
			if (PlanStairCells.Contains(NeighborCell))
			{
				continue;
			}
			OpenSides.Add(Side);
		}
		if (OpenSides.Num() == 0)
		{
			continue;
		}

		const int32 OpenSide = OpenSides[Rng.RandRange(0, OpenSides.Num() - 1)];
		PlanStairCells.Add(Cell);
		PlanStairOpenEdges.Add(OpenSide);
		PlanStairYaws.Add(GYawForSide[OpenSide]);

		if (PlanStairCells.Num() == 3)
		{
			break;
		}
	}

	if (PlanStairCells.Num() != 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC SciFi01] Seed %d could not place 3 stairs (%d); retrying."),
			Seed, PlanStairCells.Num());
		return false;
	}

	TArray<bool> IsStair;
	IsStair.SetNumZeroed(N);
	for (int32 Cell : PlanStairCells)
	{
		IsStair[Cell] = true;
	}

	// --- 7) Upper wall masks: outer void + room partitions, doors stay open ---
	TArray<uint8> UpperMask;
	UpperMask.Init(0, N);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const int32 Cell = Idx(X, Y);
			if (!Walk[Cell] || IsStair[Cell] || Room[Cell] == PlanHallRoomId)
			{
				continue;
			}

			uint8 Mask = 0;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = X + GWallDir[Side].X;
				const int32 NY = Y + GWallDir[Side].Y;
				if (!InGrid(NX, NY) || !Walk[Idx(NX, NY)])
				{
					Mask |= GWallBit[Side];
					continue;
				}

				if (Room[Cell] >= 0)
				{
					const int32 NeighborRoom = Room[Idx(NX, NY)];
					if (NeighborRoom != Room[Cell])
					{
						if (NeighborRoom == -1)
						{
							if ((Door[Cell] & GWallBit[Side]) == 0)
							{
								Mask |= GWallBit[Side];
							}
						}
						else
						{
							Mask |= GWallBit[Side];
						}
					}
				}
			}
			UpperMask[Cell] = Mask;
		}
	}

	// --- 8) Store the plan for PostProcess ---
	PlanWalkable = Walk;
	PlanRoomIds = Room;
	PlanUpperWallMasks = UpperMask;
	PlanWidth = W;
	PlanHeight = H;

	// --- 9) Resolve module indices and clamp every cell ---
	TArray<int32> WallMaskIndex;
	WallMaskIndex.SetNumZeroed(16);
	WallMaskIndex[0] = FindModuleIndex(FName(TEXT("SciFi01_Floor")));
	for (int32 Mask = 1; Mask < 16; ++Mask)
	{
		WallMaskIndex[Mask] = FindModuleIndex(
			FName(*FString::Printf(TEXT("SciFi01_WallMask_%d"), Mask)));
	}
	const int32 StairWellIndex = FindModuleIndex(FName(TEXT("SciFi01_StairWell")));
	const int32 AtriumIndex = FindModuleIndex(FName(TEXT("SciFi01_Atrium")));
	const int32 VoidIndex = FindModuleIndex(FName(TEXT("SciFi01_Void")));

	bool bModulesValid = StairWellIndex != INDEX_NONE && AtriumIndex != INDEX_NONE && VoidIndex != INDEX_NONE;
	for (int32 Mask = 0; Mask < 16; ++Mask)
	{
		bModulesValid = bModulesValid && WallMaskIndex[Mask] != INDEX_NONE;
	}
	if (!bModulesValid)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC SciFi01] Missing floor/wall-mask/well/atrium/void modules."));
		return false;
	}

	auto StairVariant = [&](int32 Yaw) -> int32
	{
		const int32 Index = FindModuleVariantIndex(FName(TEXT("SciFi01_Stair")), Yaw);
		return (Index != INDEX_NONE) ? Index : FindModuleIndex(FName(TEXT("SciFi01_Stair")));
	};

	for (int32 S = 0; S < PlanStairCells.Num(); ++S)
	{
		const int32 Cell = PlanStairCells[S];
		const int32 X = Cell % W;
		const int32 Y = Cell / W;

		AddClamp(OutClamps, X, Y, 0, StairVariant(PlanStairYaws[S]));
		AddClamp(OutClamps, X, Y, 1, StairWellIndex);
	}

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const int32 Cell = Idx(X, Y);
			if (IsStair[Cell])
			{
				continue;
			}

			if (!Walk[Cell])
			{
				AddClamp(OutClamps, X, Y, 0, VoidIndex);
				AddClamp(OutClamps, X, Y, 1, VoidIndex);
				continue;
			}

			const uint8 GroundValue = GroundMask[Cell];
			AddClamp(OutClamps, X, Y, 0, WallMaskIndex[GroundValue]);

			if (Room[Cell] == PlanHallRoomId)
			{
				AddClamp(OutClamps, X, Y, 1, AtriumIndex);
			}
			else
			{
				const uint8 UpperValue = UpperMask[Cell];
				AddClamp(OutClamps, X, Y, 1, WallMaskIndex[UpperValue]);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WFC SciFi01] Seed %d layout: %dx%d, walkable=%d, hallRoom=%d, stairs=%d at (%d,%d)/(%d,%d)/(%d,%d)."),
		Seed, W, H, WalkableCount, PlanHallRoomId, PlanStairCells.Num(),
		PlanStairCells[0] % W, PlanStairCells[0] / W,
		PlanStairCells[1] % W, PlanStairCells[1] / W,
		PlanStairCells[2] % W, PlanStairCells[2] / W);

	return true;
}

void AWFC3DArenaGenerator_SciFi01::PostProcess(int32 Seed)
{
	const FVector CellSize = GetCellWorldSize();
	if (PlanWidth < 12 || PlanHeight < 10 || PlanWalkable.Num() != PlanWidth * PlanHeight)
	{
		return;
	}

	const int32 W = PlanWidth;
	const int32 H = PlanHeight;
	auto Idx = [W](int32 X, int32 Y) { return Y * W + X; };
	auto InGrid = [W, H](int32 X, int32 Y) { return X >= 0 && X < W && Y >= 0 && Y < H; };

	TArray<int32> StairAt;
	StairAt.Init(INDEX_NONE, W * H);
	for (int32 S = 0; S < PlanStairCells.Num(); ++S)
	{
		StairAt[PlanStairCells[S]] = S;
	}

	const float UpperFloorZ = CellSize.Z;

	// --- Tree in the centre of the hall ---
	if (TreeMesh)
	{
		float SumX = 0.0f;
		float SumY = 0.0f;
		int32 Count = 0;
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const int32 Cell = Idx(X, Y);
				if (PlanRoomIds[Cell] == PlanHallRoomId)
				{
					SumX += (X + 0.5f) * CellSize.X;
					SumY += (Y + 0.5f) * CellSize.Y;
					++Count;
				}
			}
		}

		if (Count > 0)
		{
			const float CenterX = SumX / Count;
			const float CenterY = SumY / Count;
			const FBoxSphereBounds B = TreeMesh->GetBounds();
			const float Scale = FMath::Max(0.01f, TreeScale);
			const float Bottom = (B.Origin.Z - B.BoxExtent.Z) * Scale;
			const FVector Offset(B.Origin.X * Scale, B.Origin.Y * Scale, 0.0f);

			FTransform T;
			T.SetLocation(FVector(CenterX - Offset.X, CenterY - Offset.Y, -Bottom));
			T.SetRotation(FQuat::Identity);
			T.SetScale3D(FVector(Scale));
			if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(TreeMesh))
			{
				Comp->AddInstance(T, false);
			}
		}
	}

	// --- Four columns on corridor cells touching the hall, supporting the gallery ---
	if (ColumnMesh)
	{
		TArray<int32> ColumnCandidates;
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const int32 Cell = Idx(X, Y);
				if (!PlanWalkable[Cell] || PlanRoomIds[Cell] != -1 || StairAt[Cell] != INDEX_NONE)
				{
					continue;
				}

				bool bTouchesHall = false;
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NX = X + GWallDir[Side].X;
					const int32 NY = Y + GWallDir[Side].Y;
					if (InGrid(NX, NY) && PlanRoomIds[Idx(NX, NY)] == PlanHallRoomId)
					{
						bTouchesHall = true;
						break;
					}
				}
				if (bTouchesHall)
				{
					ColumnCandidates.Add(Cell);
				}
			}
		}

		const FBoxSphereBounds B = ColumnMesh->GetBounds();
		const float Bottom = B.Origin.Z - B.BoxExtent.Z;
		if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(ColumnMesh))
		{
			const int32 ColumnCount = FMath::Min(4, ColumnCandidates.Num());
			for (int32 I = 0; I < ColumnCount; ++I)
			{
				const int32 Cell = ColumnCandidates[I];
				const float CX = (Cell % W + 0.5f) * CellSize.X;
				const float CY = (Cell / W + 0.5f) * CellSize.Y;

				FTransform T;
				T.SetLocation(FVector(CX - B.Origin.X, CY - B.Origin.Y, -Bottom));
				T.SetRotation(FQuat::Identity);
				T.SetScale3D(FVector::OneVector);
				Comp->AddInstance(T, false);
			}
		}
	}

	// --- Railings on gallery edges facing the atrium / stair wells ---
	if (RailingMesh)
	{
		const FBoxSphereBounds RB = RailingMesh->GetBounds();
		const bool bRailingLongX = RB.BoxExtent.X >= RB.BoxExtent.Y;
		const float RailingAlongX = bRailingLongX ? 0.0f : 90.0f;
		const float RailingAlongY = bRailingLongX ? 90.0f : 0.0f;
		const float RailingBottom = RB.Origin.Z - RB.BoxExtent.Z;

		UInstancedStaticMeshComponent* RailComp = GetOrCreateInstancedComponent(RailingMesh);
		if (RailComp)
		{
			auto AddRailing = [&](const FVector& EdgeCenter, float Yaw)
			{
				const FRotator Rot(0.0f, Yaw, 0.0f);
				const FVector CenterOffset = Rot.RotateVector(FVector(RB.Origin.X, RB.Origin.Y, 0.0f));
				FTransform T;
				T.SetLocation(FVector(
					EdgeCenter.X - CenterOffset.X,
					EdgeCenter.Y - CenterOffset.Y,
					UpperFloorZ - RailingBottom));
				T.SetRotation(Rot.Quaternion());
				T.SetScale3D(FVector::OneVector);
				RailComp->AddInstance(T, false);
			};

			auto AddEdgeRailing = [&](int32 X, int32 Y, int32 Side)
			{
				const float CX = (X + 0.5f) * CellSize.X;
				const float CY = (Y + 0.5f) * CellSize.Y;
				switch (Side)
				{
				case 0:
					AddRailing(FVector((X + 1) * CellSize.X, CY, 0.0f), RailingAlongY);
					break;
				case 1:
					AddRailing(FVector(CX, (Y + 1) * CellSize.Y, 0.0f), RailingAlongX);
					break;
				case 2:
					AddRailing(FVector(X * CellSize.X, CY, 0.0f), RailingAlongY);
					break;
				case 3:
					AddRailing(FVector(CX, Y * CellSize.Y, 0.0f), RailingAlongX);
					break;
				default:
					break;
				}
			};

			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					const int32 Cell = Idx(X, Y);
					if (!PlanWalkable[Cell] || StairAt[Cell] != INDEX_NONE || PlanRoomIds[Cell] == PlanHallRoomId)
					{
						continue;
					}

					const uint8 Mask = PlanUpperWallMasks[Cell];
					for (int32 Side = 0; Side < 4; ++Side)
					{
						const int32 NX = X + GWallDir[Side].X;
						const int32 NY = Y + GWallDir[Side].Y;
						if (!InGrid(NX, NY))
						{
							continue;
						}

						const int32 Neighbor = Idx(NX, NY);
						const bool bNeighborHall = (PlanRoomIds[Neighbor] == PlanHallRoomId);
						const int32 NeighborStair = StairAt[Neighbor];
						if (!bNeighborHall && NeighborStair == INDEX_NONE)
						{
							continue;
						}

						// A real upper wall already closes this edge.
						if ((Mask & GWallBit[Side]) != 0)
						{
							continue;
						}

						if (NeighborStair != INDEX_NONE)
						{
							// The stair well is open toward its ascent side; no rail there.
							const int32 OpenSide = PlanStairOpenEdges[NeighborStair];
							if (OpenSide == (Side + 2) % 4)
							{
								continue;
							}
						}

						AddEdgeRailing(X, Y, Side);
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WFC SciFi01] PostProcess: random hall tree, gallery columns and railings."));
}

FVector AWFC3DArenaGenerator_SciFi01::GetCellWorldSize() const
{
	return FVector(SciFi01CellX, SciFi01CellY, SciFi01CellZ);
}
