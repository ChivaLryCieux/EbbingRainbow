// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleEventHub.h"
#include "Engine/World.h"

UBattleEventHub* UBattleEventHub::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleEventHub>() : nullptr;
}