// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Core/UBattleContext.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Units/BattleUnit.h"
#include "Engine/World.h"

UBattleContext* UBattleContext::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleContext>() : nullptr;
}

void UBattleContext::RegisterUnit(ABattleUnit* Unit)
{
	if (Unit && !Units.Contains(Unit))
	{
		Units.Add(Unit);
		++UnitsRevision;
	}
}

void UBattleContext::UnregisterUnit(ABattleUnit* Unit)
{
	if (Units.Remove(Unit) > 0)
	{
		++UnitsRevision;
	}
}

FBattleRandomStream* UBattleContext::GetRng()
{
	if (Manager)
	{
		return &Manager->GetRng();
	}
	// 兜底流 (无管理器的孤立场景): 固定种子, 保证至少可复现
	if (FallbackRng.InitialSeed == 0)
	{
		FallbackRng.Init(0x5EED1234);
	}
	return &FallbackRng;
}
