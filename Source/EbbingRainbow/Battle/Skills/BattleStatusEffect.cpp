// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleStatusEffect.h"

UBattleStatusEffect::UBattleStatusEffect()
{
	BaseDurationRounds = 2;
	StackingPolicy = EStatusStackingPolicy::StackAndRefresh;
	MaxStacks = 3;
	TickTiming = EStatusTickTiming::RoundEnd;
}