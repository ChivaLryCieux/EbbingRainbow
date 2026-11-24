// Copyright Ebbing Rainbow. All Rights Reserved.

#include "Battle/FX/BattleVFXSubsystem.h"
#include "Battle/FX/BattleProceduralVFXActor.h"
#include "Battle/FX/BattleSkillVFXCue.h"
#include "Battle/Skills/BattleEventHub.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "Battle/Units/BattleUnit.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

static TAutoConsoleVariable<int32> CVarVFXPlayerOnly(
	TEXT("ER.VFX.PlayerOnly"),
	0,
	TEXT("1 = 仅我方战棋施放技能显示特效; 0 = 我方按学派主题色，敌方用朱红弹道。"),
	ECVF_Default);

UBattleVFXSubsystem* UBattleVFXSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleVFXSubsystem>() : nullptr;
}

void UBattleVFXSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UBattleEventHub* Hub = UBattleEventHub::Get(&InWorld))
	{
		CastHandle = Hub->OnSkillCastStarted.AddUObject(this, &UBattleVFXSubsystem::HandleSkillCastStarted);
		AreaHandle = Hub->OnSkillAreaTriggered.AddUObject(this, &UBattleVFXSubsystem::HandleSkillAreaTriggered);
		ChainHandle = Hub->OnChainArcTriggered.AddUObject(this, &UBattleVFXSubsystem::HandleChainArcTriggered);
		HitHandle = Hub->OnHitResolved.AddUObject(this, &UBattleVFXSubsystem::HandleHitResolved);
	}
}

void UBattleVFXSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UBattleEventHub* Hub = UBattleEventHub::Get(World))
		{
			Hub->OnSkillCastStarted.Remove(CastHandle);
			Hub->OnSkillAreaTriggered.Remove(AreaHandle);
			Hub->OnChainArcTriggered.Remove(ChainHandle);
			Hub->OnHitResolved.Remove(HitHandle);
		}
	}
	Super::Deinitialize();
}

FLinearColor UBattleVFXSubsystem::ResolveSkillThemeColor(const ABattleUnit* Caster, const UBattleSkillData* Skill)
{
	if (Skill && Skill->VFX.bOverrideColor)
	{
		return Skill->VFX.ColorOverride;
	}

	// 敌方单位统一偏朱红敌意色，避免和玩家“热学红”混淆时可清晰识别施放归属
	if (Caster && Caster->Team == EBattleTeam::Enemy)
	{
		return FLinearColor(0.95f, 0.24f, 0.16f, 1.0f);
	}

	if (Skill)
	{
		return UBattleSkillData::GetDisciplineDefaultColor(Skill->Discipline);
	}

	return FLinearColor(0.20f, 0.45f, 0.95f, 1.0f);
}

