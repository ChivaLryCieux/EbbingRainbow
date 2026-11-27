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
#include "Legion/Units/LegionUnitInternal.h"
#include "Legion/LegionPerf.h"

void ALegionUnit::ApplyPeerSeparation(float DeltaSeconds)
{
	ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
	if (!GM) { return; }

	FVector Push = FVector::ZeroVector;
	const float MinSep = 680.0f;
	const float MinSepSq = MinSep * MinSep;
	const FVector MyLoc = GetActorLocation();

	auto Accumulate = [&](const TArray<TObjectPtr<ALegionUnit>>& List)
	{
		for (ALegionUnit* Other : List)
		{
			if (!Other || Other == this || Other->IsWiped()) { continue; }
			const FVector Diff = MyLoc - Other->GetActorLocation();
			const float D2 = Diff.SizeSquared2D();
			if (D2 > 1.0f && D2 < MinSepSq)
			{
				const float D = FMath::Sqrt(D2);
				const float Strength = (1.0f - D / MinSep);
				// 同队保持更干净的队列间隔, 异队交战允许稍微贴近
				const float Mul = (Other->GetTeam() == Team) ? 380.0f : 140.0f;
				Push += Diff.GetSafeNormal2D() * (Strength * Mul);
			}
		}
	};
	Accumulate(GM->GetPlayerLegions());
	Accumulate(GM->GetEnemyLegions());

	if (!Push.IsNearlyZero())
	{
		FVector Next = MyLoc + Push * DeltaSeconds;
		const float HalfE = GM->GetBattlefieldHalfExtent();
		const FVector Center = GM->GetDeployCenter();
		Next.X = FMath::Clamp(Next.X, Center.X - HalfE, Center.X + HalfE);
		Next.Y = FMath::Clamp(Next.Y, Center.Y - HalfE, Center.Y + HalfE);
		if (!TerrainGrid || !TerrainGrid->IsBuilt() || TerrainGrid->IsWalkableWorld(Next))
		{
			SetActorLocation(Next);
		}
	}
}

bool ALegionUnit::IsFriendlyBlockingFireLine(int32 SoldierIndex, const FVector2D& AimDir2D, float CheckDist, float& OutClearSideSign) const
{
	OutClearSideSign = (SoldierIndex % 2 == 0) ? 1.0f : -1.0f;
	if (!Soldiers.IsValidIndex(SoldierIndex) || AimDir2D.IsNearlyZero()) { return false; }
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_FriendlyFireLine);

	const FVector2D SelfXY = Soldiers[SoldierIndex].WorldPosXY;
	const FVector2D RightDir(-AimDir2D.Y, AimDir2D.X);
	bool bBlocked = false;
	int32 LeftDensity = 0;
	int32 RightDensity = 0;

	for (int32 j = 0; j < Soldiers.Num(); ++j)
	{
		if (j == SoldierIndex || !Soldiers[j].bAlive) { continue; }
		const FVector2D Delta = Soldiers[j].WorldPosXY - SelfXY;
		const float Fwd = FVector2D::DotProduct(Delta, AimDir2D);
		const float Lat = FVector2D::DotProduct(Delta, RightDir);
		if (Fwd > 30.0f && Fwd < CheckDist)
		{
			if (FMath::Abs(Lat) < 52.0f)
			{
				bBlocked = true;
			}
			if (FMath::Abs(Lat) < 260.0f)
			{
				if (Lat >= 0.0f) { ++RightDensity; }
				else { ++LeftDensity; }
			}
		}
	}

	if (LeftDensity != RightDensity)
	{
		OutClearSideSign = (RightDensity < LeftDensity) ? 1.0f : -1.0f;
	}
	return bBlocked;
}

