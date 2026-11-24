// Copyright Ebbing Rainbow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Battle/FX/BattleVFXTypes.h"
#include "BattleProceduralVFXActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT()
struct FBattleProceduralVFXLayer
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;

	float BaseIntensity = 4.0f;
};

/**
 * 技能特效程序化兜底演出 Actor:
 * 当技能的 Niagara 槽位尚未绑定资产时，自动借助项目既有的发光着色器
 * (M_Route_Glow / M_Route_Halo) + 瞬时点光源，按主题色渲染对应形态：
 * - 近战斩击弧 / 远程直线光束 / 电学折线电弧 / 抛物线投射球 / 精确半径 AoE 波纹 / 自身护盾光环 / 命中爆裂。
 */
UCLASS(Transient)
class EBBINGRAINBOW_API ABattleProceduralVFXActor : public AActor
{
	GENERATED_BODY()

public:
	ABattleProceduralVFXActor();

	void InitVFX(const FBattleVFXSpawnContext& InContext, int32 SeedOffset);
	virtual void Tick(float DeltaSeconds) override;

private:
	FBattleProceduralVFXLayer SpawnPlaneLayer(UMaterialInterface* Mat, const FVector& WorldLoc, const FRotator& WorldRot,
		const FVector& Scale, float BaseIntensity);

	void SetupCastSlash();
	void SetupBeamOrArc(bool bJagged, int32 SeedOffset);
	void SetupProjectile();
	void SetupAreaPulse();
	void SetupSelfAura();
	void SetupImpactBurst();

	FBattleVFXSpawnContext Ctx;
	float Elapsed = 0.0f;
	float TotalDuration = 0.35f;

	UPROPERTY(Transient)
	TArray<FBattleProceduralVFXLayer> VFXLayers;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GlowMat = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HaloMat = nullptr;

	FRotator SlashStartRot = FRotator::ZeroRotator;
	float ArcPeakHeight = 110.0f;
	bool bIsProjectile = false;
	bool bIsSlash = false;
	bool bIsArea = false;
	bool bIsSelfAura = false;
	bool bIsImpact = false;
};
