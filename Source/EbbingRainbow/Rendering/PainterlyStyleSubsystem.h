// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PainterlyStyleSubsystem.generated.h"

class APostProcessVolume;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Applies the painterly (thick paint / impasto) post process to every game world.
 *
 * Instead of hand-placing an unbound PostProcessVolume in every level, the
 * subsystem spawns one at BeginPlay, so the whole game -- hall, route map and
 * arena -- shares the same look, including PIE and packaged builds.
 *
 * Configuration (Config/DefaultGame.ini):
 *     [/Script/EbbingRainbow.PainterlyStyle]
 *     MaterialPath=/Game/Style/Painterly/MI_PP_Painterly
 *
 * Runtime toggle (great for art direction A/B):
 *     Paint.Style 0     // original renderer
 *     Paint.Style 1     // painterly
 *     Paint.Debug 3     // 0 final, 1 flow, 2 ink, 3 impasto normal, 4 raw filter, 5 confidence
 *     Paint.Param P_BrushRadius 12
 *     Paint.Shot my_shot
 */
UCLASS()
class UPainterlyStyleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/** Loads the style material from config and (re)creates the volume. */
	void ApplyStyle();

	/** 0..1 blend weight; 0 removes the look. */
	void SetStyleWeight(float InWeight);

	float GetStyleWeight() const { return StyleWeight; }

	/** Debug view: 0 final, 1 flow, 2 ink, 3 impasto normal, 4 raw filter, 5 confidence. */
	void SetDebugMode(float InMode);

	/** Sets any P_* scalar parameter at runtime (returns false if the name is unknown). */
	bool SetScalarParameter(const FName& ParameterName, float Value);

private:
	APostProcessVolume* EnsureVolume(UWorld* World);
	UMaterialInterface* GetStyleMaterial();

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> Volume = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> StyleMaterial = nullptr;

	/** Dynamic instance actually plugged into the volume, so parameters can be tweaked live. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StyleMID = nullptr;

	float StyleWeight = 1.0f;
	bool bMaterialResolved = false;
	int32 AppliedDebugMode = -1;
};