bool ALegionUnit::PickConflictFreeStepOffset(int32 SoldierIndex, const FVector2D& HomeWorldXY,
	float MinStepDist, float MaxRadius, const FVector2D& PreferredDir, float DirSpreadRad, FVector2D& OutOffset)
{
	if (!Soldiers.IsValidIndex(SoldierIndex)) { return false; }
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_StepOffset);
	const FVector2D SelfXY = Soldiers[SoldierIndex].WorldPosXY;
	const bool bHasPref = !PreferredDir.IsNearlyZero();
	const float PrefAng = bHasPref ? FMath::Atan2(PreferredDir.Y, PreferredDir.X) : 0.0f;
	const bool bHasGrid = (TerrainGrid && TerrainGrid->IsBuilt());
	const float RefZ = GetActorLocation().Z;

	constexpr float MinClearanceSq = 124.0f * 124.0f;
	float BestScore = -MAX_FLT;
	bool bFound = false;

	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		const float Ang = bHasPref
			? (PrefAng + UnitRng.FRandRange(-DirSpreadRad, DirSpreadRad))
			: UnitRng.FRandRange(0.0f, 2.0f * PI);
		const float Rad = UnitRng.FRandRange(MinStepDist * 0.65f, MaxRadius);
		const FVector2D CandOffset(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad);
		const FVector2D CandWorldXY = HomeWorldXY + CandOffset;

		const float StepDist = FVector2D::Distance(SelfXY, CandWorldXY);
		if (StepDist < MinStepDist) { continue; }
		if (bHasGrid && !TerrainGrid->IsWalkableWorld(FVector(CandWorldXY.X, CandWorldXY.Y, RefZ))) { continue; }

		bool bConflict = false;
		float NearestPeerSq = MAX_FLT;
		for (int32 j = 0; j < Soldiers.Num(); ++j)
		{
			if (j == SoldierIndex || !Soldiers[j].bAlive) { continue; }
			const float D2Pos = FVector2D::DistSquared(CandWorldXY, Soldiers[j].WorldPosXY);
			const float D2Goal = FVector2D::DistSquared(CandWorldXY, HomeWorldXY + Soldiers[j].MoveGoalOffsetXY);
			const float D2 = FMath::Min(D2Pos, D2Goal);
			if (D2 < MinClearanceSq)
			{
				bConflict = true;
				break;
			}
			NearestPeerSq = FMath::Min(NearestPeerSq, D2);
		}
		if (bConflict) { continue; }

		const float Score = FMath::Sqrt(FMath::Min(NearestPeerSq, 600.0f * 600.0f)) + StepDist * 0.25f;
		if (Score > BestScore)
		{
			BestScore = Score;
			OutOffset = CandOffset;
			bFound = true;
		}
	}
	return bFound;
}

int32 ALegionUnit::PickSoldierCombatTarget(FLegionSoldier& S, int32 SoldierIndex, ALegionUnit* Foe, bool bForceReselect, FVector& OutTargetFeet)
{
	if (!Foe || Foe->IsWiped() || Foe->GetAliveCount() <= 0)
	{
		S.AssignedTargetSoldier = INDEX_NONE;
		return INDEX_NONE;
	}

	if (!bForceReselect && S.TargetReselectTimer > 0.0f && S.AssignedTargetSoldier != INDEX_NONE)
	{
		if (Foe->GetAliveSoldierFeet(S.AssignedTargetSoldier, OutTargetFeet))
		{
			return S.AssignedTargetSoldier;
		}
	}

	// 在 4 名候选存活敌兵中挑距离最近 + 略偏同侧的个体, 避免全团齐刷刷打同一个点
	const FVector2D SelfXY = S.WorldPosXY;
	int32 BestIdx = INDEX_NONE;
	float BestScore = MAX_FLT;
	FVector BestFeet = FVector::ZeroVector;
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		const int32 Cand = Foe->PickRandomAliveSoldier();
		if (Cand == INDEX_NONE) { break; }
		FVector Feet;
		if (!Foe->GetAliveSoldierFeet(Cand, Feet)) { continue; }
		const float Dist = FVector2D::Distance(SelfXY, FVector2D(Feet.X, Feet.Y));
		const float Score = Dist + UnitRng.FRandRange(0.0f, 380.0f);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestIdx = Cand;
			BestFeet = Feet;
		}
	}

	if (BestIdx != INDEX_NONE) { OutTargetFeet = BestFeet; }
	S.AssignedTargetSoldier = BestIdx;
	S.TargetReselectTimer = UnitRng.FRandRange(3.2f, 5.5f);
	(void)SoldierIndex;
	return BestIdx;
}

