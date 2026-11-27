// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionUnit.h"

#include "Core/ERAssetCatalog.h"
#include "Core/ERCollisionChannels.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Legion/Terrain/LegionTerrainGrid.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/ConstructorHelpers.h"

// ---------------- 命令与弱 AI 指令 ----------------

void ALegionUnit::MoveToInternal(const FVector& Destination, float OptionalFacingYaw, ELegionDirective KeepDirective)
{
	if (bWiped) { return; }

	FVector CleanDest = Destination;
	if (TerrainGrid && TerrainGrid->IsBuilt())
	{
		CleanDest = TerrainGrid->ClampToWalkable(Destination);
		TerrainGrid->FindPath(GetActorLocation(), CleanDest, Waypoints);
	}
	else
	{
		Waypoints.Reset();
		Waypoints.Add(CleanDest);
	}
	WaypointIndex = 0;

	MoveDestination = CleanDest;
	bHasMoveDestination = true;
	AttackTarget = nullptr;
	bHasCustomFacing = (OptionalFacingYaw < 999.0f);
	if (bHasCustomFacing)
	{
		DesiredFacingYaw = FMath::UnwindDegrees(OptionalFacingYaw);
	}
	Directive = KeepDirective;
	State = ELegionState::Moving;
}

void ALegionUnit::CommandMoveTo(const FVector& Destination, float OptionalFacingYaw)
{
	DirectiveSpeedMul = 1.0f;
	PatrolWaypoints.Reset();
	bHasReconGoal = false;
	MoveToInternal(Destination, OptionalFacingYaw, ELegionDirective::PlayerOrder);
}

void ALegionUnit::CommandAttack(ALegionUnit* Target)
{
	if (bWiped || !Target || Target == this || Target->IsWiped()) { return; }
	Directive = ELegionDirective::PlayerOrder;
	DirectiveSpeedMul = 1.0f;
	PatrolWaypoints.Reset();
	bHasReconGoal = false;
	AttackTarget = Target;
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	RepathTimer = 0.0f; // 立即触发一次路径计算
	Waypoints.Reset();
	State = ELegionState::Moving;

	// 敌方或空闲兵团受到近距离威胁时自卫反击 (不抢占玩家现有显式移动指令)
	if (!Target->GetAttackTarget() && Target->GetLegionState() == ELegionState::Holding)
	{
		if (FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation()) <=
			FMath::Square(Target->GetEffectiveEngageRange(this) + 400.0f))
		{
			Target->NotifyEnemyContact(this, GetActorLocation(), false);
		}
	}
}

void ALegionUnit::CommandHalt()
{
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	AttackTarget = nullptr;
	Waypoints.Reset();
	PatrolWaypoints.Reset();
	bHasReconGoal = false;
	Directive = ELegionDirective::Standby;
	DirectiveSpeedMul = 1.0f;
	if (!bWiped) { State = ELegionState::Holding; }
}

void ALegionUnit::CommandGuard(const FVector& AnchorPos, float OptionalFacingYaw)
{
	if (bWiped) { return; }
	FVector CleanAnchor = AnchorPos;
	if (TerrainGrid && TerrainGrid->IsBuilt())
	{
		CleanAnchor = TerrainGrid->ClampToWalkable(AnchorPos);
	}
	GuardAnchor = CleanAnchor;
	GuardBaseYaw = (OptionalFacingYaw < 999.0f) ? FMath::UnwindDegrees(OptionalFacingYaw) : FacingYaw;
	PatrolWaypoints.Reset();
	bHasReconGoal = false;
	AttackTarget = nullptr;
	AlertLevel = FMath::Max(AlertLevel, 0.55f);
	AlertDecayCooldown = 8.0f;
	Alertness = ELegionAlertness::Alert;
	DirectiveSpeedMul = 0.80f;

	if (FVector::Dist2D(GetActorLocation(), CleanAnchor) > 220.0f)
	{
		MoveToInternal(CleanAnchor, GuardBaseYaw, ELegionDirective::Guard);
	}
	else
	{
		Directive = ELegionDirective::Guard;
		bHasMoveDestination = false;
		bHasCustomFacing = true;
		DesiredFacingYaw = GuardBaseYaw;
		Waypoints.Reset();
		State = ELegionState::Holding;
	}
}

