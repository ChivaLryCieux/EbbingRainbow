// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/DayNightCycleManager.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "EngineUtils.h"

ADayNightCycleManager::ADayNightCycleManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
}

void ADayNightCycleManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bAutoFindLights && (!SunLight || !SkyLight))
	{
		FindSceneLights();
	}

	if (SunLight)
	{
		if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			if (CachedSunIntensity <= 0.0f && LightComp->Intensity > 0.01f)
			{
				CachedSunIntensity = LightComp->Intensity;
			}
		}
	}

	if (bPreviewInEditor)
	{
		ApplyLightingState();
	}
}

void ADayNightCycleManager::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindLights && (!SunLight || !SkyLight))
	{
		FindSceneLights();
	}

	if (SunLight)
	{
		if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			if (CachedSunIntensity <= 0.0f && LightComp->Intensity > 0.01f)
			{
				CachedSunIntensity = LightComp->Intensity;
			}
		}
	}

	if (SkyLight && bEnsureSkyLightRealtimeCapture)
	{
		if (USkyLightComponent* SkyComp = SkyLight->GetLightComponent())
		{
			SkyComp->bRealTimeCapture = true;
		}
	}

	bWasDay = IsDay();
	LastRecordedHour = GetCurrentHour();

	ApplyLightingState();
}

void ADayNightCycleManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCyclePaused && DayDurationInMinutes > 0.0f)
	{
		const float DayDurationSeconds = DayDurationInMinutes * 60.0f;
		const float TimeIncrement = (DeltaTime / DayDurationSeconds) * 24.0f * TimeScale;

		TimeOfDay = FMath::Fmod(TimeOfDay + TimeIncrement, 24.0f);
		if (TimeOfDay < 0.0f)
		{
			TimeOfDay += 24.0f;
		}

		ApplyLightingState();

		OnTimeOfDayChanged.Broadcast(TimeOfDay);

		const int32 CurrentHour = GetCurrentHour();
		if (CurrentHour != LastRecordedHour)
		{
			LastRecordedHour = CurrentHour;
			OnHourChanged.Broadcast(CurrentHour);
		}

		const bool bCurrentDay = IsDay();
		if (bCurrentDay != bWasDay)
		{
			bWasDay = bCurrentDay;
			if (bCurrentDay)
			{
				OnDayStarted.Broadcast();
			}
			else
			{
				OnNightStarted.Broadcast();
			}
		}
	}
}

void ADayNightCycleManager::CalculateCelestialRotations(FRotator& OutSunRotation, FRotator& OutMoonRotation, float& OutSolarElevation) const
{
	// 1. Convert TimeOfDay (0 - 24) to orbit angle in radians:
	// 06:00 -> 0 deg (East horizon, sunrise)
	// 12:00 -> 90 deg (Noon, zenith peak)
	// 18:00 -> 180 deg (West horizon, sunset)
	// 00:00 -> 270 deg (Midnight, nadir below horizon)
	const float NormalizedTime = TimeOfDay / 24.0f;
	const float OrbitAngleDeg = (NormalizedTime - 0.25f) * 360.0f;
	const float ThetaRad = FMath::DegreesToRadians(OrbitAngleDeg);

	// 2. Azimuth (Yaw) and Orbit Tilt (Latitude) in radians
	const float YawRad = FMath::DegreesToRadians(SunOrientationYaw);
	const float TiltRad = FMath::DegreesToRadians(SunLatitudeTilt);

	// Horizon East vector (sunrise direction)
	const FVector EastVec(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.0f);

	// Horizon South vector (perpendicular to East)
	const FVector SouthVec(-FMath::Sin(YawRad), FMath::Cos(YawRad), 0.0f);

	// Noon vector (sun position at peak noon, tilted from zenith by TiltRad along SouthVec)
	const FVector NoonVec = FMath::Sin(TiltRad) * SouthVec + FVector(0.0f, 0.0f, FMath::Cos(TiltRad));

	// Sun position vector pointing FROM the world origin TO the sun in the celestial dome
	const FVector SunPositionInSky = FMath::Cos(ThetaRad) * EastVec + FMath::Sin(ThetaRad) * NoonVec;

	// Solar elevation: World Z component of the skyward vector (> 0 is daytime, < 0 is nighttime)
	OutSolarElevation = SunPositionInSky.Z;

	// Directional light shines FROM the sun INTO the world, so its forward vector is -SunPositionInSky
	const FVector LightForwardDir = -SunPositionInSky;
	OutSunRotation = LightForwardDir.Rotation();

	// Moon is diametrically opposite to the sun
	const FVector MoonForwardDir = SunPositionInSky;
	OutMoonRotation = MoonForwardDir.Rotation();
}

void ADayNightCycleManager::ApplyLightingState()
{
	FRotator SunRot;
	FRotator MoonRot;
	float SolarElevation = 0.0f;
	CalculateCelestialRotations(SunRot, MoonRot, SolarElevation);

	if (SunLight)
	{
		SunLight->SetActorRotation(SunRot);
		UpdateSunProperties(SolarElevation);
	}

	if (MoonLight)
	{
		MoonLight->SetActorRotation(MoonRot);
		UpdateMoonProperties(SolarElevation);
	}
}

