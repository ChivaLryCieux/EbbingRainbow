// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DModuleLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "HAL/PlatformTime.h"

AWFC3DArenaGeneratorBase::AWFC3DArenaGeneratorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

bool AWFC3DArenaGeneratorBase::Generate(const FWFCGenerationRequest& Request)
{
	LastResult = FWFCGenerationResult();
	ClearGenerated();

	if (Request.Scale.X <= 0 || Request.Scale.Y <= 0 || Request.Scale.Z <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] Generate aborted: invalid scale %s."), *Request.ToString());
		return false;
	}

	const int32 Seed = (Request.Seed != 0) ? Request.Seed : FMath::Rand();
	const double StartTime = FPlatformTime::Seconds();

	// 1) 编译本风格的模块与相容表
	TArray<FCompiledWFC3DModule> Modules;
	TArray<TArray<TBitArray<>>> Compatibility;
	if (!BuildModules(Modules, Compatibility) || Modules.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] Generate aborted: BuildModules failed (%s)."), *Request.ToString());
		LastResult.UsedSeed = Seed;
		LastResult.UsedScale = Request.Scale;
		LastResult.CellWorldSize = GetCellWorldSize();
		return false;
	}

	CompiledModules = MoveTemp(Modules);
	CompatibilityTable = MoveTemp(Compatibility);

	// 2) 初始化求解器
	Solver.Initialize(Request.Scale, CompiledModules, CompatibilityTable);

	// 3) 本风格的初始约束
	TMap<int32, int32> Clamps;
	if (!BuildConstraints(Request.Scale, Seed, Clamps))
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] Generate aborted: BuildConstraints failed (%s)."), *Request.ToString());
		LastResult.UsedSeed = Seed;
		LastResult.UsedScale = Request.Scale;
		LastResult.CellWorldSize = GetCellWorldSize();
		return false;
	}

	// 4) 求解
	const bool bSolved = Solver.Solve(MaxRetryAttempts, Seed, Clamps);
	if (!bSolved)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC] Generate failed: solver exhausted %d retries (%s, clamps=%d)."),
			MaxRetryAttempts, *Request.ToString(), Clamps.Num());
		LastResult.UsedSeed = Seed;
		LastResult.UsedScale = Request.Scale;
		LastResult.CellWorldSize = GetCellWorldSize();
		return false;
	}

	// 5) 后处理 + 渲染
	PostProcess(Seed);
	RenderResult();

	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	LastResult.bSuccess = true;
	LastResult.UsedSeed = Seed;
	LastResult.UsedScale = Request.Scale;
	LastResult.CellWorldSize = GetCellWorldSize();
	LastResult.ModuleCount = CompiledModules.Num();
	LastResult.CellCount = Request.Scale.X * Request.Scale.Y * Request.Scale.Z;
	LastResult.DurationMs = DurationMs;

	UE_LOG(LogTemp, Log, TEXT("[WFC] Generated %s in %.2f ms (modules=%d, cells=%d)."),
		*Request.ToString(), DurationMs, LastResult.ModuleCount, LastResult.CellCount);
	return true;
}

void AWFC3DArenaGeneratorBase::ClearGenerated()
{
	for (auto& Pair : InstancedMeshComponents)
	{
		if (Pair.Value && IsValid(Pair.Value))
		{
			Pair.Value->ClearInstances();
		}
	}
}

bool AWFC3DArenaGeneratorBase::CompileModuleLibrary(
	UWFC3DModuleLibrary* Library,
	TArray<FCompiledWFC3DModule>& OutModules,
	TArray<TArray<TBitArray<>>>& OutCompatibility)
{
	if (!Library)
	{
		return false;
	}
	return Library->CompileLibrary(OutModules, OutCompatibility);
}

int32 AWFC3DArenaGeneratorBase::FindModuleIndex(FName SourceModuleId) const
{
	int32 FirstMatch = INDEX_NONE;
	for (int32 ModIdx = 0; ModIdx < CompiledModules.Num(); ++ModIdx)
	{
		if (CompiledModules[ModIdx].SourceModuleId != SourceModuleId)
		{
			continue;
		}

		const FString VariantId = CompiledModules[ModIdx].VariantId.ToString();
		if (VariantId.EndsWith(TEXT("_Rot0")))
		{
			return ModIdx;
		}
		if (FirstMatch == INDEX_NONE)
		{
			FirstMatch = ModIdx;
		}
	}
	return FirstMatch;
}