void ALegionUnit::CommandPatrol(const TArray<FVector>& InPoints)
{
	if (bWiped) { return; }
	PatrolWaypoints.Reset();
	for (const FVector& P : InPoints)
	{
		const FVector Clean = (TerrainGrid && TerrainGrid->IsBuilt()) ? TerrainGrid->ClampToWalkable(P) : P;
		if (PatrolWaypoints.Num() == 0 || FVector::Dist2D(PatrolWaypoints.Last(), Clean) > 250.0f)
		{
			PatrolWaypoints.Add(Clean);
		}
	}
	if (PatrolWaypoints.Num() == 0)
	{
		PatrolWaypoints.Add(GetActorLocation());
	}
	if (PatrolWaypoints.Num() == 1)
	{
		const FVector Fwd = GetFacingVector();
		const FVector Alt = PatrolWaypoints[0] + FVector(-Fwd.Y, Fwd.X, 0.0f) * 1400.0f;
		PatrolWaypoints.Add((TerrainGrid && TerrainGrid->IsBuilt()) ? TerrainGrid->ClampToWalkable(Alt) : Alt);
	}

	PatrolCursor = (PatrolWaypoints.Num() > 1 && FVector::Dist2D(GetActorLocation(), PatrolWaypoints[0]) < 350.0f) ? 1 : 0;
	PatrolDirection = 1;
	PatrolDwellTimer = 0.0f;
	bHasReconGoal = false;
	AttackTarget = nullptr;
	AlertLevel = FMath::Max(AlertLevel, 0.45f);
	AlertDecayCooldown = 10.0f;
	Alertness = ELegionAlertness::Alert;
	DirectiveSpeedMul = 0.66f;
	MoveToInternal(PatrolWaypoints[PatrolCursor], 1000.0f, ELegionDirective::Patrol);
}

void ALegionUnit::CommandRecon()
{
	if (bWiped) { return; }
	PatrolWaypoints.Reset();
	AttackTarget = nullptr;
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	Waypoints.Reset();
	bHasReconGoal = false;
	ReconDwellTimer = 0.0f;
	Directive = ELegionDirective::Recon;
	DirectiveSpeedMul = 0.78f;
	AlertLevel = FMath::Max(AlertLevel, 0.50f);
	AlertDecayCooldown = 10.0f;
	Alertness = ELegionAlertness::Alert;
}

void ALegionUnit::CommandAutonomousAssault(ALegionUnit* PreferredTarget)
{
	if (bWiped || !PreferredTarget || PreferredTarget->IsWiped() || PreferredTarget == this) { return; }
	Directive = ELegionDirective::Assault;
	DirectiveSpeedMul = 1.0f;
	AttackTarget = PreferredTarget;
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	RepathTimer = 0.0f;
	Waypoints.Reset();
	AlertLevel = 1.0f;
	AlertDecayCooldown = 8.0f;
	Alertness = ELegionAlertness::Combat;
	State = ELegionState::Moving;
}

