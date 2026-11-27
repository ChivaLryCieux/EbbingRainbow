// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionWeather.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Core/ERLog.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const FLegionWeatherSpec GWeatherTable[] = {
		// 0: ClearMorning — 清冽晨曦: 低角度暖金主光 + 冷蓝天光 + 轻薄谷雾
		{
			ELegionWeatherPreset::ClearMorning, TEXT("清冽晨曦"),
			-22.0f, -66.0f, 5.8f, FColor(255, 232, 194, 255),
			0.90f, FColor(176, 206, 240, 255),
			0.010f, 0.28f, 1400.0f, FLinearColor(0.22f, 0.28f, 0.36f, 1.0f), true, 0.9f, FColor(200, 218, 242, 255),
			0.28f, 1.02f, 1.00f, 0.0f, 0.0f, false
		},
		// 1: HighNoon — 烈阳正午: 高位强光 + 通透视野 + 清晰立体明暗
		{
			ELegionWeatherPreset::HighNoon, TEXT("烈阳正午"),
			-50.0f, -48.0f, 6.4f, FColor(255, 246, 232, 255),
			0.95f, FColor(190, 214, 245, 255),
			0.005f, 0.30f, 2200.0f, FLinearColor(0.26f, 0.32f, 0.40f, 1.0f), false, 0.6f, FColor(215, 228, 245, 255),
			0.24f, 1.05f, 1.02f, 400.0f, 0.0f, false
		},
		// 2: DuskGold — 残阳暮色: 极低斜阳长影 + 暖琥珀暮光
		{
			ELegionWeatherPreset::DuskGold, TEXT("残阳暮色"),
			-14.0f, -108.0f, 5.4f, FColor(255, 192, 128, 255),
			0.78f, FColor(152, 174, 216, 255),
			0.016f, 0.22f, 1000.0f, FLinearColor(0.32f, 0.24f, 0.19f, 1.0f), true, 1.2f, FColor(235, 204, 178, 255),
			0.34f, 1.06f, 0.98f, 0.0f, 0.0f, false
		},
		// 3: MountainMist — 山岚浓雾: 柔和散射 + 谷底积雾 + 静谧冷青
		{
			ELegionWeatherPreset::MountainMist, TEXT("山岚浓雾"),
			-28.0f, 74.0f, 3.8f, FColor(228, 238, 250, 255),
			1.10f, FColor(186, 208, 230, 255),
			0.032f, 0.17f, 600.0f, FLinearColor(0.24f, 0.30f, 0.36f, 1.0f), true, 1.8f, FColor(208, 222, 236, 255),
			0.25f, 0.88f, 0.92f, -600.0f, 0.0f, false
		},
		// 4: OvercastStorm — 阴云骤雨: 压低主光 + 冷灰雨幕 + 湿润地表反光
		{
			ELegionWeatherPreset::OvercastStorm, TEXT("阴云骤雨"),
			-34.0f, -85.0f, 2.7f, FColor(196, 212, 232, 255),
			1.05f, FColor(148, 166, 192, 255),
			0.026f, 0.19f, 700.0f, FLinearColor(0.16f, 0.20f, 0.26f, 1.0f), true, 1.6f, FColor(168, 182, 202, 255),
			0.22f, 0.78f, 0.68f, 0.0f, 1.0f, false
		},
		// 5: AshConflict — 战火硝烟: 高烈度星区专用, 浑浊暖灰尘雾 + 飘散余烬
		{
			ELegionWeatherPreset::AshConflict, TEXT("战火硝烟"),
			-19.0f, 112.0f, 4.2f, FColor(255, 184, 134, 255),
			0.74f, FColor(168, 148, 138, 255),
			0.024f, 0.20f, 800.0f, FLinearColor(0.28f, 0.20f, 0.16f, 1.0f), true, 1.5f, FColor(218, 180, 152, 255),
			0.36f, 0.86f, 0.96f, 600.0f, 0.85f, true
		}
	};
}

const FLegionWeatherSpec& FLegionWeatherSystem::GetSpec(ELegionWeatherPreset Preset)
{
	const int32 Idx = FMath::Clamp((int32)Preset, 0, UE_ARRAY_COUNT(GWeatherTable) - 1);
	return GWeatherTable[Idx];
}

FString FLegionWeatherSystem::GetPresetName(ELegionWeatherPreset Preset)
{
	return FString(GetSpec(Preset).DisplayName);
}

