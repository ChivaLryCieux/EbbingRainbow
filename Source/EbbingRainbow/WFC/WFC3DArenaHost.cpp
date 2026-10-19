// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DArenaHost.h"
#include "Engine/World.h"

AWFC3DArenaHost::AWFC3DArenaHost()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AWFC3DArenaHost::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoGenerateOnBeginPlay)
	{
		GeneratePreview();
	}
}

bool AWFC3DArenaHost::Generate(const FWFCGenerationRequest& Request)
{
	LastResult = FWFCGenerationResult();

	TSubclassOf<AWFC3DArenaGeneratorBase> GeneratorClass;
	if (StyleTable)
	{
		GeneratorClass = StyleTable->Resolve(Request.Style);
	}

	if (!GeneratorClass)
	{
		GeneratorClass = FallbackGeneratorClass;
		if (GeneratorClass && StyleTable)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WFC Host] StyleTable has no entry for style %d; falling back to '%s'."),
				static_cast<int32>(Request.Style), *GeneratorClass->GetName());
		}
	}

	if (!GeneratorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Host] No generator class for %s (StyleTable=%s, Fallback=null)."),
			*Request.ToString(), StyleTable ? TEXT("set") : TEXT("null"));
		return false;
	}

	if (!EnsureActiveGenerator(GeneratorClass))
	{
		return false;
	}

	// Host 的位置就是竞技场原点；每次生成前同步，避免 Host 被移动后生成器留在旧位置。
	// 缩放保持 1：格子世界尺寸由具体生成器提供，不能叠加 Actor 缩放。
	FTransform GeneratorTransform = GetActorTransform();
	GeneratorTransform.SetScale3D(FVector::OneVector);
	ActiveGenerator->SetActorTransform(GeneratorTransform);

	const bool bOk = ActiveGenerator->Generate(Request);
	LastResult = ActiveGenerator->GetLastResult();

	UE_LOG(LogTemp, Log, TEXT("[WFC Host] %s -> %s (seed=%d, %.2f ms)."),
		*Request.ToString(), bOk ? TEXT("OK") : TEXT("FAILED"),
		LastResult.UsedSeed, LastResult.DurationMs);

	return bOk;
}

bool AWFC3DArenaHost::GeneratePreview()
{
	return Generate(PreviewRequest);
}

void AWFC3DArenaHost::Clear()
{
	if (ActiveGenerator && IsValid(ActiveGenerator))
	{
		ActiveGenerator->ClearGenerated();
	}
	LastResult = FWFCGenerationResult();
}

bool AWFC3DArenaHost::EnsureActiveGenerator(TSubclassOf<AWFC3DArenaGeneratorBase> Class)
{
	if (ActiveGenerator && IsValid(ActiveGenerator) && ActiveGenerator->GetClass() == Class)
	{
		return true;
	}

	if (ActiveGenerator && IsValid(ActiveGenerator))
	{
		ActiveGenerator->Destroy();
	}
	ActiveGenerator = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Host] EnsureActiveGenerator failed: no world."));
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		Class,
		GetActorTransform(),
		Params);
	ActiveGenerator = Cast<AWFC3DArenaGeneratorBase>(SpawnedActor);

	if (!ActiveGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("[WFC Host] Failed to spawn generator class '%s'."), *Class->GetName());
		return false;
	}

	return true;
}