void ALegionUnit::NotifyEnemyContact(ALegionUnit* SpottedEnemy, const FVector& ContactWorldPos, bool bFromDirectFire)
{
	if (bWiped) { return; }
	LastKnownThreatPos = ContactWorldPos;
	ThreatMemoryTimer = 12.0f;
	AlertLevel = FMath::Max(AlertLevel, bFromDirectFire ? 1.0f : 0.78f);
	AlertDecayCooldown = 8.0f;
	Alertness = (AlertLevel >= 0.72f) ? ELegionAlertness::Combat : ELegionAlertness::Alert;

	if (!SpottedEnemy || SpottedEnemy->IsWiped()) { return; }

	// 玩家直属命令、正在进攻其它目标、或未激活强制接敌时不抢占指挥官意志
	if (Directive == ELegionDirective::PlayerOrder)
	{
		// 玩家驻守部队在近距离受威胁时仅自卫反击或旋转面向威胁
		if (!AttackTarget.IsValid() && State == ELegionState::Holding)
		{
			const float D = FVector::Dist2D(GetActorLocation(), SpottedEnemy->GetActorLocation());
			if (D <= GetEffectiveEngageRange(SpottedEnemy) + 450.0f)
			{
				AttackTarget = SpottedEnemy;
				DesiredFacingYaw = (ContactWorldPos - GetActorLocation()).Rotation().Yaw;
			}
		}
		return;
	}

	if (Directive == ELegionDirective::Guard)
	{
		const float DistFromAnchor = FVector::Dist2D(GuardAnchor, SpottedEnemy->GetActorLocation());
		if (!AttackTarget.IsValid() && DistFromAnchor <= GetEffectiveEngageRange(SpottedEnemy) + 1600.0f)
		{
			AttackTarget = SpottedEnemy;
			DesiredFacingYaw = (ContactWorldPos - GetActorLocation()).Rotation().Yaw;
		}
		return;
	}

	if (Directive == ELegionDirective::Standby)
	{
		const float Dist = FVector::Dist2D(GetActorLocation(), SpottedEnemy->GetActorLocation());
		if (bFromDirectFire || Dist <= GetEffectiveEngageRange(SpottedEnemy) + 500.0f)
		{
			CommandAutonomousAssault(SpottedEnemy);
		}
		else
		{
			DesiredFacingYaw = (ContactWorldPos - GetActorLocation()).Rotation().Yaw;
		}
		return;
	}

	if (Directive == ELegionDirective::Patrol || Directive == ELegionDirective::Recon)
	{
		// 侦察/巡逻部队发现目标后转入主动接战, 射手可借机抢占射程边缘架枪
		if (!AttackTarget.IsValid())
		{
			CommandAutonomousAssault(SpottedEnemy);
		}
	}
}

void ALegionUnit::NotifyIncomingFire(const FVector& ThreatWorldPos)
{
	if (bWiped) { return; }
	LastKnownThreatPos = ThreatWorldPos;
	ThreatMemoryTimer = 10.0f;
	AlertLevel = 1.0f;
	AlertDecayCooldown = 9.0f;
	Alertness = ELegionAlertness::Combat;

	if (!AttackTarget.IsValid() && State == ELegionState::Holding)
	{
		DesiredFacingYaw = (ThreatWorldPos - GetActorLocation()).Rotation().Yaw;
	}

	// 受击瞬间给若干士兵注入少量压制, 使其自然做出闪避/寻找掩护步
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		if (S.bAlive && UnitRng.FRand() < 0.30f)
		{
			S.Suppression = FMath::Min(1.0f, S.Suppression + 0.35f);
		}
	}
}