ELegionWeatherPreset FLegionWeatherSystem::SelectPreset(int32 Seed, int32 SectorId, float ConflictIntensity, int32 DangerRating)
{
	int32 Forced = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("-LegionWeather="), Forced) &&
		Forced >= 0 && Forced < UE_ARRAY_COUNT(GWeatherTable))
	{
		return static_cast<ELegionWeatherPreset>(Forced);
	}

	// 按星区与冲突烈度分配权重: 烈度越高, 硝烟/骤雨/浓雾占比越高
	float Weights[UE_ARRAY_COUNT(GWeatherTable)] = { 24.0f, 18.0f, 18.0f, 16.0f, 12.0f, 8.0f };

	const float Conflict = FMath::Clamp(ConflictIntensity, 0.0f, 1.0f);
	const float Danger = FMath::Clamp((float)DangerRating, 1.0f, 5.0f);
	if (Conflict >= 0.65f || Danger >= 3.5f)
	{
		Weights[(int32)ELegionWeatherPreset::AshConflict]   += 18.0f * Conflict + (Danger - 2.0f) * 4.0f;
		Weights[(int32)ELegionWeatherPreset::OvercastStorm] += 10.0f * Conflict;
		Weights[(int32)ELegionWeatherPreset::DuskGold]      += 6.0f;
		Weights[(int32)ELegionWeatherPreset::HighNoon]      *= 0.55f;
	}

	// 不同星区偏向不同气象底色 (如 苍白裂隙/霜桥中继站偏雾雪, 赫尔墨斯前哨偏战火)
	switch (SectorId)
	{
	case 0: Weights[(int32)ELegionWeatherPreset::DuskGold]      += 10.0f; Weights[(int32)ELegionWeatherPreset::AshConflict] += 8.0f; break;
	case 1: Weights[(int32)ELegionWeatherPreset::HighNoon]      += 10.0f; Weights[(int32)ELegionWeatherPreset::DuskGold]    += 8.0f; break;
	case 2: Weights[(int32)ELegionWeatherPreset::MountainMist]  += 18.0f; Weights[(int32)ELegionWeatherPreset::OvercastStorm] += 8.0f; break;
	case 3: Weights[(int32)ELegionWeatherPreset::AshConflict]   += 18.0f; Weights[(int32)ELegionWeatherPreset::OvercastStorm] += 8.0f; break;
	case 4: Weights[(int32)ELegionWeatherPreset::MountainMist]  += 14.0f; Weights[(int32)ELegionWeatherPreset::ClearMorning] += 10.0f; break;
	case 5: Weights[(int32)ELegionWeatherPreset::ClearMorning]  += 12.0f; Weights[(int32)ELegionWeatherPreset::OvercastStorm] += 10.0f; break;
	default: break;
	}

	float TotalW = 0.0f;
	for (float W : Weights) { TotalW += FMath::Max(0.0f, W); }

	FRandomStream Rng(Seed ^ 0x5A17C3);
	float Roll = Rng.FRand() * FMath::Max(1.0f, TotalW);
	for (int32 i = 0; i < UE_ARRAY_COUNT(GWeatherTable); ++i)
	{
		Roll -= FMath::Max(0.0f, Weights[i]);
		if (Roll <= 0.0f)
		{
			return GWeatherTable[i].Preset;
		}
	}
	return ELegionWeatherPreset::ClearMorning;
}

void FLegionWeatherSystem::Apply(UWorld* World, ELegionWeatherPreset Preset, float BattleAxisYawDeg, int32 Seed)
{
	if (!World) { return; }

	const FLegionWeatherSpec& Spec = GetSpec(Preset);
	FRandomStream Rng(Seed ^ 0x2F41B9);

	// 太阳方位以战线侧逆/侧顺光为基准叠加少量种子扰动, 保证山脊立体明暗而不正逆刺眼
	const float MirrorSide = (Rng.FRand() > 0.5f) ? 1.0f : -1.0f;
	const float SunYaw = FRotator::NormalizeAxis(
		BattleAxisYawDeg + Spec.SunYawOffsetDeg * MirrorSide + Rng.FRandRange(-14.0f, 14.0f));
	const float SunPitch = FMath::Clamp(Spec.SunPitchDeg + Rng.FRandRange(-3.0f, 3.0f), -65.0f, -11.0f);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) { continue; }

		if (ADirectionalLight* Sun = Cast<ADirectionalLight>(A))
		{
			if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				// 关卡原灯光常为 Stationary, 切 Movable 后运行时旋转与调光才完全生效
				LC->SetMobility(EComponentMobility::Movable);
				Sun->SetActorRotation(FRotator(SunPitch, SunYaw, 0.0f));
				LC->SetIntensity(Spec.SunIntensity);
				LC->SetLightColor(Spec.SunColor);
			}
		}
		else if (ASkyLight* Sky = Cast<ASkyLight>(A))
		{
			if (USkyLightComponent* SC = Sky->GetLightComponent())
			{
				SC->SetMobility(EComponentMobility::Movable);
				SC->SetIntensity(Spec.SkyIntensity);
				SC->SetLightColor(Spec.SkyColor);
				SC->RecaptureSky();
			}
		}
		else if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(A))
		{
			if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
			{
				FC->SetMobility(EComponentMobility::Movable);
				FC->SetFogDensity(Spec.FogDensity);
				FC->SetFogHeightFalloff(Spec.FogHeightFalloff);
				FC->SetStartDistance(Spec.FogStartDistance);
				FC->SetFogInscatteringColor(Spec.FogInscatter);
				FC->SetVolumetricFog(Spec.bVolumetricFog);
				FC->SetVolumetricFogExtinctionScale(Spec.VolumetricExtinction);
				FC->SetVolumetricFogAlbedo(Spec.VolumetricAlbedo);
			}
		}
		else if (APostProcessVolume* PPV = Cast<APostProcessVolume>(A))
		{
			PPV->Settings.bOverride_BloomIntensity = true;
			PPV->Settings.BloomIntensity = Spec.BloomIntensity;
			PPV->Settings.bOverride_ColorSaturation = true;
			PPV->Settings.ColorSaturation = FVector4(Spec.Saturation, Spec.Saturation, Spec.Saturation, 1.0f);
		}
		else if (ALandscapeProxy* Land = Cast<ALandscapeProxy>(A))
		{
			if (Land->bUseDynamicMaterialInstance)
			{
				Land->SetLandscapeMaterialScalarParameterValue(TEXT("MW_RoughnessMultiplier"), Spec.LandscapeRoughnessMul);
				Land->SetLandscapeMaterialScalarParameterValue(TEXT("MW_SnowWorldPosition"), 4000.0f + Spec.LandscapeSnowOffsetCm);
			}
		}
	}

	UE_LOG(LogERLegion, Log,
		TEXT("[Weather] Applied '%s' (SunPitch=%.1f, SunYaw=%.1f, Intensity=%.2f, Fog=%.4f, Seed=%d)"),
		Spec.DisplayName, SunPitch, SunYaw, Spec.SunIntensity, Spec.FogDensity, Seed);
}
