// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC/Shared/ERArenaAtmosphere.h"
#include "Core/ERDebug.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

namespace
{
	/** 虚空雾参数 (近黑蓝; 调谐关卡雾与自 spawn 共用) */
	void ApplyVoidFog(UExponentialHeightFogComponent* FC)
	{
		FC->SetFogDensity(0.008f);
		FC->SetFogHeightFalloff(0.45f);
		FC->SetFogInscatteringColor(FLinearColor::FromSRGBColor(FColor(10, 13, 22)));
		FC->SetDirectionalInscatteringColor(FLinearColor(0.001f, 0.002f, 0.004f));
		FC->SetDirectionalInscatteringExponent(14.0f);
		FC->SetStartDistance(1200.0f);
		FC->SetVolumetricFog(true);
		FC->SetVolumetricFogScatteringDistribution(0.55f);
		FC->SetVolumetricFogAlbedo(FColor(18, 26, 48));
		FC->SetVolumetricFogEmissive(FLinearColor(0.002f, 0.003f, 0.006f));
	}
}

void ER::Atmosphere::SpawnVoidAtmosphere(AActor* Owner, FRandomStream& Rng, TArray<TObjectPtr<AActor>>& OutSpawned)
{
	if (!Owner)
	{
		return;
	}
	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}
	// 默认不生成/不调氛围: 关卡自摆光照雾效归用户手调;
	// -Atmo 显式开启程序氛围 (种子化三点光+虚空雾)
	if (!ER::Debug::IsAtmoEnabled())
	{
		return;
	}
	const FVector Origin = Owner->GetActorLocation();

	// ---- 调谐关卡自带氛围演员 (粉紫参数覆盖一切, 统一压成虚空基调) ----
	bool bFoundLevelFog = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A == Owner || A->GetOwner() == Owner)
		{
			continue;
		}
		if (AExponentialHeightFog* F = Cast<AExponentialHeightFog>(A))
		{
			bFoundLevelFog = true;
			if (UExponentialHeightFogComponent* FC = F->GetComponent())
			{
				ApplyVoidFog(FC);
			}
		}
		else if (ASkyLight* S = Cast<ASkyLight>(A))
		{
			if (USkyLightComponent* SC = S->GetLightComponent())
			{
				SC->SetIntensity(0.3f);
				SC->SetLightColor(FLinearColor::FromSRGBColor(FColor(70, 82, 110)));
				SC->bLowerHemisphereIsBlack = true;
			}
		}
		else if (A->GetClass()->GetName().Contains(TEXT("SkyAtmosphere")))
		{
			A->SetActorHiddenInGame(true);
		}
	}

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	P.Owner = Owner;

	// 主光: 暖白, 高位斜射, 方向随种子变化
	const float YawDeg = 20.0f + Rng.FRand() * 320.0f;
	const float PitchDeg = -(48.0f + Rng.FRand() * 14.0f);
	if (ADirectionalLight* Key = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), Origin, FRotator(PitchDeg, YawDeg, 0.0f), P))
	{
		UDirectionalLightComponent* LC = Key->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(7.5f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(255, 244, 229))); // 微暖白
		LC->SetCastShadows(true);
		LC->LightSourceAngle = 0.8f;
		LC->ForwardShadingPriority = 3; // 三盏各不同优先级, 消多主光警告
		Key->SetActorRotation(FRotator(PitchDeg, YawDeg, 0.0f));
		OutSpawned.Add(Key);
	}

	// 补光: 无影冷色低位, 只抬阴影不死黑 (替代 skylight, 避免捕获时序问题)
	if (ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), Origin, FRotator(-25.0f, YawDeg + 160.0f, 0.0f), P))
	{
		UDirectionalLightComponent* LC = Fill->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(0.9f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(120, 140, 175))); // 冷蓝灰
		LC->SetCastShadows(false);
		LC->bAffectTranslucentLighting = true;
		LC->ForwardShadingPriority = 2;
		OutSpawned.Add(Fill);
	}

	// 反弹光: 从下往上打, 亮出立面/底面 -> "漂在云海之上"的反光感
	if (ADirectionalLight* Bounce = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), Origin, FRotator(65.0f, YawDeg + 180.0f, 0.0f), P))
	{
		UDirectionalLightComponent* LC = Bounce->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(1.4f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(90, 105, 140)));
		LC->SetCastShadows(false);
		LC->ForwardShadingPriority = 1;
		OutSpawned.Add(Bounce);
	}

	// 雾池: 关卡已放雾则只调谐; 否则自己补一个 (其它地图兜底)
	if (!bFoundLevelFog)
	{
		if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(
			AExponentialHeightFog::StaticClass(), Origin + FVector(0, 0, -1400.0f),
			FRotator::ZeroRotator, P))
		{
			ApplyVoidFog(Fog->GetComponent());
			OutSpawned.Add(Fog);
		}
	}
}
