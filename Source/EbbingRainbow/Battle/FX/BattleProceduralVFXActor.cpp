// Copyright Ebbing Rainbow. All Rights Reserved.

#include "Battle/FX/BattleProceduralVFXActor.h"
#include "Battle/Units/BattleUnit.h"
#include "Core/ERAssetCatalog.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"

ABattleProceduralVFXActor::ABattleProceduralVFXActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ABattleProceduralVFXActor::InitVFX(const FBattleVFXSpawnContext& InContext, int32 SeedOffset)
{
	Ctx = InContext;
	Elapsed = 0.0f;
	TotalDuration = FMath::Max(InContext.Duration, 0.14f);
	SetLifeSpan(TotalDuration + 0.08f);

	if (UERAssetCatalog* Catalog = UERAssetCatalog::Get(this))
	{
		PlaneMesh = Catalog->LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane"));
		GlowMat = Catalog->LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Glow.M_Route_Glow"));
		HaloMat = Catalog->LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Halo.M_Route_Halo"));
	}

	FlashLight = NewObject<UPointLightComponent>(this, TEXT("FlashLight"));
	if (FlashLight)
	{
		FlashLight->SetupAttachment(GetRootComponent());
		FlashLight->RegisterComponent();
		FlashLight->SetLightColor(Ctx.ThemeColor);
		FlashLight->SetIntensity(0.0f);
		FlashLight->SetAttenuationRadius(320.0f * Ctx.ScaleMultiplier);
		FlashLight->SetCastShadows(false);
		FlashLight->bUseInverseSquaredFalloff = false;
		FlashLight->SetLightFalloffExponent(2.0f);
	}

	switch (Ctx.Stage)
	{
	case EBattleVFXCueStage::Cast:
		if (Ctx.ResolvedPattern == EVFXDeliveryPattern::MeleeSlash)
		{
			SetupCastSlash();
		}
		else if (Ctx.ResolvedPattern == EVFXDeliveryPattern::RangedBeam)
		{
			SetupBeamOrArc(Ctx.bJaggedArc, SeedOffset);
		}
		else if (Ctx.ResolvedPattern == EVFXDeliveryPattern::ProjectileArc)
		{
			SetupProjectile();
		}
		else if (Ctx.ResolvedPattern == EVFXDeliveryPattern::SelfBuff)
		{
			SetupSelfAura();
		}
		else
		{
			SetupImpactBurst();
		}
		break;

	case EBattleVFXCueStage::Area:
		SetupAreaPulse();
		break;

	case EBattleVFXCueStage::ChainJump:
		SetupBeamOrArc(true, SeedOffset);
		break;

	case EBattleVFXCueStage::Impact:
	case EBattleVFXCueStage::Reaction:
	default:
		SetupImpactBurst();
		break;
	}
}

FBattleProceduralVFXLayer ABattleProceduralVFXActor::SpawnPlaneLayer(
	UMaterialInterface* Mat, const FVector& WorldLoc, const FRotator& WorldRot,
	const FVector& Scale, float BaseIntensity)
{
	FBattleProceduralVFXLayer Out;
	if (!PlaneMesh || !Mat)
	{
		return Out;
	}

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	if (!Mesh)
	{
		return Out;
	}
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetStaticMesh(PlaneMesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->SetReceivesDecals(false);
	Mesh->RegisterComponent();
	Mesh->SetWorldLocationAndRotation(WorldLoc, WorldRot);
	Mesh->SetWorldScale3D(Scale);

	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Mat))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Ctx.ThemeColor);
		MID->SetScalarParameterValue(TEXT("Intensity"), BaseIntensity);
		Out.MID = MID;
	}

	Out.Mesh = Mesh;
	Out.BaseIntensity = BaseIntensity;
	VFXLayers.Add(Out);
	return Out;
}