void ALegionUnit::UpdateSoldiersMicroAI(float DeltaSeconds, float Now, ALegionGameMode* GM)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_MicroAI);
	LegionPerf::FScope PerfScope(LegionPerf::Get().MicroAIMs);
	const FVector LegionLoc = GetActorLocation();
	const FVector FwdVec = GetFacingVector();
	const FVector2D Fwd2D(FwdVec.X, FwdVec.Y);
	const bool bBattleStarted = (GM && GM->IsBattleStarted());
	const bool bLegionMoving = (State == ELegionState::Moving);

	// 目标敌团 (优先显式 AttackTarget, 其次自动射程内目标)
	ALegionUnit* PrimaryFoe = AttackTarget.Get();
	if (PrimaryFoe && PrimaryFoe->IsWiped()) { PrimaryFoe = nullptr; }
	ALegionUnit* AutoFoe = CachedFireTarget.Get();
	if (AutoFoe && AutoFoe->IsWiped()) { AutoFoe = nullptr; }
	ALegionUnit* AimFoe = PrimaryFoe ? PrimaryFoe : AutoFoe;

	// 团级自动开火扫描 (战前不扫描、不开火)
	if (bBattleStarted && !bWiped && AliveCount > 0)
	{
		FireScanTimer -= DeltaSeconds;
		if (FireScanTimer <= 0.0f)
		{
			FireScanTimer = 0.35f;
			CachedFireTarget = nullptr;
			const TArray<TObjectPtr<ALegionUnit>>& Foes =
				(Team == ELegionTeam::Player) ? GM->GetEnemyLegions() : GM->GetPlayerLegions();
			float BestD2 = FMath::Square(ArchetypeStats.EngageRange + 200.0f);
			for (ALegionUnit* F : Foes)
			{
				if (!F || F->IsWiped()) { continue; }
				const float D2 = FVector::DistSquared2D(LegionLoc, F->GetActorLocation());
				if (D2 < BestD2) { BestD2 = D2; CachedFireTarget = F; }
			}
			AutoFoe = CachedFireTarget.Get();
			if (!AimFoe) { AimFoe = AutoFoe; }
		}
	}

	const float Cadence = FMath::Max(0.5f, ArchetypeStats.VolleyInterval);
	constexpr float BulletSpeed = 5200.0f;
	const float MaxTetherRadius = bLegionMoving ? 520.0f : 360.0f;
	const bool bHasGrid = (TerrainGrid && TerrainGrid->IsBuilt());
	// 限制运动学与贴地积分单步时长, 防止截图/GC 卡顿帧引发大跨度瞬移与 Z 跃变
	const float MotionDt = FMath::Min(DeltaSeconds, 0.035f);

	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		if (!S.bAlive) { continue; }

		// 1. 衰减压制、枪械微后坐力与反应延迟
		S.Suppression = FMath::Max(0.0f, S.Suppression - DeltaSeconds * 0.28f);
		S.FireRecoilAlpha = FMath::Max(0.0f, S.FireRecoilAlpha - DeltaSeconds * 6.0f);
		if (S.TargetReselectTimer > 0.0f) { S.TargetReselectTimer -= DeltaSeconds; }

		if (S.PerceivedAlertness != Alertness)
		{
			S.ReactionDelay -= DeltaSeconds;
			if (S.ReactionDelay <= 0.0f)
			{
				S.PerceivedAlertness = Alertness;
				S.StateTimer = FMath::Min(S.StateTimer, 0.35f);
			}
		}

		// 2. 个体张望偏角刷新 (拉长周期, 彻底消除高频摇头抖动)
		S.GlanceTimer -= DeltaSeconds;
		if (S.GlanceTimer <= 0.0f)
		{
			if (S.PerceivedAlertness == ELegionAlertness::Relaxed)
			{
				S.GlanceTimer = UnitRng.FRandRange(3.0f, 6.5f);
				S.GlanceYawOffset = UnitRng.FRandRange(-36.0f, 36.0f);
			}
			else if (S.PerceivedAlertness == ELegionAlertness::Alert)
			{
				S.GlanceTimer = UnitRng.FRandRange(2.4f, 4.8f);
				S.GlanceYawOffset = UnitRng.FRandRange(-24.0f, 24.0f);
			}
			else
			{
				S.GlanceTimer = 3.0f;
				S.GlanceYawOffset = 0.0f;
			}
		}

		// 3. 微状态机转换 (槽位不再随军团扇面扫视整体旋转, 避免旋转木马拖拽)
		S.StateTimer -= DeltaSeconds;
		const FVector2D HomeWorldXY(LegionLoc.X + S.HomeOffset.X, LegionLoc.Y + S.HomeOffset.Y);

		int32 TargetSoldierIdx = INDEX_NONE;
		FVector TargetFeet = FVector::ZeroVector;
		bool bHasSoldierTarget = false;
		if (bBattleStarted && AimFoe)
		{
			TargetSoldierIdx = PickSoldierCombatTarget(S, i, AimFoe, false, TargetFeet);
			bHasSoldierTarget = (TargetSoldierIdx != INDEX_NONE);
		}

		FVector2D AimDir2D = Fwd2D;
		float DistToTarget = MAX_FLT;
		if (bHasSoldierTarget)
		{
			const FVector2D DeltaT = FVector2D(TargetFeet.X, TargetFeet.Y) - S.WorldPosXY;
			DistToTarget = DeltaT.Size();
			if (DistToTarget > 1.0f) { AimDir2D = DeltaT / DistToTarget; }
		}

		auto EnterPlantedState = [&](ELegionSoldierState NewState, float MinSec, float MaxSec)
		{
			S.SoldierState = NewState;
			S.StateTimer = UnitRng.FRandRange(MinSec, MaxSec);
		};

		auto TryBeginStep = [&](ELegionSoldierState StepState, float MinStepDist, float MaxRadius,
			const FVector2D& PrefDir, float SpreadRad, float MinSec, float MaxSec) -> bool
		{
			FVector2D ChosenOffset = S.MoveGoalOffsetXY;
			if (!PickConflictFreeStepOffset(i, HomeWorldXY, MinStepDist, MaxRadius, PrefDir, SpreadRad, ChosenOffset))
			{
				return false;
			}
			S.SoldierState = StepState;
			S.MoveGoalOffsetXY = ChosenOffset;
			S.TacticalOffsetXY = ChosenOffset;
			const FVector2D StepVec = (HomeWorldXY + ChosenOffset) - S.WorldPosXY;
			if (StepVec.SizeSquared() > 25.0f)
			{
				S.StepFacingYaw = FMath::RadiansToDegrees(FMath::Atan2(StepVec.Y, StepVec.X));
			}
			S.StateTimer = UnitRng.FRandRange(MinSec, MaxSec);
			return true;
		};

		if (bLegionMoving && (!bHasSoldierTarget || DistToTarget > ArchetypeStats.EngageRange * 0.98f))
		{
			S.SoldierState = ELegionSoldierState::FormationMarch;
			S.MoveGoalOffsetXY = FVector2D::ZeroVector;
			S.TacticalOffsetXY = FVector2D::ZeroVector;
		}
		else if (bBattleStarted && S.PerceivedAlertness == ELegionAlertness::Combat && bHasSoldierTarget)
		{
			float ClearSideSign = 1.0f;
			const bool bBlocked = IsFriendlyBlockingFireLine(i, AimDir2D, 340.0f, ClearSideSign);
			const bool bTooFar = (DistToTarget > ArchetypeStats.EngageRange * 0.94f);
			const FVector2D SideDir(-AimDir2D.Y, AimDir2D.X);

			if (S.SoldierState != ELegionSoldierState::SeekingFireLine
				&& S.SoldierState != ELegionSoldierState::CombatAimFire
				&& S.SoldierState != ELegionSoldierState::CombatSkirmish)
			{
				if (bBlocked || bTooFar)
				{
					const FVector2D Pref = bTooFar ? AimDir2D : (SideDir * ClearSideSign + AimDir2D * 0.45f).GetSafeNormal();
					if (!TryBeginStep(ELegionSoldierState::SeekingFireLine, 80.0f, 240.0f, Pref, 0.55f, 1.4f, 2.4f))
					{
						EnterPlantedState(ELegionSoldierState::CombatAimFire, 1.8f, 3.2f);
					}
				}
				else
				{
					EnterPlantedState(ELegionSoldierState::CombatAimFire, 1.8f, 3.4f);
				}
				S.BurstShotsLeft = UnitRng.RandRange(3, 6);
			}

			if (S.Suppression >= 0.65f && S.SoldierState != ELegionSoldierState::CombatSkirmish)
			{
				// 侧前/侧后斜向滑步避开 ±180 正后方接缝
				const FVector2D Pref = (SideDir * ClearSideSign - AimDir2D * 0.35f).GetSafeNormal();
				if (TryBeginStep(ELegionSoldierState::CombatSkirmish, 85.0f, 240.0f, Pref, 0.50f, 1.4f, 2.3f))
				{
					S.Suppression = 0.25f;
				}
				else
				{
					S.Suppression = 0.25f;
				}
			}
			else if (S.SoldierState == ELegionSoldierState::SeekingFireLine || S.SoldierState == ELegionSoldierState::CombatSkirmish)
			{
				const FVector2D GoalWorldXY = HomeWorldXY + S.MoveGoalOffsetXY;
				if (FVector2D::Distance(S.WorldPosXY, GoalWorldXY) <= 22.0f || S.StateTimer <= 0.0f)
				{
					EnterPlantedState(ELegionSoldierState::CombatAimFire, 1.8f, 3.6f);
					S.BurstShotsLeft = UnitRng.RandRange(3, 6);
				}
			}
			else if (S.StateTimer <= 0.0f) // CombatAimFire 结束周期
			{
				if (S.BurstShotsLeft <= 0)
				{
					const float RandSide = (UnitRng.FRand() < 0.5f) ? 1.0f : -1.0f;
					const FVector2D Pref = (SideDir * RandSide - AimDir2D * 0.25f).GetSafeNormal();
					if (TryBeginStep(ELegionSoldierState::CombatSkirmish, 85.0f, 240.0f, Pref, 0.55f, 1.4f, 2.4f))
					{
						S.BurstShotsLeft = UnitRng.RandRange(3, 6);
					}
					else
					{
						EnterPlantedState(ELegionSoldierState::CombatAimFire, 1.6f, 3.0f);
						S.BurstShotsLeft = UnitRng.RandRange(3, 6);
					}
				}
				else if (bBlocked || bTooFar)
				{
					const FVector2D Pref = bTooFar ? AimDir2D : (SideDir * ClearSideSign + AimDir2D * 0.45f).GetSafeNormal();
					if (!TryBeginStep(ELegionSoldierState::SeekingFireLine, 80.0f, 240.0f, Pref, 0.55f, 1.4f, 2.4f))
					{
						EnterPlantedState(ELegionSoldierState::CombatAimFire, 1.5f, 2.8f);
					}
				}
				else
				{
					EnterPlantedState(ELegionSoldierState::CombatAimFire, 1.6f, 3.0f);
				}
			}
		}
		else if (S.PerceivedAlertness == ELegionAlertness::Alert || (bBattleStarted && S.PerceivedAlertness == ELegionAlertness::Combat))
		{
			if (S.SoldierState != ELegionSoldierState::AlertScan && S.SoldierState != ELegionSoldierState::AlertPatrolStep)
			{
				if (UnitRng.FRand() < 0.35f)
				{
					if (!TryBeginStep(ELegionSoldierState::AlertPatrolStep, 75.0f, 145.0f, FVector2D::ZeroVector, PI, 1.4f, 2.4f))
					{
						EnterPlantedState(ELegionSoldierState::AlertScan, 2.2f, 4.5f);
					}
				}
				else
				{
					EnterPlantedState(ELegionSoldierState::AlertScan, 2.2f, 4.5f);
				}
			}
			else if (S.SoldierState == ELegionSoldierState::AlertPatrolStep)
			{
				const FVector2D GoalWorldXY = HomeWorldXY + S.MoveGoalOffsetXY;
				if (FVector2D::Distance(S.WorldPosXY, GoalWorldXY) <= 20.0f || S.StateTimer <= 0.0f)
				{
					EnterPlantedState(ELegionSoldierState::AlertScan, 2.4f, 4.8f);
				}
			}
			else if (S.StateTimer <= 0.0f) // AlertScan
			{
				if (UnitRng.FRand() < 0.36f * S.TraitRestlessness)
				{
					if (!TryBeginStep(ELegionSoldierState::AlertPatrolStep, 75.0f, 145.0f, FVector2D::ZeroVector, PI, 1.4f, 2.4f))
					{
						EnterPlantedState(ELegionSoldierState::AlertScan, 2.2f, 4.2f);
					}
				}
				else
				{
					EnterPlantedState(ELegionSoldierState::AlertScan, 2.2f, 4.5f);
				}
			}
		}
		else // Relaxed
		{
			if (S.SoldierState != ELegionSoldierState::RelaxedIdle && S.SoldierState != ELegionSoldierState::RelaxedWander)
			{
				if (UnitRng.FRand() < 0.38f)
				{
					const float MaxR = bBattleStarted ? 150.0f : 110.0f;
					if (!TryBeginStep(ELegionSoldierState::RelaxedWander, 72.0f, MaxR, FVector2D::ZeroVector, PI, 1.6f, 2.8f))
					{
						EnterPlantedState(ELegionSoldierState::RelaxedIdle, 2.5f, 5.5f);
					}
				}
				else
				{
					EnterPlantedState(ELegionSoldierState::RelaxedIdle, 2.5f, 5.5f);
				}
			}
			else if (S.SoldierState == ELegionSoldierState::RelaxedWander)
			{
				const FVector2D GoalWorldXY = HomeWorldXY + S.MoveGoalOffsetXY;
				if (FVector2D::Distance(S.WorldPosXY, GoalWorldXY) <= 18.0f || S.StateTimer <= 0.0f)
				{
					EnterPlantedState(ELegionSoldierState::RelaxedIdle, 2.6f, 5.6f);
				}
			}
			else if (S.StateTimer <= 0.0f) // RelaxedIdle
			{
				if (UnitRng.FRand() < 0.45f * S.TraitRestlessness)
				{
					const float MaxR = bBattleStarted ? 150.0f : 110.0f;
					if (!TryBeginStep(ELegionSoldierState::RelaxedWander, 72.0f, MaxR, FVector2D::ZeroVector, PI, 1.6f, 2.8f))
					{
						EnterPlantedState(ELegionSoldierState::RelaxedIdle, 2.5f, 5.2f);
					}
				}
				else
				{
					EnterPlantedState(ELegionSoldierState::RelaxedIdle, 2.5f, 5.5f);
				}
			}
		}

		// 4. 严格区分「驻立锁脚」vs「有目的迈步」二元运动学
		const bool bPlanted = (S.SoldierState == ELegionSoldierState::RelaxedIdle
			|| S.SoldierState == ELegionSoldierState::AlertScan
			|| S.SoldierState == ELegionSoldierState::CombatAimFire);

		FVector2D DesiredVel = FVector2D::ZeroVector;
		if (!bPlanted)
		{
			FVector2D TargetWorldXY = HomeWorldXY + S.MoveGoalOffsetXY;
			float MaxSoldierSpeed = 120.0f;
			switch (S.SoldierState)
			{
			case ELegionSoldierState::RelaxedWander:   MaxSoldierSpeed = 115.0f; break;
			case ELegionSoldierState::AlertPatrolStep: MaxSoldierSpeed = 135.0f; break;
			case ELegionSoldierState::SeekingFireLine: MaxSoldierSpeed = 215.0f; break;
			case ELegionSoldierState::CombatSkirmish:  MaxSoldierSpeed = 220.0f; break;
			case ELegionSoldierState::FormationMarch:
			{
				MaxSoldierSpeed = MoveSpeed * DirectiveSpeedMul * 1.12f;
				const FVector2D GentleNoise = LegionUnitInternal::SampleWanderNoise(Now, S.NoiseSeedX, S.NoiseSeedY, 0.12f);
				TargetWorldXY = HomeWorldXY + GentleNoise * 38.0f;
				break;
			}
			default: break;
			}

			const FVector2D ToTarget = TargetWorldXY - S.WorldPosXY;
			const float DistToDesired = ToTarget.Size();
			if (DistToDesired > 10.0f)
			{
				const float Ramp = FMath::Clamp((DistToDesired - 10.0f) / 65.0f, 0.25f, 1.0f);
				DesiredVel = (ToTarget / DistToDesired) * (MaxSoldierSpeed * Ramp * S.TraitPlayRate);
			}
			else if (bLegionMoving)
			{
				const float Lag = FVector2D::Distance(HomeWorldXY, S.WorldPosXY);
				if (Lag > 120.0f)
				{
					DesiredVel = (HomeWorldXY - S.WorldPosXY).GetSafeNormal() * MoveSpeed * 1.2f;
				}
			}

			// 仅对移动中士兵施加柔和切向/防重叠绕行 (绝不推挤驻立士兵双脚)
			if (!DesiredVel.IsNearlyZero(10.0f))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_LocalAvoidance);
				const FVector2D MoveDir = DesiredVel.GetSafeNormal();
				const FVector2D RightMove(-MoveDir.Y, MoveDir.X);
				constexpr float AvoidRadius = 110.0f;
				constexpr float AvoidSq = AvoidRadius * AvoidRadius;
				for (int32 j = 0; j < Soldiers.Num(); ++j)
				{
					if (j == i || !Soldiers[j].bAlive) { continue; }
					const FVector2D ToPeer = Soldiers[j].WorldPosXY - S.WorldPosXY;
					const float D2 = ToPeer.SizeSquared();
					if (D2 > 1.0f && D2 < AvoidSq)
					{
						const float FwdProj = FVector2D::DotProduct(ToPeer, MoveDir);
						if (FwdProj > -15.0f)
						{
							const float D = FMath::Sqrt(D2);
							const float LatProj = FVector2D::DotProduct(ToPeer, RightMove);
							const float AwaySign = (LatProj >= 0.0f) ? -1.0f : 1.0f;
							DesiredVel += RightMove * (AwaySign * (1.0f - D / AvoidRadius) * 140.0f);
						}
					}
				}
			}
		}

		const float VelInterp = bPlanted ? 11.0f : (bLegionMoving ? 7.5f : 6.0f);
		S.WorldVelXY = FMath::Vector2DInterpTo(S.WorldVelXY, DesiredVel, MotionDt, VelInterp);
		if (bPlanted && S.WorldVelXY.SizeSquared() < 36.0f)
		{
			S.WorldVelXY = FVector2D::ZeroVector;
		}

		if (!S.WorldVelXY.IsNearlyZero(0.5f))
		{
			FVector2D NextXY = S.WorldPosXY + S.WorldVelXY * MotionDt;
			const FVector2D FromHome = NextXY - HomeWorldXY;
			if (FromHome.SizeSquared() > MaxTetherRadius * MaxTetherRadius)
			{
				NextXY = HomeWorldXY + FromHome.GetSafeNormal() * MaxTetherRadius;
			}
			if (!bHasGrid || TerrainGrid->IsWalkableWorld(FVector(NextXY.X, NextXY.Y, LegionLoc.Z)))
			{
				S.WorldPosXY = NextXY;
			}
			else
			{
				S.WorldVelXY = FVector2D::ZeroVector;
				if (!bPlanted && S.SoldierState != ELegionSoldierState::FormationMarch)
				{
					EnterPlantedState(S.PerceivedAlertness == ELegionAlertness::Combat
						? ELegionSoldierState::CombatAimFire
						: (S.PerceivedAlertness == ELegionAlertness::Alert ? ELegionSoldierState::AlertScan : ELegionSoldierState::RelaxedIdle),
						2.2f, 4.5f);
				}
			}
		}

		// 5. 单源世界空间贴地 (双线性网格 + 慢速高精度射线残差校准, 驻立死区 + 爬坡速率限幅)
		S.SmoothedRayOffset = FMath::FInterpTo(S.SmoothedRayOffset, S.RayGroundOffset, MotionDt, 3.5f);
		float TargetWorldZ = LegionLoc.Z;
		if (bHasGrid && TerrainGrid->SampleHeightBilinear(FVector(S.WorldPosXY.X, S.WorldPosXY.Y, LegionLoc.Z), TargetWorldZ))
		{
			TargetWorldZ += S.SmoothedRayOffset;
		}
		else if (S.bWorldZInitialized)
		{
			TargetWorldZ = S.WorldZ;
		}

		if (!S.bWorldZInitialized)
		{
			S.WorldZ = TargetWorldZ;
			S.bWorldZInitialized = true;
		}
		else
		{
			const float Dz = TargetWorldZ - S.WorldZ;
			const float Speed2DNow = S.WorldVelXY.Size();
			if (bPlanted && Speed2DNow < 4.0f && FMath::Abs(Dz) < 2.5f)
			{
				// 驻立锁脚时屏蔽亚厘米级微小高度残差波动
			}
			else
			{
				const float RawNextZ = FMath::FInterpTo(S.WorldZ, TargetWorldZ, MotionDt, 12.0f);
				const float MaxStepZ = FMath::Min(3.8f, FMath::Max(0.8f, 150.0f * MotionDt));
				S.WorldZ = FMath::Clamp(RawNextZ, S.WorldZ - MaxStepZ, S.WorldZ + MaxStepZ);
			}
		}

		S.GroundZ = S.WorldZ - LegionLoc.Z;
		S.LocalOffset = FVector(S.WorldPosXY.X - LegionLoc.X, S.WorldPosXY.Y - LegionLoc.Y, S.GroundZ);

		// 6. 阻尼转向 (休整/警戒用柔和恒定朝向 + 速度限幅, 消除高速震荡摇头)
		float WantYaw = FacingYaw + S.JitterYaw * 0.5f;
		if (S.SoldierState == ELegionSoldierState::RelaxedWander)
		{
			WantYaw = S.StepFacingYaw;
		}
		else if (S.SoldierState == ELegionSoldierState::RelaxedIdle)
		{
			WantYaw = FacingYaw + S.JitterYaw + S.GlanceYawOffset;
		}
		else if (S.SoldierState == ELegionSoldierState::AlertScan || S.SoldierState == ELegionSoldierState::AlertPatrolStep)
		{
			WantYaw = FacingYaw + S.GlanceYawOffset + S.JitterYaw * 0.35f;
		}
		else if ((S.SoldierState == ELegionSoldierState::SeekingFireLine
			|| S.SoldierState == ELegionSoldierState::CombatAimFire
			|| S.SoldierState == ELegionSoldierState::CombatSkirmish) && bHasSoldierTarget)
		{
			WantYaw = FMath::RadiansToDegrees(FMath::Atan2(AimDir2D.Y, AimDir2D.X));
		}
		else if (S.SoldierState == ELegionSoldierState::FormationMarch && S.WorldVelXY.SizeSquared() > 3600.0f)
		{
			WantYaw = FMath::RadiansToDegrees(FMath::Atan2(S.WorldVelXY.Y, S.WorldVelXY.X));
		}

		const float DeltaYaw = FMath::FindDeltaAngleDegrees(S.CurYaw, WantYaw);
		if (FMath::Abs(DeltaYaw) >= 0.35f)
		{
			const float MaxRate = (S.PerceivedAlertness == ELegionAlertness::Combat || bLegionMoving) ? 185.0f : 88.0f;
			const float AngVel = FMath::Clamp(DeltaYaw * 4.8f, -MaxRate, MaxRate);
			S.CurYaw = FRotator::NormalizeAxis(S.CurYaw + AngVel * MotionDt);
		}

		// 7. 记录多帧运动稳定性遥测 (Z 跃变 / Z 方向翻转 / Yaw 方向翻转)
		const float TelZ = S.WorldZ;
		const float TelYaw = S.CurYaw;
		if (!S.bTelPrimed)
		{
			S.PrevTelZ = TelZ;
			S.PrevTelYaw = TelYaw;
			S.PrevTelAnimDir = S.SmoothedAnimDir;
			S.PrevDeltaZ = 0.0f;
			S.PrevDeltaYaw = 0.0f;
			S.bTelPrimed = true;
		}
		else
		{
			const float DZ = TelZ - S.PrevTelZ;
			const float DYaw = FMath::FindDeltaAngleDegrees(S.PrevTelYaw, TelYaw);
			S.MaxFrameJumpZ = FMath::Max(S.MaxFrameJumpZ, FMath::Abs(DZ));
			if (FMath::Abs(DZ) > 0.25f && FMath::Abs(S.PrevDeltaZ) > 0.25f && (DZ * S.PrevDeltaZ) < 0.0f)
			{
				++S.ZFlipCount;
			}
			if (FMath::Abs(DYaw) > 1.2f && FMath::Abs(S.PrevDeltaYaw) > 1.2f && (DYaw * S.PrevDeltaYaw) < 0.0f)
			{
				++S.YawFlipCount;
			}
			S.PrevDeltaZ = DZ;
			S.PrevDeltaYaw = DYaw;
			S.PrevTelZ = TelZ;
			S.PrevTelYaw = TelYaw;
			S.PrevTelAnimDir = S.SmoothedAnimDir;
		}

		// 8. 真实弹道独立开火 (需满足射程内、朝向基本对准、非贴脸挡枪线)
		if (bBattleStarted && AimFoe && bHasSoldierTarget && DistToTarget <= ArchetypeStats.EngageRange + 150.0f)
		{
			S.FireTimer -= DeltaSeconds;
			if (S.FireTimer <= 0.0f)
			{
				const float YawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(S.CurYaw,
					FMath::RadiansToDegrees(FMath::Atan2(AimDir2D.Y, AimDir2D.X))));
				float DummySide = 1.0f;
				const bool bPointBlankBlocked = IsFriendlyBlockingFireLine(i, AimDir2D, 135.0f, DummySide);

				if (YawDiff < 42.0f && !bPointBlankBlocked)
				{
					S.FireTimer = Cadence * UnitRng.FRandRange(0.72f, 1.32f);
					S.BurstShotsLeft = FMath::Max(0, S.BurstShotsLeft - 1);

					const FVector MuzzleFeet(S.WorldPosXY.X, S.WorldPosXY.Y, S.WorldZ);
					const FVector ToVictim = (TargetFeet - MuzzleFeet).GetSafeNormal2D();
					const FVector MuzzlePos = MuzzleFeet + FVector(0.0f, 0.0f, 138.0f) + ToVictim * 45.0f;
					const FVector AimPos = TargetFeet + FVector(0.0f, 0.0f, 118.0f);
					FVector Dir = (AimPos - MuzzlePos).GetSafeNormal();
					if (!Dir.IsNearlyZero())
					{
						float HalfAngleDeg = 1.25f;
						if (S.WorldVelXY.SizeSquared() > 1600.0f) { HalfAngleDeg += 1.05f; }
						if (AimFoe->GetLegionState() == ELegionState::Moving) { HalfAngleDeg += 1.35f; }
						Dir = UnitRng.VRandCone(Dir, FMath::DegreesToRadians(HalfAngleDeg));
						GM->SpawnBullet(MuzzlePos, Dir * BulletSpeed, this);
						S.FireRecoilAlpha = 1.0f;
					}
				}
				else
				{
					S.FireTimer = 0.22f; // 很快重测枪线与朝向
				}
			}
		}
	}
}

