// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/PainterlyStyleSubsystem.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogPainterlyStyle, Log, All);

/** Last created subsystem, so the console commands can reach it without a world context. */
static TWeakObjectPtr<UPainterlyStyleSubsystem> GPainterlySubsystem;

static const TCHAR* GPainterlyConfigSection = TEXT("/Script/EbbingRainbow.PainterlyStyle");
static const TCHAR* GPainterlyMaterialKey = TEXT("MaterialPath");
static const TCHAR* GPainterlyDefaultMaterial = TEXT("/Game/Style/Painterly/MI_PP_Painterly");

static TAutoConsoleVariable<int32> CVarPainterlyStyle(
	TEXT("Paint.Style"),
	1,
	TEXT("Enable the painterly (thick paint) post process.\n")
	TEXT("0: off (original renderer)\n")
	TEXT("1: on"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarPainterlyDebug(
	TEXT("Paint.DebugMode"),
	0,
	TEXT("Painterly debug view.\n")
	TEXT("0: final, 1: stroke flow, 2: ink mask, 3: impasto normal, 4: raw Kuwahara, 5: edge confidence"),
	ECVF_Cheat);

bool UPainterlyStyleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Only real game worlds: no editor preview / thumbnail worlds.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UPainterlyStyleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	ApplyStyle();
}

void UPainterlyStyleSubsystem::Deinitialize()
{
	if (GPainterlySubsystem.Get() == this)
	{
		GPainterlySubsystem.Reset();
	}

	Volume = nullptr;
	StyleMaterial = nullptr;
	StyleMID = nullptr;
	Super::Deinitialize();
}

TStatId UPainterlyStyleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPainterlyStyleSubsystem, STATGROUP_Tickables);
}

void UPainterlyStyleSubsystem::Tick(float DeltaTime)
{
	// Poll the cvars so the look can be toggled and inspected live while playing.
	const float Wanted = (CVarPainterlyStyle.GetValueOnGameThread() != 0) ? 1.0f : 0.0f;
	if (!FMath::IsNearlyEqual(Wanted, StyleWeight))
	{
		if (Wanted > 0.0f)
		{
			// Re-apply: the volume is created lazily so Paint.Style 0 at startup costs nothing.
			ApplyStyle();
		}
		else
		{
			SetStyleWeight(0.0f);
		}
	}

	const int32 WantedDebug = FMath::Clamp(CVarPainterlyDebug.GetValueOnGameThread(), 0, 5);
	if (WantedDebug != AppliedDebugMode && StyleMID)
	{
		AppliedDebugMode = WantedDebug;
		SetDebugMode(static_cast<float>(WantedDebug));
	}
}

void UPainterlyStyleSubsystem::ApplyStyle()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (CVarPainterlyStyle.GetValueOnGameThread() == 0)
	{
		StyleWeight = 0.0f;
		return;
	}

	StyleWeight = 1.0f;
	APostProcessVolume* NewVolume = EnsureVolume(World);
	if (NewVolume)
	{
		NewVolume->BlendWeight = StyleWeight;
	}
}

void UPainterlyStyleSubsystem::SetStyleWeight(float InWeight)
{
	StyleWeight = FMath::Clamp(InWeight, 0.0f, 1.0f);

	if (Volume)
	{
		Volume->BlendWeight = StyleWeight;
	}
}

UMaterialInterface* UPainterlyStyleSubsystem::GetStyleMaterial()
{
	if (bMaterialResolved)
	{
		return StyleMaterial;
	}

	FString MaterialPath;
	if (!GConfig->GetString(GPainterlyConfigSection, GPainterlyMaterialKey, MaterialPath, GGameIni) || MaterialPath.IsEmpty() || MaterialPath == TEXT("None"))
	{
		bMaterialResolved = true;
		StyleMaterial = nullptr;
		UE_LOG(LogPainterlyStyle, Log, TEXT("Painterly style disabled globally (MaterialPath is empty). Manual volume control active."));
		return nullptr;
	}

	StyleMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	bMaterialResolved = true;

	if (!StyleMaterial)
	{
		UE_LOG(LogPainterlyStyle, Warning,
			TEXT("Painterly style material '%s' not found. Run "
				"Scripts/Painterly/build_painterly_postprocess.py to create it, "
				"or clear [/Script/EbbingRainbow.PainterlyStyle] MaterialPath in DefaultGame.ini."),
			*MaterialPath);
	}
	else if (const UMaterial* BaseMaterial = StyleMaterial->GetMaterial())
	{
		UE_LOG(LogPainterlyStyle, Log,
			TEXT("Style material '%s': domain=%d blendableLocation=%d blendMode=%d"),
			*StyleMaterial->GetPathName(),
			static_cast<int32>(BaseMaterial->MaterialDomain),
			static_cast<int32>(BaseMaterial->BlendableLocation),
			static_cast<int32>(BaseMaterial->BlendMode));

		if (BaseMaterial->MaterialDomain != MD_PostProcess)
		{
			UE_LOG(LogPainterlyStyle, Error,
				TEXT("Material domain is not Post Process - the painterly look will not render."));
		}
	}

	return StyleMaterial;
}