void ABattleProceduralVFXActor::SetupCastSlash()
{
	bIsSlash = true;
	const FVector Fwd = (Ctx.TargetLocation - Ctx.StartLocation).GetSafeNormal2D();
	const FRotator Facing = Fwd.IsNearlyZero() ? GetActorRotation() : Fwd.Rotation();
	SlashStartRot = Facing + FRotator(14.0f, -65.0f, 18.0f);

	const float S = 2.4f * Ctx.ScaleMultiplier;
	FBattleProceduralVFXLayer Ring = SpawnPlaneLayer(HaloMat ? HaloMat : GlowMat,
		Ctx.StartLocation + Facing.Vector() * 55.0f,
		SlashStartRot,
		FVector(S, S, 1.0f),
		7.5f);
	if (Ring.MID)
	{
		Ring.MID->SetScalarParameterValue(TEXT("RingRadius"), 0.72f);
		Ring.MID->SetScalarParameterValue(TEXT("RingWidth"), 0.055f);
		Ring.MID->SetScalarParameterValue(TEXT("RingIntensity"), 1.6f);
		Ring.MID->SetScalarParameterValue(TEXT("GlowPower"), 4.0f);
		Ring.MID->SetScalarParameterValue(TEXT("HoleInner"), 0.45f);
		Ring.MID->SetScalarParameterValue(TEXT("HoleOuter"), 0.65f);
		Ring.MID->SetScalarParameterValue(TEXT("RayIntensity"), 0.0f);
		Ring.MID->SetScalarParameterValue(TEXT("BreathAmount"), 0.0f);
	}

	if (FlashLight)
	{
		FlashLight->SetWorldLocation(Ctx.StartLocation + Facing.Vector() * 60.0f);
		FlashLight->SetIntensity(8.0f);
	}
}

void ABattleProceduralVFXActor::SetupBeamOrArc(bool bJagged, int32 SeedOffset)
{
	const FVector Delta = Ctx.TargetLocation - Ctx.StartLocation;
	const float TotalLen = Delta.Size();
	if (TotalLen <= KINDA_SMALL_NUMBER)
	{
		SetupImpactBurst();
		return;
	}

	const FVector Dir = Delta / TotalLen;
	FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}
	const FVector UpOrtho = FVector::CrossProduct(Side, Dir).GetSafeNormal();

	const int32 NumSegments = bJagged ? 5 : 1;
	TArray<FVector> Points;
	Points.Reserve(NumSegments + 1);
	Points.Add(Ctx.StartLocation);

	FRandomStream Rng(SeedOffset + 1337);
	for (int32 i = 1; i < NumSegments; ++i)
	{
		const float Alpha = (float)i / (float)NumSegments;
		const float Envelope = FMath::Sin(Alpha * PI);
		const float JitterAmp = FMath::Min(48.0f, TotalLen * 0.09f) * Envelope;
		const FVector Offset = Side * Rng.FRandRange(-JitterAmp, JitterAmp)
			+ UpOrtho * Rng.FRandRange(-JitterAmp * 0.65f, JitterAmp * 0.65f);
		Points.Add(FMath::Lerp(Ctx.StartLocation, Ctx.TargetLocation, Alpha) + Offset);
	}
	Points.Add(Ctx.TargetLocation);

	const float BeamWidth = (bJagged ? 0.30f : 0.42f) * Ctx.ScaleMultiplier;
	const float CoreIntensity = bJagged ? 11.0f : 8.5f;

	for (int32 i = 0; i < Points.Num() - 1; ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[i + 1];
		const FVector Seg = B - A;
		const float SegLen = Seg.Size();
		if (SegLen <= 1.0f)
		{
			continue;
		}
		const FVector Mid = (A + B) * 0.5f;
		const FRotator SegRot = FRotationMatrix::MakeFromX(Seg).Rotator();

		// 两片正交发光条带保证任意仰角都饱满可见
		for (int32 Cross = 0; Cross < 2; ++Cross)
		{
			const FRotator CrossRot = (Cross == 0) ? SegRot : (FQuat(Seg / SegLen, HALF_PI) * SegRot.Quaternion()).Rotator();
			FBattleProceduralVFXLayer L = SpawnPlaneLayer(GlowMat, Mid, CrossRot, FVector(SegLen / 100.0f, BeamWidth, 1.0f), CoreIntensity);
			if (L.MID)
			{
				L.MID->SetScalarParameterValue(TEXT("ScaleU"), 1.0f);
				L.MID->SetScalarParameterValue(TEXT("ScaleV"), 2.4f);
				L.MID->SetScalarParameterValue(TEXT("Power"), 2.2f);
				L.MID->SetScalarParameterValue(TEXT("EndFade"), 0.05f);
			}
		}
	}

	// 命中末端高光闪斑
	FBattleProceduralVFXLayer EndFlash = SpawnPlaneLayer(HaloMat ? HaloMat : GlowMat,
		Ctx.TargetLocation, FRotator::ZeroRotator,
		FVector(1.35f * Ctx.ScaleMultiplier, 1.35f * Ctx.ScaleMultiplier, 1.0f), 7.5f);
	if (EndFlash.MID)
	{
		EndFlash.MID->SetScalarParameterValue(TEXT("RingRadius"), 0.45f);
		EndFlash.MID->SetScalarParameterValue(TEXT("RingWidth"), 0.04f);
		EndFlash.MID->SetScalarParameterValue(TEXT("RayIntensity"), 0.8f);
	}

	if (FlashLight)
	{
		FlashLight->SetWorldLocation(Ctx.TargetLocation);
		FlashLight->SetIntensity(10.0f);
	}
}