void ALegionUnit::UpdateAlertness(float DeltaSeconds, ALegionGameMode* GM)
{
	if (bWiped) { return; }
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (ThreatMemoryTimer > 0.0f) { ThreatMemoryTimer = FMath::Max(0.0f, ThreatMemoryTimer - DeltaSeconds); }
	if (ContactBroadcastCooldown > 0.0f) { ContactBroadcastCooldown = FMath::Max(0.0f, ContactBroadcastCooldown - DeltaSeconds); }
	if (AlertDecayCooldown > 0.0f) { AlertDecayCooldown = FMath::Max(0.0f, AlertDecayCooldown - DeltaSeconds); }

	const ELegionAlertness PrevAlert = Alertness;
	float TargetAlert = 0.0f;

	if (Directive == ELegionDirective::Guard || Directive == ELegionDirective::Patrol || Directive == ELegionDirective::Recon)
	{
		TargetAlert = 0.50f;
	}
	else if (Directive == ELegionDirective::Assault)
	{
		TargetAlert = 0.85f;
	}

	if (State == ELegionState::Moving && (Directive == ELegionDirective::PlayerOrder || Directive == ELegionDirective::Assault))
	{
		TargetAlert = FMath::Max(TargetAlert, 0.58f);
	}

	ALegionUnit* NearestFoe = nullptr;
	float NearestDist = MAX_FLT;
	if (GM && GM->IsBattleStarted())
	{
		const TArray<TObjectPtr<ALegionUnit>>& Foes =
			(Team == ELegionTeam::Player) ? GM->GetEnemyLegions() : GM->GetPlayerLegions();
		for (ALegionUnit* F : Foes)
		{
			if (!F || F->IsWiped()) { continue; }
			const float D = FVector::Dist2D(GetActorLocation(), F->GetActorLocation());
			if (D < NearestDist)
			{
				NearestDist = D;
				NearestFoe = F;
			}
		}
	}

	const float CombatEngageDist = GetEffectiveEngageRange(NearestFoe) + 350.0f;
	const float AlertSenseDist = (Directive == ELegionDirective::Recon) ? 5200.0f : 4200.0f;

	if (State == ELegionState::Engaged || (AttackTarget.IsValid() && NearestDist <= CombatEngageDist) || CachedFireTarget.IsValid())
	{
		TargetAlert = 1.0f;
		AlertDecayCooldown = 5.5f;
	}
	else if (NearestFoe && NearestDist <= AlertSenseDist)
	{
		const float Alpha = FMath::Clamp(1.0f - (NearestDist - CombatEngageDist) / FMath::Max(400.0f, AlertSenseDist - CombatEngageDist), 0.0f, 1.0f);
		TargetAlert = FMath::Max(TargetAlert, 0.48f + 0.44f * Alpha);
		AlertDecayCooldown = FMath::Max(AlertDecayCooldown, 4.0f);
	}

	if (TargetAlert >= AlertLevel)
	{
		AlertLevel = FMath::FInterpTo(AlertLevel, TargetAlert, DeltaSeconds, 3.2f);
	}
	else if (AlertDecayCooldown <= 0.0f)
	{
		AlertLevel = FMath::FInterpTo(AlertLevel, TargetAlert, DeltaSeconds, 0.45f);
	}

	if (AlertLevel >= 0.72f || State == ELegionState::Engaged || CachedFireTarget.IsValid())
	{
		Alertness = ELegionAlertness::Combat;
	}
	else if (AlertLevel >= 0.36f)
	{
		Alertness = ELegionAlertness::Alert;
	}
	else
	{
		Alertness = ELegionAlertness::Relaxed;
	}

	// 警戒分级跃迁时给所有存活士兵分配错峰反应延迟 (0.06..0.68s), 形成自然的波浪式举枪/收枪
	if (Alertness != PrevAlert)
	{
		for (int32 i = 0; i < Soldiers.Num(); ++i)
		{
			FLegionSoldier& S = Soldiers[i];
			if (!S.bAlive) { continue; }
			const float BaseLag = (Alertness == ELegionAlertness::Combat) ? 0.05f : 0.12f;
			const float SpanLag = (Alertness == ELegionAlertness::Relaxed) ? 0.85f : 0.58f;
			S.ReactionDelay = BaseLag + SpanLag * UnitRng.FRand();
		}
	}

	// 发现敌军后主动向友军网络通报敌情 (节流 2.5s)
	if (GM && GM->IsBattleStarted() && NearestFoe && NearestDist <= AlertSenseDist && ContactBroadcastCooldown <= 0.0f)
	{
		ContactBroadcastCooldown = 2.5f;
		GM->BroadcastTeamContact(Team, NearestFoe, NearestFoe->GetActorLocation(), State == ELegionState::Engaged);
	}

	(void)Now;
}