EVFXDeliveryPattern UBattleVFXSubsystem::ResolveDeliveryPattern(const UBattleSkillData* Skill)
{
	if (!Skill)
	{
		return EVFXDeliveryPattern::MeleeSlash;
	}
	if (Skill->VFX.Pattern != EVFXDeliveryPattern::Auto)
	{
		return Skill->VFX.Pattern;
	}

	if (Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
	{
		return Skill->AoE.IsArea() ? EVFXDeliveryPattern::SelfNova : EVFXDeliveryPattern::SelfBuff;
	}
	if (Skill->AoE.IsArea())
	{
		return (Skill->RangeProfile == ESkillRangeProfile::Ranged)
			? EVFXDeliveryPattern::ProjectileArc
			: EVFXDeliveryPattern::GroundAoE;
	}
	if (Skill->Chain.IsChain())
	{
		return EVFXDeliveryPattern::ChainArc;
	}
	if (Skill->RangeProfile == ESkillRangeProfile::Ranged)
	{
		return EVFXDeliveryPattern::RangedBeam;
	}
	return EVFXDeliveryPattern::MeleeSlash;
}

FVector UBattleVFXSubsystem::GetUnitAnchorLocation(const ABattleUnit* Unit, float HeightFraction) const
{
	if (!Unit)
	{
		return FVector::ZeroVector;
	}
	const float HalfH = Unit->GetCapsuleHalfHeight();
	return Unit->GetActorLocation() + FVector(0.0f, 0.0f, HalfH * HeightFraction);
}

const TSoftObjectPtr<UNiagaraSystem>* UBattleVFXSubsystem::SelectNiagaraSlot(
	const FBattleSkillVFXConfig& Config, EBattleVFXCueStage Stage, EVFXDeliveryPattern Pattern) const
{
	switch (Stage)
	{
	case EBattleVFXCueStage::Area:
		return !Config.AreaNiagara.IsNull() ? &Config.AreaNiagara : nullptr;

	case EBattleVFXCueStage::ChainJump:
		return !Config.BeamOrTrailNiagara.IsNull() ? &Config.BeamOrTrailNiagara : nullptr;

	case EBattleVFXCueStage::Impact:
	case EBattleVFXCueStage::Reaction:
		return !Config.ImpactNiagara.IsNull() ? &Config.ImpactNiagara : nullptr;

	case EBattleVFXCueStage::Cast:
	default:
		if ((Pattern == EVFXDeliveryPattern::RangedBeam || Pattern == EVFXDeliveryPattern::ProjectileArc
			|| Pattern == EVFXDeliveryPattern::ChainArc) && !Config.BeamOrTrailNiagara.IsNull())
		{
			return &Config.BeamOrTrailNiagara;
		}
		return !Config.CastNiagara.IsNull() ? &Config.CastNiagara : nullptr;
	}
}

void UBattleVFXSubsystem::HandleSkillCastStarted(ABattleUnit* Caster, const UBattleSkillData* Skill, ABattleUnit* Target,
	const FVector& TargetLocation, float HitDelay)
{
	if (!Caster || !Skill)
	{
		return;
	}
	if (CVarVFXPlayerOnly.GetValueOnGameThread() > 0 && Caster->Team != EBattleTeam::Player)
	{
		return;
	}

	const EVFXDeliveryPattern Pattern = ResolveDeliveryPattern(Skill);
	const FLinearColor Theme = ResolveSkillThemeColor(Caster, Skill);
	const FVector StartPos = GetUnitAnchorLocation(Caster, 0.25f);
	const FVector EndPos = Target ? GetUnitAnchorLocation(Target, 0.18f) : TargetLocation;

	FBattleVFXSpawnContext BaseCtx;
	BaseCtx.Stage = EBattleVFXCueStage::Cast;
	BaseCtx.ResolvedPattern = (Pattern == EVFXDeliveryPattern::ChainArc) ? EVFXDeliveryPattern::RangedBeam : Pattern;
	BaseCtx.Caster = Caster;
	BaseCtx.Target = Target;
	BaseCtx.Skill = Skill;
	BaseCtx.StartLocation = StartPos;
	BaseCtx.TargetLocation = EndPos;
	BaseCtx.ThemeColor = Theme;
	BaseCtx.Radius = Skill->AoE.Radius;
	BaseCtx.ScaleMultiplier = Skill->VFX.ScaleMultiplier;
	BaseCtx.bJaggedArc = (Skill->Discipline == ESkillDiscipline::Electric);

	if (Pattern == EVFXDeliveryPattern::ProjectileArc)
	{
		if (HitDelay > 0.08f)
		{
			// 抛射物在起手瞬间飞出，飞行时长严格贴合 HitDelay，落地即结算 AoE/Hit
			BaseCtx.Duration = HitDelay;
			PlayCue(BaseCtx);
		}
		return;
	}

	if (Pattern == EVFXDeliveryPattern::SelfBuff)
	{
		BaseCtx.Duration = 0.55f;
		ScheduleCue(BaseCtx, FMath::Clamp(HitDelay * 0.4f, 0.0f, 0.2f));
		return;
	}

	if (Pattern == EVFXDeliveryPattern::SelfNova)
	{
		// 起手聚能环; 真实半径冲击波在 HitDelay 落地时由 OnSkillAreaTriggered 触发
		BaseCtx.ResolvedPattern = EVFXDeliveryPattern::SelfBuff;
		BaseCtx.Duration = FMath::Max(0.28f, HitDelay);
		PlayCue(BaseCtx);
		return;
	}

	if (Pattern == EVFXDeliveryPattern::MeleeSlash)
	{
		BaseCtx.Duration = 0.28f;
		ScheduleCue(BaseCtx, FMath::Max(0.0f, HitDelay * 0.48f));
		return;
	}

	// RangedBeam / ChainArc (支持 HitCount > 1 连射，如酸蚀喷射三连发)
	const int32 Hits = FMath::Max(1, Skill->HitCount);
	const float FirstBeamDelay = FMath::Max(0.0f, HitDelay - 0.05f);
	for (int32 HitIdx = 0; HitIdx < Hits; ++HitIdx)
	{
		FBattleVFXSpawnContext BeamCtx = BaseCtx;
		BeamCtx.Duration = (Hits > 1) ? 0.18f : 0.28f;
		ScheduleCue(BeamCtx, FirstBeamDelay + (float)HitIdx * Skill->HitInterval);
	}
}

void UBattleVFXSubsystem::HandleSkillAreaTriggered(ABattleUnit* Caster, const UBattleSkillData* Skill,
	const FVector& TargetLocation, float Radius)
{
	if (!Caster || !Skill || Radius <= 0.0f)
	{
		return;
	}
	if (CVarVFXPlayerOnly.GetValueOnGameThread() > 0 && Caster->Team != EBattleTeam::Player)
	{
		return;
	}

	FBattleVFXSpawnContext Ctx;
	Ctx.Stage = EBattleVFXCueStage::Area;
	Ctx.ResolvedPattern = (Skill->TargetFilter == ESkillTargetFilter::SelfOnly)
		? EVFXDeliveryPattern::SelfNova
		: EVFXDeliveryPattern::GroundAoE;
	Ctx.Caster = Caster;
	Ctx.Skill = Skill;
	// 把圆心对齐地面 (单位根位置减去胶囊半高)
	FVector GroundPos = TargetLocation;
	GroundPos.Z -= Caster->GetCapsuleHalfHeight();
	Ctx.StartLocation = GroundPos;
	Ctx.TargetLocation = GroundPos;
	Ctx.ThemeColor = ResolveSkillThemeColor(Caster, Skill);
	Ctx.Radius = Radius;
	Ctx.Duration = 0.52f;
	Ctx.ScaleMultiplier = Skill->VFX.ScaleMultiplier;
	PlayCue(Ctx);
}

void UBattleVFXSubsystem::HandleChainArcTriggered(ABattleUnit* Caster, ABattleUnit* FromUnit, ABattleUnit* ToUnit,
	const UBattleSkillData* Skill, int32 JumpIndex, float Delay)
{
	if (!Caster || !FromUnit || !ToUnit || !Skill)
	{
		return;
	}
	if (CVarVFXPlayerOnly.GetValueOnGameThread() > 0 && Caster->Team != EBattleTeam::Player)
	{
		return;
	}

	FBattleVFXSpawnContext Ctx;
	Ctx.Stage = EBattleVFXCueStage::ChainJump;
	Ctx.ResolvedPattern = EVFXDeliveryPattern::ChainArc;
	Ctx.Caster = Caster;
	Ctx.Target = ToUnit;
	Ctx.Skill = Skill;
	Ctx.StartLocation = GetUnitAnchorLocation(FromUnit, 0.25f);
	Ctx.TargetLocation = GetUnitAnchorLocation(ToUnit, 0.22f);
	Ctx.ThemeColor = ResolveSkillThemeColor(Caster, Skill);
	Ctx.Duration = 0.24f;
	Ctx.ScaleMultiplier = Skill->VFX.ScaleMultiplier * FMath::Pow(0.88f, (float)JumpIndex);
	Ctx.bJaggedArc = true;

	ScheduleCue(Ctx, FMath::Max(0.0f, Delay - 0.03f));
}

void UBattleVFXSubsystem::HandleHitResolved(const FBattleHitResult& Hit)
{
	ABattleUnit* Attacker = Hit.Attacker.Get();
	ABattleUnit* Target = Hit.Target.Get();
	if (!Target)
	{
		return;
	}
	if (CVarVFXPlayerOnly.GetValueOnGameThread() > 0 && (!Attacker || Attacker->Team != EBattleTeam::Player))
	{
		return;
	}
	if (!Hit.bHit)
	{
		return;
	}

	FBattleVFXSpawnContext Ctx;
	Ctx.Stage = Hit.bReactionTriggered ? EBattleVFXCueStage::Reaction : EBattleVFXCueStage::Impact;
	Ctx.ResolvedPattern = EVFXDeliveryPattern::MeleeSlash;
	Ctx.Caster = Attacker;
	Ctx.Target = Target;
	Ctx.StartLocation = Attacker ? GetUnitAnchorLocation(Attacker, 0.2f) : Target->GetActorLocation();
	Ctx.TargetLocation = GetUnitAnchorLocation(Target, 0.22f);
	Ctx.bCrit = Hit.bCrit;
	Ctx.bReaction = Hit.bReactionTriggered;
	Ctx.Duration = (Hit.bCrit || Hit.bReactionTriggered) ? 0.36f : 0.24f;
	Ctx.ScaleMultiplier = 1.0f;

	if (Attacker && Attacker->Team == EBattleTeam::Enemy)
	{
		Ctx.ThemeColor = FLinearColor(0.95f, 0.24f, 0.16f, 1.0f);
	}
	else
	{
		Ctx.ThemeColor = FLinearColor(GetDamageTypeColor(Hit.DamageType));
	}

	PlayCue(Ctx);
}

void UBattleVFXSubsystem::ScheduleCue(const FBattleVFXSpawnContext& Context, float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (DelaySeconds <= KINDA_SMALL_NUMBER)
	{
		PlayCue(Context);
		return;
	}

	FTimerHandle TimerHandle;
	TWeakObjectPtr<UBattleVFXSubsystem> Self(this);
	World->GetTimerManager().SetTimer(TimerHandle,
		[Self, Context]()
		{
			if (Self.IsValid())
			{
				FBattleVFXSpawnContext Refreshed = Context;
				if (Refreshed.Caster)
				{
					Refreshed.StartLocation = Self->GetUnitAnchorLocation(Refreshed.Caster, 0.25f);
				}
				if (Refreshed.Target && Context.Stage != EBattleVFXCueStage::ChainJump)
				{
					Refreshed.TargetLocation = Self->GetUnitAnchorLocation(Refreshed.Target, 0.20f);
				}
				Self->PlayCue(Refreshed);
			}
		},
		DelaySeconds, false);
}

void UBattleVFXSubsystem::PlayCue(const FBattleVFXSpawnContext& Context)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector SpawnLoc = (Context.Stage == EBattleVFXCueStage::Cast || Context.Stage == EBattleVFXCueStage::ChainJump)
		? Context.StartLocation
		: Context.TargetLocation;
	const FVector AimVec = (Context.TargetLocation - Context.StartLocation).GetSafeNormal2D();
	const FRotator SpawnRot = AimVec.IsNearlyZero() ? FRotator::ZeroRotator : AimVec.Rotation();

	// 1. 可选蓝图自定义演出优先
	if (Context.Skill && Context.Skill->VFX.CustomCueClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ABattleSkillVFXCue* CueActor = World->SpawnActor<ABattleSkillVFXCue>(
			Context.Skill->VFX.CustomCueClass, SpawnLoc, SpawnRot, Params))
		{
			CueActor->PlayCue(Context);
		}
		return;
	}

	// 2. Niagara 槽位表现
	if (Context.Skill)
	{
		if (const TSoftObjectPtr<UNiagaraSystem>* Slot = SelectNiagaraSlot(
			Context.Skill->VFX, Context.Stage, Context.ResolvedPattern))
		{
			if (UNiagaraSystem* Sys = Slot->LoadSynchronous())
			{
				if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World, Sys, SpawnLoc, SpawnRot, FVector(Context.ScaleMultiplier), true, true))
				{
					ABattleSkillVFXCue::ApplyContextToNiagara(
						Comp, Context,
						Context.Skill->VFX.ColorParamName,
						Context.Skill->VFX.StartParamName,
						Context.Skill->VFX.EndParamName,
						Context.Skill->VFX.RadiusParamName);
					return;
				}
			}
		}
	}

	// 3. 程序化发光兜底演出 (M_Route_Glow / M_Route_Halo + 主题色点光)
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABattleProceduralVFXActor* ProcActor = World->SpawnActor<ABattleProceduralVFXActor>(
		ABattleProceduralVFXActor::StaticClass(), SpawnLoc, SpawnRot, Params))
	{
		ProcActor->InitVFX(Context, ++SeedCounter);
	}
}