void ABattleProceduralVFXActor::SetupProjectile()
{
	bIsProjectile = true;
	const float Dist = FVector::Dist(Ctx.StartLocation, Ctx.TargetLocation);
	ArcPeakHeight = FMath::Clamp(Dist * 0.22f, 55.0f, 180.0f);

	const float OrbSize = 0.72f * Ctx.ScaleMultiplier;
	for (int32 i = 0; i < 2; ++i)
	{
		const FRotator Rot = (i == 0) ? FRotator::ZeroRotator : FRotator(90.0f, 0.0f, 0.0f);
		FBattleProceduralVFXLayer L = SpawnPlaneLayer(GlowMat, Ctx.StartLocation, Rot, FVector(OrbSize, OrbSize, 1.0f), 10.0f);
		if (L.MID)
		{
			L.MID->SetScalarParameterValue(TEXT("Power"), 1.8f);
		}
	}

	if (FlashLight)
	{
		FlashLight->SetWorldLocation(Ctx.StartLocation);
		FlashLight->SetIntensity(7.0f);
		FlashLight->SetAttenuationRadius(240.0f);
	}
}

void ABattleProceduralVFXActor::SetupAreaPulse()
{
	bIsArea = true;
	const float EffectiveRadius = FMath::Max(Ctx.Radius, 160.0f) * Ctx.ScaleMultiplier;
	const float PlaneScale = EffectiveRadius / 50.0f; // BasicShapes/Plane 为 100x100cm

	// 与游戏实际 AoE.Radius 完全等尺度的地面冲击波圆环 (略抬高避免和砖面 Z-fight)
	const FVector GroundCenter = Ctx.TargetLocation + FVector(0.0f, 0.0f, 14.0f);
	FBattleProceduralVFXLayer Wave = SpawnPlaneLayer(HaloMat ? HaloMat : GlowMat,
		GroundCenter, FRotator::ZeroRotator, FVector(PlaneScale * 0.35f, PlaneScale * 0.35f, 1.0f), 7.0f);
	if (Wave.MID)
	{
		Wave.MID->SetScalarParameterValue(TEXT("RingRadius"), 0.88f);
		Wave.MID->SetScalarParameterValue(TEXT("RingWidth"), 0.045f);
		Wave.MID->SetScalarParameterValue(TEXT("RingIntensity"), 1.8f);
		Wave.MID->SetScalarParameterValue(TEXT("GlowPower"), 2.4f);
		Wave.MID->SetScalarParameterValue(TEXT("RayCount"), 12.0f);
		Wave.MID->SetScalarParameterValue(TEXT("RayIntensity"), 0.55f);
		Wave.MID->SetScalarParameterValue(TEXT("HoleInner"), 0.12f);
		Wave.MID->SetScalarParameterValue(TEXT("HoleOuter"), 0.30f);
	}

	// 内核竖直耀斑平面
	FBattleProceduralVFXLayer Core = SpawnPlaneLayer(GlowMat,
		Ctx.TargetLocation + FVector(0.0f, 0.0f, 45.0f), FRotator(90.0f, 0.0f, 0.0f),
		FVector(PlaneScale * 0.45f, PlaneScale * 0.45f, 1.0f), 6.0f);
	if (Core.MID)
	{
		Core.MID->SetScalarParameterValue(TEXT("Power"), 2.2f);
	}

	if (FlashLight)
	{
		FlashLight->SetWorldLocation(Ctx.TargetLocation + FVector(0.0f, 0.0f, 60.0f));
		FlashLight->SetAttenuationRadius(EffectiveRadius * 1.35f);
		FlashLight->SetIntensity(14.0f);
	}
}