FVector ALegionUnit::PickNextReconPoint(const ALegionGameMode* GM)
{
	const FVector MyLoc = GetActorLocation();
	if (!GM) { return MyLoc; }

	const FVector Center = GM->GetDeployCenter();
	const float HalfExt = FMath::Max(2500.0f, GM->GetBattlefieldHalfExtent() - 900.0f);
	const FLegionTerrainGrid* Grid = (TerrainGrid && TerrainGrid->IsBuilt()) ? TerrainGrid : nullptr;

	// 优先偏向敌方半场或侧翼高地/林缘做渗透侦察
	const TArray<TObjectPtr<ALegionUnit>>& Foes =
		(Team == ELegionTeam::Player) ? GM->GetEnemyLegions() : GM->GetPlayerLegions();
	FVector FoeCentroid = Center;
	int32 FoeCount = 0;
	for (ALegionUnit* F : Foes)
	{
		if (F && !F->IsWiped())
		{
			FoeCentroid += F->GetActorLocation();
			++FoeCount;
		}
	}
	if (FoeCount > 0)
	{
		FoeCentroid = (FoeCentroid - Center) / (float)FoeCount;
	}

	FVector BestPt = MyLoc;
	float BestScore = -MAX_FLT;
	for (int32 Attempt = 0; Attempt < 14; ++Attempt)
	{
		const float Ang = UnitRng.FRandRange(0.0f, 2.0f * PI);
		const float Dist = UnitRng.FRandRange(2200.0f, 5200.0f);
		FVector Cand = MyLoc + FVector(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 0.0f);
		// 向敌军阵线半场方向轻度牵引
		Cand = FMath::Lerp(Cand, FoeCentroid + FVector(UnitRng.FRandRange(-3200.0f, 3200.0f), UnitRng.FRandRange(-3200.0f, 3200.0f), 0.0f), 0.38f);
		Cand.X = FMath::Clamp(Cand.X, Center.X - HalfExt, Center.X + HalfExt);
		Cand.Y = FMath::Clamp(Cand.Y, Center.Y - HalfExt, Center.Y + HalfExt);

		float Score = 0.0f;
		if (Grid)
		{
			Cand = Grid->ClampToWalkable(Cand);
			float Z = 0.0f, Slope = 0.0f;
			if (!Grid->SampleHeightAndSlope(Cand, Z, Slope)) { continue; }
			Score += (Z - MyLoc.Z) * 0.015f; // 略偏好高地视野
			Score += (float)Grid->GetTreeCountWorld(Cand) * 14.0f; // 偏爱林缘隐蔽点
		}
		const float TravelDist = FVector::Dist2D(MyLoc, Cand);
		if (TravelDist < 1400.0f) { Score -= 40.0f; }
		Score += UnitRng.FRandRange(0.0f, 18.0f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestPt = Cand;
		}
	}
	return BestPt;
}

