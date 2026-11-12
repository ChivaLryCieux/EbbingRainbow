// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Skills/BattleInteractionTable.h"

const FStatusReactionRow* UBattleInteractionTable::FindReaction(EDamageType DamageType, const FGameplayTagContainer& TargetStatusTags) const
{
	for (const FStatusReactionRow& Row : Rows)
	{
		if (Row.TriggerDamage != DamageType)
		{
			continue;
		}
		if (Row.RequiredStatus.IsValid() && !TargetStatusTags.HasTag(Row.RequiredStatus))
		{
			continue;
		}
		return &Row;
	}
	return nullptr;
}