void ALegionUnit::ConformToTerrain(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	const FVector Loc = GetActorLocation();
	float GZ = Loc.Z;
	if (TraceGroundZ(Loc, GZ))
	{
		const float NewZ = (DeltaSeconds > 1.0f) ? GZ
			: FMath::FInterpTo(Loc.Z, GZ, DeltaSeconds, GroundConformSpeed * 1.5f);
		SetActorLocation(FVector(Loc.X, Loc.Y, NewZ));
	}

	// 单兵高度主源已统一交由 UpdateSoldiersMicroAI (双线性连续网格 + S.SmoothedRayOffset);
	// 这里仅按周期轮询最多 4 名存活士兵校正射线残差 S.RayGroundOffset, 不再直接覆写组件坐标打架。
	GroundConformTimer -= DeltaSeconds;
	if (GroundConformTimer > 0.0f) { return; }
	GroundConformTimer = GroundConformInterval;

	const int32 N = Soldiers.Num();
	if (N <= 0 || !TerrainGrid || !TerrainGrid->IsBuilt()) { return; }

	const FVector SelfLoc = GetActorLocation();
	const int32 Checks = FMath::Min(4, N);
	for (int32 c = 0; c < Checks; ++c)
	{
		const int32 Idx = (GroundTraceCursor + c) % N;
		FLegionSoldier& S = Soldiers[Idx];
		if (!S.bAlive) { continue; }
		const FVector WPos(S.WorldPosXY.X, S.WorldPosXY.Y, SelfLoc.Z);
		float SZ = 0.0f, GridZ = 0.0f;
		if (TraceGroundZ(WPos, SZ) && TerrainGrid->SampleHeightBilinear(WPos, GridZ))
		{
			S.RayGroundOffset = FMath::Clamp(SZ - GridZ, -120.0f, 120.0f);
		}
	}
	GroundTraceCursor = (GroundTraceCursor + Checks) % N;
}

bool ALegionUnit::TraceGroundZ(const FVector& XY, float& OutZ) const
{
	FHitResult Hit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams P(SCENE_QUERY_STAT(LegionGround), false, this);
	const FVector A(XY.X, XY.Y, GroundTraceHeight);
	const FVector B(XY.X, XY.Y, -GroundTraceHeight);
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, A, B, ObjParams, P))
	{
		if (Hit.GetActor() && Hit.GetActor()->GetClass() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Pickup")))
		{
			return false;
		}
		OutZ = Hit.Location.Z;
		return true;
	}
	return false;
}