APostProcessVolume* UPainterlyStyleSubsystem::EnsureVolume(UWorld* World)
{
	if (Volume)
	{
		return Volume;
	}

	UMaterialInterface* Material = GetStyleMaterial();
	if (!Material)
	{
		return nullptr;
	}

	// Drive a dynamic instance so art direction can tune parameters (and the debug
	// views) without recompiling anything.
	StyleMID = UMaterialInstanceDynamic::Create(Material, this);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.Name = TEXT("PainterlyStyleVolume");

	Volume = World->SpawnActor<APostProcessVolume>(
		APostProcessVolume::StaticClass(), FTransform::Identity, SpawnParams);

	if (!Volume)
	{
		UE_LOG(LogPainterlyStyle, Warning, TEXT("Failed to spawn the painterly post process volume."));
		return nullptr;
	}

	Volume->bUnbound = true;
	Volume->bEnabled = true;
	Volume->Priority = 100.0f;
	Volume->BlendRadius = 0.0f;
	Volume->BlendWeight = 1.0f;
	Volume->Settings.WeightedBlendables.Array.Empty();
	Volume->Settings.WeightedBlendables.Array.Add(
		FWeightedBlendable(1.0f, StyleMID ? static_cast<UMaterialInterface*>(StyleMID) : Material));

	GPainterlySubsystem = this;

	UE_LOG(LogPainterlyStyle, Log, TEXT("Painterly post process applied: %s"), *Material->GetPathName());
	UE_LOG(LogPainterlyStyle, Log,
		TEXT("Volume: unbound=%d enabled=%d weight=%.2f priority=%.1f blendables=%d"),
		Volume->bUnbound ? 1 : 0, Volume->bEnabled ? 1 : 0, Volume->BlendWeight, Volume->Priority,
		Volume->Settings.WeightedBlendables.Array.Num());
	return Volume;
}

void UPainterlyStyleSubsystem::SetDebugMode(float InMode)
{
	if (StyleMID)
	{
		StyleMID->SetScalarParameterValue(TEXT("P_DebugMode"), FMath::Clamp(InMode, 0.0f, 5.0f));
		UE_LOG(LogPainterlyStyle, Log, TEXT("Paint debug mode = %d"), FMath::RoundToInt(InMode));
	}
}

bool UPainterlyStyleSubsystem::SetScalarParameter(const FName& ParameterName, float Value)
{
	if (!StyleMID)
	{
		return false;
	}

	// Unknown names are simply ignored by the material, but they are almost always
	// typos, so keep a note in the log.
	StyleMID->SetScalarParameterValue(ParameterName, Value);
	UE_LOG(LogPainterlyStyle, Log, TEXT("%s = %.3f"), *ParameterName.ToString(), Value);
	return true;
}

// ---------------------------------------------------------------------------
//  Debug helpers: Paint.Shot [Name] -> writes Saved/Screenshots/<Name>.png
// ---------------------------------------------------------------------------
static void PaintShotCommand(const TArray<FString>& Args)
{
	const FString Name = Args.Num() > 0 ? Args[0] : FString(TEXT("PaintShot"));
	FScreenshotRequest::RequestScreenshot(Name, false, false);
	UE_LOG(LogPainterlyStyle, Log, TEXT("Screenshot requested: %s"), *Name);
}

static FAutoConsoleCommand GPaintShotCommand(
	TEXT("Paint.Shot"),
	TEXT("Paint.Shot [Name] : capture the current frame to Saved/Screenshots (useful for A/B of Paint.Style)."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&PaintShotCommand));

static void PaintDebugCommand(const TArray<FString>& Args)
{
	CVarPainterlyDebug->Set(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, ECVF_SetByConsole);
}

static FAutoConsoleCommand GPaintDebugCommand(
	TEXT("Paint.Debug"),
	TEXT("Paint.Debug <0..5> : alias for the Paint.DebugMode cvar."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&PaintDebugCommand));

static void PaintParamCommand(const TArray<FString>& Args)
{
	if (UPainterlyStyleSubsystem* Subsystem = GPainterlySubsystem.Get())
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogPainterlyStyle, Warning, TEXT("Usage: Paint.Param <P_Name> <Value>"));
			return;
		}

		if (!Subsystem->SetScalarParameter(FName(*Args[0]), FCString::Atof(*Args[1])))
		{
			UE_LOG(LogPainterlyStyle, Warning, TEXT("Unknown painterly parameter: %s"), *Args[0]);
		}
	}
}

static FAutoConsoleCommand GPaintParamCommand(
	TEXT("Paint.Param"),
	TEXT("Paint.Param <P_Name> <Value> : set any painterly scalar parameter live, e.g. Paint.Param P_BrushRadius 12."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&PaintParamCommand));
