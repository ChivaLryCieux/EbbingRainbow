// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFCArchTypes.h"
#include "WFC3DArenaGenerator_Atrium.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * Macro-architecture arena generator for the WFC_Kit.
 *
 * It bypasses the per-cell socket solver on purpose: FWFCArchGrammar owns the
 * global composition and resolves every boundary to a kit tile, then this actor
 * renders the two levels with HISM.  The solver remains available for the other
 * styles; this style is the "space grammar + socket-exact fill" reference.
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_Atrium : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_Atrium();

	virtual bool Generate(const FWFCGenerationRequest& Request) override;
	virtual void ClearGenerated() override;

protected:
	virtual FVector GetCellWorldSize() const override { return FVector(400.0f, 400.0f, 400.0f); }

private:
	UStaticMesh* LoadTileMesh(const FName& TileId);
	void AddTile(const FName& TileId, int32 X, int32 Y, float Z);
	void AddStairs(const FArchPlan& Plan);
	void BuildCollision(const FArchPlan& Plan);

	UPROPERTY(Transient)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CollisionHISM = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StairRamps;

	TMap<FName, TObjectPtr<UStaticMesh>> MeshCache;

	static constexpr int32 GridW = 19;
	static constexpr int32 GridH = 19;
	static constexpr float CellSize = 400.0f;
};