void ADayNightCycleManager::UpdateSunProperties(float SolarElevation)
{
	if (!SunLight)
	{
		return;
	}

	UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent());
	if (!LightComp)
	{
		return;
	}

	// Cache original intensity once if not already cached
	if (CachedSunIntensity <= 0.0f)
	{
		if (LightComp->Intensity > 0.01f)
		{
			CachedSunIntensity = LightComp->Intensity;
		}
		else
		{
			CachedSunIntensity = 3.0f; // Sensible default if it was crushed to 0 previously
		}
	}

	const float TargetMaxIntensity = (MaxSunIntensity > 0.0f) ? MaxSunIntensity : CachedSunIntensity;

	// Revive light if it was previously set to 0 and sun is above horizon
	if (LightComp->Intensity <= 0.001f && SolarElevation > 0.0f)
	{
		LightComp->SetIntensity(TargetMaxIntensity);
		LightComp->SetCastShadows(true);
	}

	if (bControlSunIntensity)
	{
		// Smooth transition around horizon [-0.05, 0.10]
		const float DaylightAlpha = FMath::SmoothStep(-0.05f, 0.10f, SolarElevation);
		const float CurrentIntensity = FMath::Lerp(NightSunIntensity, TargetMaxIntensity, DaylightAlpha);
		LightComp->SetIntensity(CurrentIntensity);

		// Color temperature: warm at sunrise/sunset, crisp daylight at noon
		const float NoonAlpha = FMath::Clamp(SolarElevation, 0.0f, 1.0f);
		const float CurrentTemp = FMath::Lerp(SunTemperatureDawnDusk, SunTemperatureNoon, NoonAlpha);
		LightComp->bUseTemperature = true;
		LightComp->SetTemperature(CurrentTemp);

		if (bDisableShadowsAtNight)
		{
			const bool bShouldCastShadow = (SolarElevation > -0.02f);
			LightComp->SetCastShadows(bShouldCastShadow);
		}
	}
	else
	{
		// In pure rotation mode, ensure the light is on and cast shadows are active during day
		if (SolarElevation > 0.0f)
		{
			if (LightComp->Intensity <= 0.001f)
			{
				LightComp->SetIntensity(TargetMaxIntensity);
			}
			LightComp->SetCastShadows(true);
		}
		else if (bDisableShadowsAtNight && SolarElevation < -0.05f)
		{
			LightComp->SetCastShadows(false);
		}
	}
}

void ADayNightCycleManager::UpdateMoonProperties(float SolarElevation)
{
	if (!MoonLight)
	{
		return;
	}

	UDirectionalLightComponent* MoonComp = Cast<UDirectionalLightComponent>(MoonLight->GetLightComponent());
	if (!MoonComp)
	{
		return;
	}

	// Moon elevation is opposite of solar elevation
	const float MoonElevation = -SolarElevation;
	const float MoonlightAlpha = FMath::SmoothStep(-0.05f, 0.10f, MoonElevation);
	const float CurrentMoonIntensity = FMath::Lerp(0.0f, MaxMoonIntensity, MoonlightAlpha);
	MoonComp->SetIntensity(CurrentMoonIntensity);

	if (bDisableShadowsAtNight)
	{
		MoonComp->SetCastShadows(MoonElevation > 0.0f);
	}
}

void ADayNightCycleManager::SetTimeOfDay(float NewTime)
{
	TimeOfDay = FMath::Fmod(NewTime, 24.0f);
	if (TimeOfDay < 0.0f)
	{
		TimeOfDay += 24.0f;
	}
	ApplyLightingState();
}

void ADayNightCycleManager::SetDayDurationInMinutes(float NewDurationInMinutes)
{
	DayDurationInMinutes = FMath::Max(0.01f, NewDurationInMinutes);
}

void ADayNightCycleManager::SetTimeScale(float NewScale)
{
	TimeScale = FMath::Max(0.0f, NewScale);
}

void ADayNightCycleManager::PauseCycle(bool bPause)
{
	bCyclePaused = bPause;
}

bool ADayNightCycleManager::IsDay() const
{
	FRotator SunRot;
	FRotator MoonRot;
	float SolarElevation = 0.0f;
	CalculateCelestialRotations(SunRot, MoonRot, SolarElevation);
	return SolarElevation > 0.0f;
}

int32 ADayNightCycleManager::GetCurrentHour() const
{
	return FMath::FloorToInt32(TimeOfDay) % 24;
}

int32 ADayNightCycleManager::GetCurrentMinute() const
{
	const float Fraction = TimeOfDay - FMath::FloorToFloat(TimeOfDay);
	return FMath::FloorToInt32(Fraction * 60.0f) % 60;
}

FString ADayNightCycleManager::GetFormattedTime() const
{
	const int32 Hour = GetCurrentHour();
	const int32 Minute = GetCurrentMinute();
	return FString::Printf(TEXT("%02d:%02d"), Hour, Minute);
}

void ADayNightCycleManager::FindSceneLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1. Find Sun Light
	if (!SunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			ADirectionalLight* DirLight = *It;
			if (DirLight)
			{
				if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(DirLight->GetLightComponent()))
				{
					if (LightComp->bAtmosphereSunLight && LightComp->AtmosphereSunLightIndex == 0)
					{
						SunLight = DirLight;
						break;
					}
				}
			}
		}

		if (!SunLight)
		{
			for (TActorIterator<ADirectionalLight> It(World); It; ++It)
			{
				if (*It != MoonLight)
				{
					SunLight = *It;
					break;
				}
			}
		}
	}

	// 2. Find Moon Light if configured
	if (!MoonLight && SunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			ADirectionalLight* DirLight = *It;
			if (DirLight && DirLight != SunLight)
			{
				if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(DirLight->GetLightComponent()))
				{
					if (LightComp->bAtmosphereSunLight && LightComp->AtmosphereSunLightIndex == 1)
					{
						MoonLight = DirLight;
						break;
					}
				}
			}
		}
	}

	// 3. Find Sky Light
	if (!SkyLight)
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			SkyLight = *It;
			break;
		}
	}
}