void ALegionUnit::UpdateWeakAIDirective(float DeltaSeconds, ALegionGameMode* GM)
{
	if (bWiped || !GM || !GM->IsBattleStarted()) { return; }

	UpdateAlertness(DeltaSeconds, GM);

	if (DirectiveHoldTimer > 0.0f)
	{
		DirectiveHoldTimer = FMath::Max(0.0f, DirectiveHoldTimer - DeltaSeconds);
		return;
	}

	DirectiveDecisionTimer -= DeltaSeconds;
	if (DirectiveDecisionTimer > 0.0f) { return; }
	DirectiveDecisionTimer = 0.35f;

	const FVector MyLoc = GetActorLocation();
	const TArray<TObjectPtr<ALegionUnit>>& Foes =
		(Team == ELegionTeam::Player) ? GM->GetEnemyLegions() : GM->GetPlayerLegions();

	ALegionUnit* NearestFoe = nullptr;
	float NearestDist = MAX_FLT;
	for (ALegionUnit* F : Foes)
	{
		if (!F || F->IsWiped()) { continue; }
		const float D = FVector::Dist2D(MyLoc, F->GetActorLocation());
		if (D < NearestDist)
		{
			NearestDist = D;
			NearestFoe = F;
		}
	}

	switch (Directive)
	{
	case ELegionDirective::Standby:
	{
		// 待命休整: 敌军贴近有效射程 + 缓冲或正在开火时, 转入就近迎击; 我方不主动远距离追击
		if (NearestFoe)
		{
			const float AutoRange = (Team == ELegionTeam::Player)
				? FMath::Max(GM->PlayerAutoDefenseRange, GetEffectiveEngageRange(NearestFoe) * 0.85f)
				: GM->EnemyAggroRange;
			if (!AttackTarget.IsValid() && NearestDist <= AutoRange)
			{
				CommandAutonomousAssault(NearestFoe);
			}
		}
		break;
	}
	case ELegionDirective::Guard:
	{
		constexpr float GuardLeashRadius = 2400.0f;
		// 脱离锚点过远且无目标/目标远遁时退回固守点
		if (AttackTarget.IsValid())
		{
			if (AttackTarget->IsWiped() || FVector::Dist2D(GuardAnchor, AttackTarget->GetActorLocation()) > GetEffectiveEngageRange(AttackTarget.Get()) + GuardLeashRadius)
			{
				AttackTarget = nullptr;
				MoveToInternal(GuardAnchor, GuardBaseYaw, ELegionDirective::Guard);
				break;
			}
		}
		else if (NearestFoe && FVector::Dist2D(GuardAnchor, NearestFoe->GetActorLocation()) <= GetEffectiveEngageRange(NearestFoe) + 950.0f)
		{
			AttackTarget = NearestFoe;
			DesiredFacingYaw = (NearestFoe->GetActorLocation() - MyLoc).Rotation().Yaw;
		}
		else if (!bHasMoveDestination)
		{
			if (FVector::Dist2D(MyLoc, GuardAnchor) > 350.0f)
			{
				MoveToInternal(GuardAnchor, GuardBaseYaw, ELegionDirective::Guard);
			}
			else
			{
				// 戒备扇面缓缓扫视 (约 ±28° 周期摆动), 让全团呈警惕张望态
				GuardScanPhase += 0.35f * 0.9f;
				bHasCustomFacing = true;
				DesiredFacingYaw = FRotator::NormalizeAxis(GuardBaseYaw + FMath::Sin(GuardScanPhase) * 28.0f);
			}
		}
		break;
	}
	case ELegionDirective::Patrol:
	{
		const float DetectRange = (Team == ELegionTeam::Enemy)
			? FMath::Max(GM->EnemyAggroRange, 3600.0f)
			: FMath::Max(GetEffectiveEngageRange(NearestFoe) + 500.0f, 2800.0f);
		if (NearestFoe && NearestDist <= DetectRange)
		{
			NotifyEnemyContact(NearestFoe, NearestFoe->GetActorLocation(), false);
			break;
		}
		if (!bHasMoveDestination && PatrolWaypoints.Num() >= 2)
		{
			if (PatrolDwellTimer <= 0.0f)
			{
				// 刚抵达巡逻点: 先停驻观察四周 3.0~5.2s
				PatrolDwellTimer = UnitRng.FRandRange(3.0f, 5.2f);
				GuardScanPhase = UnitRng.FRandRange(0.0f, 6.28318f);
				GuardBaseYaw = FacingYaw;
			}
			else
			{
				PatrolDwellTimer -= 0.35f;
				bHasCustomFacing = true;
				GuardScanPhase += 0.55f;
				DesiredFacingYaw = FRotator::NormalizeAxis(GuardBaseYaw + FMath::Sin(GuardScanPhase) * 38.0f);
				if (PatrolDwellTimer <= 0.0f)
				{
					PatrolCursor += PatrolDirection;
					if (PatrolCursor >= PatrolWaypoints.Num())
					{
						PatrolCursor = FMath::Max(0, PatrolWaypoints.Num() - 2);
						PatrolDirection = -1;
					}
					else if (PatrolCursor < 0)
					{
						PatrolCursor = FMath::Min(1, PatrolWaypoints.Num() - 1);
						PatrolDirection = 1;
					}
					MoveToInternal(PatrolWaypoints[PatrolCursor], 1000.0f, ELegionDirective::Patrol);
				}
			}
		}
		break;
	}
	case ELegionDirective::Recon:
	{
		const float SpotRange = 4800.0f;
		if (NearestFoe && NearestDist <= SpotRange)
		{
			GM->BroadcastTeamContact(Team, NearestFoe, NearestFoe->GetActorLocation(), false);
			if (Archetype == ELegionClass::Marksman)
			{
				// 精确射手侦察到敌人后就地占点架枪戒备
				CommandGuard(MyLoc, (NearestFoe->GetActorLocation() - MyLoc).Rotation().Yaw);
			}
			else
			{
				CommandAutonomousAssault(NearestFoe);
			}
			break;
		}
		if (!bHasMoveDestination)
		{
			if (bHasReconGoal && ReconDwellTimer > 0.0f)
			{
				ReconDwellTimer -= 0.35f;
				bHasCustomFacing = true;
				GuardScanPhase += 0.65f;
				DesiredFacingYaw = FRotator::NormalizeAxis(GuardBaseYaw + FMath::Sin(GuardScanPhase) * 46.0f);
			}
			else
			{
				ReconGoal = PickNextReconPoint(GM);
				bHasReconGoal = true;
				ReconDwellTimer = UnitRng.FRandRange(2.2f, 3.8f);
				GuardBaseYaw = (ReconGoal - MyLoc).Rotation().Yaw;
				MoveToInternal(ReconGoal, 1000.0f, ELegionDirective::Recon);
			}
		}
		break;
	}
	case ELegionDirective::Assault:
	{
		if (!AttackTarget.IsValid() || AttackTarget->IsWiped())
		{
			if (NearestFoe)
			{
				AttackTarget = NearestFoe;
			}
			else
			{
				Directive = ELegionDirective::Standby;
				DirectiveSpeedMul = 1.0f;
				State = ELegionState::Holding;
			}
		}
		else if (Archetype == ELegionClass::Vanguard && FlankCommitTimer <= 0.0f)
		{
			// 突击刃兵绕侧包抄: 当目标正被友军正面牵制且我方尚在射程外时, 计算侧翼绕行航点
			const float Dist = FVector::Dist2D(MyLoc, AttackTarget->GetActorLocation());
			if (Dist > GetEffectiveEngageRange(AttackTarget.Get()) + 700.0f && Dist < 6200.0f)
			{
				const FVector ToTarget = (AttackTarget->GetActorLocation() - MyLoc).GetSafeNormal2D();
				const FVector Side(-ToTarget.Y, ToTarget.X, 0.0f);
				const float SideSign = ((CampaignLegionId + Soldiers.Num()) % 2 == 0) ? 1.0f : -1.0f;
				FlankWaypoint = AttackTarget->GetActorLocation() - ToTarget * 1500.0f + Side * (SideSign * 1900.0f);
				if (TerrainGrid && TerrainGrid->IsBuilt())
				{
					FlankWaypoint = TerrainGrid->ClampToWalkable(FlankWaypoint);
				}
				FlankCommitTimer = 5.5f;
			}
		}
		if (FlankCommitTimer > 0.0f)
		{
			FlankCommitTimer = FMath::Max(0.0f, FlankCommitTimer - 0.35f);
		}
		break;
	}
	case ELegionDirective::PlayerOrder:
	default:
		break;
	}
}