void ABattleProceduralVFXActor::SetupSelfAura()
{
	bIsSelfAura = true;
	const float RingScale = 2.2f * Ctx.ScaleMultiplier;
	FBattleProceduralVFXLayer Halo = SpawnPlaneLayer(HaloMat ? HaloMat : GlowMat,
		Ctx.StartLocation + FVector(0.0f, 0.0f, -45.0f), FRotator::ZeroRotator,
		FVector(RingScale, RingScale, 1.0f), 6.5f);
	if (Halo.MID)
	{
		Halo.MID->SetScalarParameterValue(TEXT("RingRadius"), 0.72f);
		Halo.MID->SetScalarParameterValue(TEXT("RingWidth"), 0.035f);
		Halo.MID->SetScalarParameterValue(TEXT("RingIntensity"), 1.6f);
		Halo.MID->SetScalarParameterValue(TEXT("RayIntensity"), 0.7f);
		Halo.MID->SetScalarParameterValue(TEXT("Spin"), 1.8f);
	}

	if (FlashLight)
	{
		FlashLight->SetWorldLocation(Ctx.StartLocation + FVector(0.0f, 0.0f, 20.0f));
		FlashLight->SetIntensity(8.5f);
	}
}

void ABattleProceduralVFXActor::SetupImpactBurst()
{
	bIsImpact = true;
	const float BonusScale = (Ctx.bCrit ? 1.45f : 1.0f) * (Ctx.bReaction ? 1.35f : 1.0f) * Ctx.ScaleMultiplier;
	const float S = 1.65f * BonusScale;

	FBattleProceduralVFXLayer Burst = SpawnPlaneLayer(HaloMat ? HaloMat : GlowMat,
		Ctx.TargetLocation, FRotator(65.0f, 0.0f, 0.0f),
		FVector(S * 0.45f, S * 0.45f, 1.0f), (Ctx.bCrit || Ctx.bReaction) ? 10.5f : 7.5f);
	if (Burst.MID)
	{
		Burst.MID->SetScalarParameterValue(TEXT("RingRadius"), 0.65f);
		Burst.MID->SetScalarParameterValue(TEXT("RingWidth"), 0.045f);
		Burst.MID->SetScalarParameterValue(TEXT("RingIntensity"), 1.6f);
		Burst.MID->SetScalarParameterValue(TEXT("RayCount"), 8.0f);
		Burst.MID->SetScalarParameterValue(TEXT("RayIntensity"), 1.1f);
		Burst.MID->SetScalarParameterValue(TEXT("GlowPower"), 1.8f);
	}

	if (FlashLight)
	{
		FlashLight->SetWorldLocation(Ctx.TargetLocation);
		FlashLight->SetAttenuationRadius(280.0f * BonusScale);
		FlashLight->SetIntensity((Ctx.bCrit || Ctx.bReaction) ? 13.0f : 8.0f);
	}
}

void ABattleProceduralVFXActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / TotalDuration, 0.0f, 1.0f);
	const float EaseOut = 1.0f - FMath::Pow(1.0f - Alpha, 2.5f);
	const float Fade = FMath::Pow(1.0f - Alpha, 1.4f);

	if (bIsSlash && VFXLayers.Num() > 0 && VFXLayers[0].Mesh)
	{
		const FRotator Rot = SlashStartRot + FRotator(0.0f, 130.0f * EaseOut, -20.0f * EaseOut);
		VFXLayers[0].Mesh->SetWorldRotation(Rot);
	}
	else if (bIsProjectile)
	{
		const FVector Pos = FMath::Lerp(Ctx.StartLocation, Ctx.TargetLocation, Alpha)
			+ FVector(0.0f, 0.0f, ArcPeakHeight * FMath::Sin(Alpha * PI));
		for (const FBattleProceduralVFXLayer& L : VFXLayers)
		{
			if (L.Mesh)
			{
				L.Mesh->SetWorldLocation(Pos);
			}
		}
		if (FlashLight)
		{
			FlashLight->SetWorldLocation(Pos);
		}
	}
	else if (bIsArea && VFXLayers.Num() > 0)
	{
		const float EffectiveRadius = FMath::Max(Ctx.Radius, 160.0f) * Ctx.ScaleMultiplier;
		const float TargetScale = EffectiveRadius / 50.0f;
		const float CurScale = TargetScale * FMath::Lerp(0.28f, 1.05f, EaseOut);
		if (VFXLayers[0].Mesh)
		{
			VFXLayers[0].Mesh->SetWorldScale3D(FVector(CurScale, CurScale, 1.0f));
		}
	}
	else if (bIsSelfAura && VFXLayers.Num() > 0 && VFXLayers[0].Mesh)
	{
		const FVector Anchor = Ctx.Caster ? Ctx.Caster->GetActorLocation() : Ctx.StartLocation;
		const float LiftZ = FMath::Lerp(-48.0f, 52.0f, EaseOut);
		VFXLayers[0].Mesh->SetWorldLocation(Anchor + FVector(0.0f, 0.0f, LiftZ));
		const float CurScale = (2.1f + 0.55f * FMath::Sin(Alpha * PI)) * Ctx.ScaleMultiplier;
		VFXLayers[0].Mesh->SetWorldScale3D(FVector(CurScale, CurScale, 1.0f));
	}
	else if (bIsImpact && VFXLayers.Num() > 0 && VFXLayers[0].Mesh)
	{
		const float BonusScale = (Ctx.bCrit ? 1.45f : 1.0f) * (Ctx.bReaction ? 1.35f : 1.0f) * Ctx.ScaleMultiplier;
		const float CurScale = 1.65f * BonusScale * FMath::Lerp(0.35f, 1.25f, EaseOut);
		VFXLayers[0].Mesh->SetWorldScale3D(FVector(CurScale, CurScale, 1.0f));
	}

	// 非投射物随生命周期平滑淡出
	const float IntensityFade = bIsProjectile ? 1.0f : Fade;
	for (const FBattleProceduralVFXLayer& L : VFXLayers)
	{
		if (L.MID)
		{
			L.MID->SetScalarParameterValue(TEXT("Intensity"), L.BaseIntensity * IntensityFade);
		}
	}
	if (FlashLight)
	{
		const float BaseLight = (bIsArea || Ctx.bCrit || Ctx.bReaction) ? 12.0f : 8.0f;
		FlashLight->SetIntensity(BaseLight * IntensityFade);
	}
}
