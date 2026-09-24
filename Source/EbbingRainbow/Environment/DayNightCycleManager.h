// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightCycleManager.generated.h"

class ADirectionalLight;
class ASkyLight;
class UDirectionalLightComponent;
class USkyLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeOfDayChanged, float, NewTimeOfDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHourChanged, int32, CurrentHour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayNightPhaseChanged);

/**
 * ADayNightCycleManager
 * 
 * Controls DirectionalLight rotation and parameters to simulate a realistic Day-Night cycle.
 * Supports:
 * - 24-hour cycle with configurable day duration and time scale
 * - Accurate 3D celestial orbital rotation (azimuth orientation and latitude tilt)
 * - Automatic discovery of scene DirectionalLight and SkyLight
 * - Dynamic sun intensity, color temperature, and shadow management
 * - Optional Moon DirectionalLight support
 * - Realtime SkyLight updating for Lumen and ambient lighting
 * - Editor preview via OnConstruction (drag TimeOfDay slider to see instant lighting changes)
 * - Blueprint events and callable control functions
 */
UCLASS(Blueprintable, ClassGroup = (Environment), meta = (DisplayName = "Day Night Cycle Manager"))
class EBBINGRAINBOW_API ADayNightCycleManager : public AActor
{
	GENERATED_BODY()

public:
	ADayNightCycleManager();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// ==========================================
	// Scene Light References
	// ==========================================

	/** If true, automatically finds the DirectionalLight and SkyLight in the scene if not set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lights")
	bool bAutoFindLights = true;

	/** Primary DirectionalLight acting as the Sun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lights")
	TObjectPtr<ADirectionalLight> SunLight;

	/** Optional secondary DirectionalLight acting as the Moon (opposite to Sun) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lights")
	TObjectPtr<ADirectionalLight> MoonLight;

	/** Optional SkyLight in the scene for ambient reflection/lighting updates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lights")
	TObjectPtr<ASkyLight> SkyLight;

	// ==========================================
	// Time Configuration
	// ==========================================

	/** Current time of day in 24-hour format (0.0 = 00:00 midnight, 6.0 = 06:00 sunrise, 12.0 = 12:00 noon, 18.0 = 18:00 sunset) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Time", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float TimeOfDay = 8.0f;

	/** Total real-world minutes for a complete 24-hour in-game day (e.g., 12.0 = 12 real minutes per game day) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Time", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DayDurationInMinutes = 3.0f;

	/** Time multiplier for speeding up or slowing down time progression */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Time", meta = (ClampMin = "0.0"))
	float TimeScale = 1.0f;

	/** Whether time progression is currently paused */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Time")
	bool bCyclePaused = false;

	/** If true, dragging TimeOfDay or changing settings in editor updates the viewport lighting immediately */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Time")
	bool bPreviewInEditor = true;

	// ==========================================
	// Celestial Orbit Configuration
	// ==========================================

	/** Azimuth / compass direction of the sun's trajectory (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Orbit", meta = (ClampMin = "-180.0", ClampMax = "180.0", UIMin = "-180.0", UIMax = "180.0"))
	float SunOrientationYaw = -45.0f;

	/** Orbital tilt angle / latitude inclination of the sun's path (degrees). 0 = passes straight overhead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Orbit", meta = (ClampMin = "-80.0", ClampMax = "80.0", UIMin = "-80.0", UIMax = "80.0"))
	float SunLatitudeTilt = 25.0f;

	// ==========================================
	// Lighting & Atmosphere Transition
	// ==========================================

	/** 
	 * Whether to manually modulate sun intensity and color temperature in code.
	 * Default is FALSE because UE5 SkyAtmosphere naturally handles physical atmospheric scattering and sunset/sunrise coloring purely via DirectionalLight rotation!
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments")
	bool bControlSunIntensity = false;

	/** Peak sun intensity at noon. If 0 or negative, automatically preserves the DirectionalLight's original intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments", meta = (EditCondition = "bControlSunIntensity", ClampMin = "0.0"))
	float MaxSunIntensity = 0.0f;

	/** Sun intensity when below horizon at night */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments", meta = (EditCondition = "bControlSunIntensity", ClampMin = "0.0"))
	float NightSunIntensity = 0.0f;

