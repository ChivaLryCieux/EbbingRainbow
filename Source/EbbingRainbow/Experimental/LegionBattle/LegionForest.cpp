// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionForest.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Core/ERLog.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Experimental/LegionBattle/LegionTerrainGrid.h"

namespace
{
	const TCHAR* GTreePaths[] = {
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_01.SM_Fir_Tree_01"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_02.SM_Fir_Tree_02"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_03.SM_Fir_Tree_03"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_04.SM_Fir_Tree_04"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_05.SM_Fir_Tree_05"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_06.SM_Fir_Tree_06"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_07.SM_Fir_Tree_07"),
		TEXT("/Game/Fishermans_Cabin/Meshes/Foliage/Tree/SM_Fir_Tree_08.SM_Fir_Tree_08"),
	};
}

ALegionForest::ALegionForest()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ALegionForest::ClearTrees()
{
	for (UHierarchicalInstancedStaticMeshComponent* C : TreeComps)
	{
		if (C) { C->DestroyComponent(); }
	}
	TreeComps.Reset();
	TreeCount = 0;
}

int32 ALegionForest::Generate(UWorld* World, FLegionTerrainGrid& Grid, int32 Seed, float DensityScale,
	const TArray<FBox2D>& ExclusionRects)
{
	ClearTrees();
	Grid.ResetTrees();
	if (!World || !Grid.IsBuilt() || DensityScale <= 0.0f) { return 0; }

	if (TreeMeshes.Num() == 0)
	{
		for (const TCHAR* P : GTreePaths)
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, P)) { TreeMeshes.Add(M); }
		}
	}
	if (TreeMeshes.Num() == 0)
	{
		UE_LOG(LogERLegion, Warning, TEXT("[Forest] no tree meshes found, skip"));
		return 0;
	}

	const FVector Center = Grid.GetGridCenter();
	const float Half = Grid.GetWorldSpan() * 0.5f;
	const int32 N = FMath::Max(1, FMath::FloorToInt(Half * 2.0f / CandidateSpacing));
	const float Margin = 600.0f;

	FRandomStream Rng(Seed);
	const FVector2D NoiseOff(Rng.FRandRange(-5000.0f, 5000.0f), Rng.FRandRange(-5000.0f, 5000.0f));
	const float InvWave = 1.0f / FMath::Max(1000.0f, ForestWavelength);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QP(SCENE_QUERY_STAT(LegionForestGround), false);

	TArray<TArray<FTransform>> PerMesh;
	PerMesh.SetNum(TreeMeshes.Num());
	TArray<FVector> TreePos;

	const float MinSlopeCos = FMath::Cos(FMath::DegreesToRadians(MaxTreeSlopeDeg));

	for (int32 GY = 0; GY < N; ++GY)
	{
		for (int32 GX = 0; GX < N; ++GX)
		{
			// 所有随机量无条件抽取: 改 DensityScale 只影响是否接受, 不改变其余树的位置/品种
			const float JX = Rng.FRandRange(-0.45f, 0.45f);
			const float JY = Rng.FRandRange(-0.45f, 0.45f);
			const float Roll = Rng.FRand();
			const int32 MeshIdx = Rng.RandRange(0, TreeMeshes.Num() - 1);
			const float Scale = Rng.FRandRange(0.8f, 1.35f);
			const float ScaleXY = Rng.FRandRange(0.94f, 1.06f);
			const float Yaw = Rng.FRandRange(0.0f, 360.0f);
			const float TiltP = Rng.FRandRange(-3.0f, 3.0f);
			const float TiltR = Rng.FRandRange(-3.0f, 3.0f);

			const float X = Center.X - Half + ((float)GX + 0.5f + JX) * CandidateSpacing;
			const float Y = Center.Y - Half + ((float)GY + 0.5f + JY) * CandidateSpacing;
			if (FMath::Abs(X - Center.X) > Half - Margin || FMath::Abs(Y - Center.Y) > Half - Margin) { continue; }

			const FVector2D P2(X, Y);
			bool bExcluded = false;
			for (const FBox2D& R : ExclusionRects)
			{
				if (R.IsInside(P2)) { bExcluded = true; break; }
			}
			if (bExcluded) { continue; }

			// 大块噪声成林: 低频主轮廓 + 高频碎边
			const FVector2D NP = (P2 + NoiseOff) * InvWave;
			const float Noise = 0.72f * FMath::PerlinNoise2D(NP) + 0.28f * FMath::PerlinNoise2D(NP * 2.7f + FVector2D(31.4f, 17.7f));
			const float F = FMath::SmoothStep(ForestThreshold, ForestThreshold + 0.30f, Noise);
			const float Accept = FMath::Lerp(SparseChance, 1.0f, F) * DensityScale;
			if (Roll > Accept) { continue; }

			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, FVector(X, Y, 40000.0f), FVector(X, Y, -40000.0f), ObjParams, QP)) { continue; }
			if (!Hit.GetActor() || !Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Landscape"))) { continue; }
			if (Hit.ImpactNormal.Z < MinSlopeCos) { continue; }

			const FVector Loc = Hit.Location - FVector(0.0f, 0.0f, 8.0f);
			PerMesh[MeshIdx].Add(FTransform(FRotator(TiltP, Yaw, TiltR), Loc, FVector(Scale * ScaleXY, Scale * ScaleXY, Scale)));
			TreePos.Add(Loc);
		}
	}

	for (int32 i = 0; i < TreeMeshes.Num(); ++i)
	{
		if (PerMesh[i].Num() == 0) { continue; }
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		C->SetStaticMesh(TreeMeshes[i]);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(true);
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		C->AddInstances(PerMesh[i], false, true, false);
		TreeComps.Add(C);
	}

	for (const FVector& P : TreePos)
	{
		Grid.AddTreeInfluence(P, CrownRadius);
	}
	Grid.ApplyTreeObstacles(BlockThreshold);

	TreeCount = TreePos.Num();
	int32 Blocked = 0, Covered = 0, Total = 0;
	Grid.GetTreeStats(Blocked, Covered, Total);
	UE_LOG(LogERLegion, Log, TEXT("[Forest] seed=%d scale=%.2f trees=%d blockedCells=%d (%.1f%%) coveredCells=%d (%.1f%%)"),
		Seed, DensityScale, TreeCount, Blocked, 100.0f * Blocked / FMath::Max(1, Total),
		Covered, 100.0f * Covered / FMath::Max(1, Total));
	return TreeCount;
}
