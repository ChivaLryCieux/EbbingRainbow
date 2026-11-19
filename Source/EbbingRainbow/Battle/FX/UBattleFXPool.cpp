// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/FX/UBattleFXPool.h"
#include "Battle/FX/BattleFloatingText.h"
#include "UI/ERUITheme.h"
#include "Engine/World.h"

UBattleFXPool* UBattleFXPool::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleFXPool>() : nullptr;
}

ABattleFloatingText* UBattleFXPool::SpawnFloatingText(const FVector& Location, const FString& Text,
	const FColor& Color, float WorldSize)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ABattleFloatingText* Instance = nullptr;

	// 先吃空闲栈
	while (FreeTexts.Num() > 0 && !Instance)
	{
		Instance = FreeTexts.Pop();
	}

	// 无空闲且到上限 -> 偷最老的活跃实例 (它还在放动画, 直接换内容)
	if (!Instance && AllTexts.Num() >= MaxPooledTexts)
	{
		for (TObjectPtr<ABattleFloatingText>& Candidate : AllTexts)
		{
			if (Candidate && !FreeTexts.Contains(Candidate))
			{
				Instance = Candidate;
				break;
			}
		}
		AllTexts.RemoveAll([](const TObjectPtr<ABattleFloatingText>& P) { return !P; });
	}

	if (!Instance)
	{
		UClass* SpawnCls = ABattleFloatingText::StaticClass();
		if (const UERUISettings* UISettings = UERUISettings::Get())
		{
			if (!UISettings->BattleFloatingTextClass.IsNull())
			{
				if (UClass* Loaded = UISettings->BattleFloatingTextClass.LoadSynchronous())
				{
					if (Loaded->IsChildOf(ABattleFloatingText::StaticClass()))
					{
						SpawnCls = Loaded;
					}
				}
			}
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Instance = World->SpawnActor<ABattleFloatingText>(SpawnCls, Location, FRotator::ZeroRotator, Params);
		if (Instance)
		{
			AllTexts.Add(Instance);
		}
	}

	if (Instance)
	{
		Instance->PoolAcquire(Location, Text, Color, WorldSize);
	}
	return Instance;
}

void UBattleFXPool::ReleaseFloatingText(ABattleFloatingText* Text)
{
	if (!Text)
	{
		return;
	}
	Text->PoolRelease();
	FreeTexts.Add(Text);
}
