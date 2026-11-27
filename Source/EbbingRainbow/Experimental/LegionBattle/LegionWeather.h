// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LegionWeather.generated.h"

class UWorld;

/** 开放世界气象与光影预设 (受星区、冲突烈度与战场种子联合驱动) */
UENUM(BlueprintType)
enum class ELegionWeatherPreset : uint8
{
	ClearMorning  UMETA(DisplayName = "清冽晨曦"),
	HighNoon      UMETA(DisplayName = "烈阳正午"),
	DuskGold      UMETA(DisplayName = "残阳暮色"),
	MountainMist  UMETA(DisplayName = "山岚浓雾"),
	OvercastStorm UMETA(DisplayName = "阴云骤雨"),
	AshConflict   UMETA(DisplayName = "战火硝烟")
};

/** 单个气象预设的光照/雾效/后处理参数表项 */
struct FLegionWeatherSpec
{
	ELegionWeatherPreset Preset = ELegionWeatherPreset::ClearMorning;
	const TCHAR* DisplayName = TEXT("清冽晨曦");

	float SunPitchDeg = -28.0f;
	/** 相对战线朝向 BattleAxisYawDeg 的侧逆/侧顺光偏角 (deg) */
	float SunYawOffsetDeg = -68.0f;
	float SunIntensity = 5.6f;
	FColor SunColor = FColor(255, 236, 206, 255);

	float SkyIntensity = 0.85f;
	FColor SkyColor = FColor(178, 202, 232, 255);

	float FogDensity = 0.012f;
	float FogHeightFalloff = 0.24f;
	float FogStartDistance = 1200.0f;
	FLinearColor FogInscatter = FLinearColor(0.20f, 0.25f, 0.32f, 1.0f);
	bool bVolumetricFog = true;
	float VolumetricExtinction = 1.0f;
	FColor VolumetricAlbedo = FColor(195, 210, 230, 255);

	float BloomIntensity = 0.28f;
	float Saturation = 1.0f;

	/** Landscape 动态材质可选微调 (仅在开启 bUseDynamicMaterialInstance 时生效) */
	float LandscapeRoughnessMul = 1.0f;
	float LandscapeSnowOffsetCm = 0.0f;

	/** HUD 轻量大气颗粒覆盖强度 (0=无; >0 绘制细雨或战火余烬) */
	float PrecipIntensity = 0.0f;
	bool bPrecipAsh = false;
};

/**
 * 军团战场动态气象控制器:
 *  - 根据星区 ID、冲突烈度、任务危险星级与战场种子选出气象预设 (支持 -LegionWeather=0..5 覆盖);
 *  - 将场景中的 DirectionalLight / SkyLight / ExponentialHeightFog / PostProcessVolume 切换为可调状态并注入参数。
 */
class EBBINGRAINBOW_API FLegionWeatherSystem
{
public:
	static const FLegionWeatherSpec& GetSpec(ELegionWeatherPreset Preset);
	static FString GetPresetName(ELegionWeatherPreset Preset);

	/** 按银河局势与种子加权抽取气象预设 (命令行 -LegionWeather=0..5 优先) */
	static ELegionWeatherPreset SelectPreset(int32 Seed, int32 SectorId, float ConflictIntensity, int32 DangerRating);

	/** 应用气象到当前世界的光照/雾效/后处理与地形材质 */
	static void Apply(UWorld* World, ELegionWeatherPreset Preset, float BattleAxisYawDeg, int32 Seed);
};
