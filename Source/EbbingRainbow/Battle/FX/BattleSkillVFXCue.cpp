// Copyright Ebbing Rainbow. All Rights Reserved.

#include "Battle/FX/BattleSkillVFXCue.h"
#include "NiagaraComponent.h"

ABattleSkillVFXCue::ABattleSkillVFXCue()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 2.5f;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ABattleSkillVFXCue::PlayCue(const FBattleVFXSpawnContext& InContext)
{
	CachedContext = InContext;
	SetLifeSpan(FMath::Max(InContext.Duration + 1.2f, 2.0f));
	OnPlaySkillCue(InContext);
}

void ABattleSkillVFXCue::ApplyContextToNiagara(UNiagaraComponent* NiagaraComp, const FBattleVFXSpawnContext& Context,
	FName ColorParam, FName StartParam, FName EndParam, FName RadiusParam)
{
	if (!NiagaraComp)
	{
		return;
	}

	if (!ColorParam.IsNone())
	{
		NiagaraComp->SetVariableLinearColor(ColorParam, Context.ThemeColor);
	}
	if (!StartParam.IsNone())
	{
		NiagaraComp->SetVariablePosition(StartParam, Context.StartLocation);
	}
	if (!EndParam.IsNone())
	{
		NiagaraComp->SetVariablePosition(EndParam, Context.TargetLocation);
	}
	if (!RadiusParam.IsNone() && Context.Radius > 0.0f)
	{
		NiagaraComp->SetVariableFloat(RadiusParam, Context.Radius);
	}
}