	/** Peak moon intensity at midnight (Lux) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments", meta = (ClampMin = "0.0"))
	float MaxMoonIntensity = 0.25f;

	/** Light temperature (Kelvin) at noon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments", meta = (EditCondition = "bControlSunIntensity", ClampMin = "1700.0", ClampMax = "12000.0"))
	float SunTemperatureNoon = 6500.0f;

	/** Light temperature (Kelvin) at sunrise and sunset (warm golden hour) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments", meta = (EditCondition = "bControlSunIntensity", ClampMin = "1700.0", ClampMax = "12000.0"))
	float SunTemperatureDawnDusk = 3200.0f;

	/** Whether to disable sun shadows when below the horizon to prevent ground light leaking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments")
	bool bDisableShadowsAtNight = true;

	/** Automatically ensure SkyLight real-time capture is active for Lumen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Lighting Adjustments")
	bool bEnsureSkyLightRealtimeCapture = true;

	// ==========================================
	// Blueprint Events
	// ==========================================

	/** Broadcasts every tick when TimeOfDay updates */
	UPROPERTY(BlueprintAssignable, Category = "Day Night|Events")
	FOnTimeOfDayChanged OnTimeOfDayChanged;

	/** Broadcasts when in-game clock passes an integer hour */
	UPROPERTY(BlueprintAssignable, Category = "Day Night|Events")
	FOnHourChanged OnHourChanged;

	/** Broadcasts when transition into day starts (sun rises above horizon) */
	UPROPERTY(BlueprintAssignable, Category = "Day Night|Events")
	FOnDayNightPhaseChanged OnDayStarted;

	/** Broadcasts when transition into night starts (sun sinks below horizon) */
	UPROPERTY(BlueprintAssignable, Category = "Day Night|Events")
	FOnDayNightPhaseChanged OnNightStarted;

	// ==========================================
	// Blueprint Callable Functions
	// ==========================================

	/** Sets the time of day directly (0.0 - 24.0) */
	UFUNCTION(BlueprintCallable, Category = "Day Night|Control")
	void SetTimeOfDay(float NewTime);

	/** Gets current time of day (0.0 - 24.0) */
	UFUNCTION(BlueprintPure, Category = "Day Night|Control")
	float GetTimeOfDay() const { return TimeOfDay; }

	/** Sets the day duration in minutes */
	UFUNCTION(BlueprintCallable, Category = "Day Night|Control")
	void SetDayDurationInMinutes(float NewDurationInMinutes);

	/** Sets time scale */
	UFUNCTION(BlueprintCallable, Category = "Day Night|Control")
	void SetTimeScale(float NewScale);

	/** Pauses or resumes the day-night cycle */
	UFUNCTION(BlueprintCallable, Category = "Day Night|Control")
	void PauseCycle(bool bPause);

	/** Returns true if the sun is currently above the horizon */
	UFUNCTION(BlueprintPure, Category = "Day Night|Control")
	bool IsDay() const;

	/** Returns true if the sun is currently below the horizon */
	UFUNCTION(BlueprintPure, Category = "Day Night|Control")
	bool IsNight() const { return !IsDay(); }

	/** Returns current integer hour (0 - 23) */
	UFUNCTION(BlueprintPure, Category = "Day Night|Control")
	int32 GetCurrentHour() const;

	/** Returns current integer minute (0 - 59) */
	UFUNCTION(BlueprintPure, Category = "Day Night|Control")
	int32 GetCurrentMinute() const;

	/** Returns human-readable time string (e.g. "08:15") */
	UFUNCTION(BlueprintPure, Category = "Day Night|Control")
	FString GetFormattedTime() const;

	/** Forces manual search and binding of DirectionalLight and SkyLight in the scene */
	UFUNCTION(BlueprintCallable, Category = "Day Night|Control")
	void FindSceneLights();

	/** Updates the sun rotation and parameters according to the current TimeOfDay */
	UFUNCTION(BlueprintCallable, Category = "Day Night|Control")
	void ApplyLightingState();

private:
	/** Calculates Sun and Moon rotation and solar altitude using spherical orbital vectors */
	void CalculateCelestialRotations(FRotator& OutSunRotation, FRotator& OutMoonRotation, float& OutSolarElevation) const;

	/** Updates the sun's physical and photometric properties */
	void UpdateSunProperties(float SolarElevation);

	/** Updates the moon's physical and photometric properties */
	void UpdateMoonProperties(float SolarElevation);

	/** Cached initial sun intensity to avoid destroying original scene settings */
	float CachedSunIntensity = -1.0f;

	/** Last integer hour recorded, used for OnHourChanged trigger */
	int32 LastRecordedHour = -1;

	/** Previous frame's day state, used for OnDayStarted/OnNightStarted trigger */
	bool bWasDay = false;
};