int32 AWFC3DArenaGeneratorBase::FindModuleVariantIndex(FName SourceModuleId, int32 YawDegrees) const
{
	const FString Prefix = SourceModuleId.ToString();
	const FString Suffix = FString::Printf(TEXT("_Rot%d"), YawDegrees);

	for (int32 ModIdx = 0; ModIdx < CompiledModules.Num(); ++ModIdx)
	{
		if (CompiledModules[ModIdx].SourceModuleId != SourceModuleId)
		{
			continue;
		}

		const FString VariantId = CompiledModules[ModIdx].VariantId.ToString();
		if (VariantId.StartsWith(Prefix) && VariantId.EndsWith(Suffix))
		{
			return ModIdx;
		}
	}
	return INDEX_NONE;
}

void AWFC3DArenaGeneratorBase::RenderResult()
{
	ClearGenerated();

	const TArray<FWFCSlot3D>& Slots = Solver.GetSlots();
	const FVector CellSize = GetCellWorldSize();

	if (CellSize.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC] RenderResult aborted: CellWorldSize is zero."));
		return;
	}

	for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
	{
		const FWFCSlot3D& Slot = Slots[SlotIdx];
		if (!Slot.bCollapsed || Slot.FinalModuleIndex == INDEX_NONE)
		{
			continue;
		}

		if (!CompiledModules.IsValidIndex(Slot.FinalModuleIndex))
		{
			continue;
		}

		const FCompiledWFC3DModule& Mod = CompiledModules[Slot.FinalModuleIndex];
		const FIntVector Coord = Solver.IndexToCoord(SlotIdx);
		const FVector CellLocalPos(
			Coord.X * CellSize.X,
			Coord.Y * CellSize.Y,
			Coord.Z * CellSize.Z);

		// 主网格体
		if (UStaticMesh* Mesh = Mod.Mesh.Get())
		{
			if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(Mesh))
			{
				FTransform CellTransform = Mod.LocalTransform;
				CellTransform.AddToTranslation(CellLocalPos);
				Comp->AddInstance(CellTransform, /* bWorldSpace = */ false);
			}
		}

		// 复合部件（地板底座、转角墙、立柱等）
		for (const FCompiledWFC3DMeshPart& Part : Mod.AdditionalParts)
		{
			if (UStaticMesh* PartMesh = Part.Mesh.Get())
			{
				if (UInstancedStaticMeshComponent* Comp = GetOrCreateInstancedComponent(PartMesh))
				{
					FTransform PartTransform = Part.LocalTransform;
					PartTransform.AddToTranslation(CellLocalPos);
					Comp->AddInstance(PartTransform, /* bWorldSpace = */ false);
				}
			}
		}
	}
}

UInstancedStaticMeshComponent* AWFC3DArenaGeneratorBase::GetOrCreateInstancedComponent(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}

	if (TObjectPtr<UInstancedStaticMeshComponent>* FoundComp = InstancedMeshComponents.Find(Mesh))
	{
		if (*FoundComp && IsValid(*FoundComp))
		{
			return *FoundComp;
		}
	}

	UInstancedStaticMeshComponent* NewComp = nullptr;
	const FName CompName = *FString::Printf(TEXT("WFC_%s_%d"), *Mesh->GetName(), InstancedMeshComponents.Num());

	if (bUseHISM)
	{
		NewComp = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, CompName);
	}
	else
	{
		NewComp = NewObject<UInstancedStaticMeshComponent>(this, CompName);
	}

	if (NewComp)
	{
		NewComp->SetStaticMesh(Mesh);
		NewComp->SetupAttachment(GetRootComponent());

		// 回合制战斗需要：射线拾取/视线检测（碰撞）与 NavMesh 生成
		NewComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		NewComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		NewComp->SetCanEverAffectNavigation(true);

		NewComp->RegisterComponent();
		InstancedMeshComponents.Add(Mesh, NewComp);
	}

	return NewComp;
}
