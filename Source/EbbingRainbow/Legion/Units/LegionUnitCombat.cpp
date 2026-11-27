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

float ALegionUnit::GetEffectiveEngageRange(const ALegionUnit* Target) const
{
	float R = EngageRange;
	if (ArchetypeStats.bRanged && Target)
	{
		const float DZ = GetActorLocation().Z - Target->GetActorLocation().Z;
		// 高地射程延展: 每高 1 米 +45cm, 最多 +750cm / -250cm
		R += FMath::Clamp((DZ / 100.0f) * 45.0f, -250.0f, 750.0f);
	}
	return FMath::Max(160.0f, R);
}

// ---------------- 结算 ----------------

int32 ALegionUnit::ApplyDamage(float DamageAmount, ALegionUnit* InInstigator, bool bRangedHit)
{
	if (bWiped || DamageAmount <= 0.0f) { return 0; }
	if (InInstigator) { LastAttacker = InInstigator; }

	float FinalDamage = DamageAmount;

	// 1. 正面方向格挡与侧背破防
	if (InInstigator)
	{
		const FVector ToAttacker = (InInstigator->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const FVector Fwd = GetFacingVector();
		const float Dot = FVector::DotProduct(Fwd, ToAttacker);

		const float Block = ArchetypeStats.FrontalBlockRatio;
		if (Dot > 0.35f && Block > 0.0f)
		{
			// 远程被盾牌克制效果更强
			const float EffectiveBlock = bRangedHit ? FMath::Min(0.92f, Block * 1.15f) : Block;
			FinalDamage *= (1.0f - EffectiveBlock);
		}
		else if (Dot < -0.25f)
		{
			// 侧翼/背刺惩罚
			FinalDamage *= 1.30f;
		}
	}

	const int32 PrevAlive = AliveCount;
	const FVector FromPos = InInstigator ? InInstigator->GetActorLocation()
		: GetActorLocation() + GetActorQuat().RotateVector(FVector::ForwardVector) * 100.0f;

	float Remaining = FinalDamage;
	while (Remaining > KINDA_SMALL_NUMBER && AliveCount > 0)
	{
		const int32 Victim = PickCasualtyIndex(FromPos);
		if (Victim == INDEX_NONE) { break; }

		FLegionSoldier& S = Soldiers[Victim];
		const float Applied = FMath::Min(S.HP, Remaining);
		S.HP = FMath::Max(0.0f, S.HP - Applied);
		LegionHP = FMath::Max(0.0f, LegionHP - Applied);
		Remaining -= Applied;

		if (S.HP <= KINDA_SMALL_NUMBER)
		{
			S.HP = 0.0f;
			S.DeathImpulseDir = FVector::ZeroVector;
			AliveCount = FMath::Max(0, AliveCount - 1);
			StartDying(Victim);
		}
	}

	CheckAndHandleWiped();
	if (InInstigator)
	{
		NotifyIncomingFire(InInstigator->GetActorLocation());
	}
	return PrevAlive - AliveCount;
}

int32 ALegionUnit::ApplySoldierProjectileHit(const UPrimitiveComponent* HitComp, int32 HitItem,
	const FVector& HitLocation, const FVector& HitDirection, float DamageAmount)
{
	if (bWiped || DamageAmount <= 0.0f) { return 0; }

	const int32 Victim = FindSoldierIndexByHit(HitComp, HitItem, HitLocation);
	if (Victim == INDEX_NONE || !Soldiers.IsValidIndex(Victim) || !Soldiers[Victim].bAlive)
	{
		return 0;
	}

	FLegionSoldier& S = Soldiers[Victim];
	const float Applied = FMath::Min(S.HP, DamageAmount);
	S.HP = FMath::Max(0.0f, S.HP - Applied);
	LegionHP = FMath::Max(0.0f, LegionHP - Applied);
	S.Suppression = FMath::Min(1.0f, S.Suppression + 0.65f);

	int32 Killed = 0;
	if (S.HP <= KINDA_SMALL_NUMBER)
	{
		S.HP = 0.0f;
		S.DeathImpulseDir = HitDirection.GetSafeNormal2D();
		AliveCount = FMath::Max(0, AliveCount - 1);
		StartDying(Victim);
		Killed = 1;
		CheckAndHandleWiped();
	}
	NotifyIncomingFire(HitLocation - HitDirection * 1200.0f);
	return Killed;
}

bool ALegionUnit::RunExactHitSelfTest()
{
	// 选最后一个存活士兵 (非前排), 模拟子弹命中其 HitProxy 胶囊
	int32 TargetIdx = INDEX_NONE;
	for (int32 i = Soldiers.Num() - 1; i >= 0; --i)
	{
		if (Soldiers[i].bAlive && Soldiers[i].HitProxy.IsValid())
		{
			TargetIdx = i;
			break;
		}
	}
	if (TargetIdx == INDEX_NONE) { return false; }

	const int32 AliveBefore = AliveCount;
	UCapsuleComponent* Proxy = Soldiers[TargetIdx].HitProxy.Get();
	const FVector HitLoc = Proxy->GetComponentLocation();

	// 第一枪未致命: 仅扣目标个体血量, 全团无人死亡
	const float PartialDmg = FMath::Max(1.0f, Soldiers[TargetIdx].HP * 0.4f);
	const int32 Killed1 = ApplySoldierProjectileHit(Proxy, INDEX_NONE, HitLoc, FVector(0.0f, 1.0f, 0.0f), PartialDmg);
	const bool bPartialOk = (Killed1 == 0 && Soldiers[TargetIdx].bAlive && AliveCount == AliveBefore);

	// 第二枪致命: 仅 TargetIdx 阵亡, 其余士兵全部存活, 且 TargetIdx 碰撞立刻关闭
	const float LethalDmg = Soldiers[TargetIdx].HP + 10.0f;
	const int32 Killed2 = ApplySoldierProjectileHit(Proxy, INDEX_NONE, HitLoc, FVector(0.0f, 1.0f, 0.0f), LethalDmg);
	bool bOnlyTargetDied = (Killed2 == 1 && !Soldiers[TargetIdx].bAlive && AliveCount == AliveBefore - 1);
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		if (i != TargetIdx && !Soldiers[i].bAlive) { bOnlyTargetDied = false; break; }
	}
	const bool bCollisionCleared = (Proxy->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

	UE_LOG(LogERLegion, Log, TEXT("[BulletHitSelfTest] %s: partial hit wounds only target without random death"),
		bPartialOk ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogERLegion, Log, TEXT("[BulletHitSelfTest] %s: lethal hit kills exact target idx=%d (not front/random)"),
		bOnlyTargetDied ? TEXT("PASS") : TEXT("FAIL"), TargetIdx);
	UE_LOG(LogERLegion, Log, TEXT("[BulletHitSelfTest] %s: killed soldier HitProxy disabled immediately"),
		bCollisionCleared ? TEXT("PASS") : TEXT("FAIL"));
	return bPartialOk && bOnlyTargetDied && bCollisionCleared;
}

void ALegionUnit::CheckAndHandleWiped()
{
	if (AliveCount <= 0 && !bWiped)
	{
		bWiped = true;
		LegionHP = 0.0f;
		State = ELegionState::Wiped;
		bHasMoveDestination = false;
		AttackTarget = nullptr;
		Waypoints.Reset();
		UpdateFrameVisual();
		OnLegionWiped.Broadcast(this);
		UE_LOG(LogERLegion, Log, TEXT("[Legion] %s wiped"), *Callsign);
	}
}

int32 ALegionUnit::FindSoldierIndexByHit(const UPrimitiveComponent* HitComp, int32 HitItem, const FVector& HitLocation) const
{
	// 1. 骨骼士兵模式: 精确匹配被子弹击中的个体胶囊代理 / 骨骼组件
	if (HitComp)
	{
		for (int32 i = 0; i < Soldiers.Num(); ++i)
		{
			if (!Soldiers[i].bAlive) { continue; }
			if (Soldiers[i].HitProxy.Get() == HitComp || Soldiers[i].SkelComp.Get() == HitComp)
			{
				return i;
			}
		}

		// 2. HISM 模式: 按命中的实例索引精确匹配个体
		if (HitItem != INDEX_NONE)
		{
			if (HitComp == SoldierHISM)
			{
				for (int32 i = 0; i < Soldiers.Num(); ++i)
				{
					if (Soldiers[i].bAlive && Soldiers[i].InstanceIndex == HitItem) { return i; }
				}
			}
			else if (HitComp == ExtraHISM)
			{
				for (int32 i = 0; i < Soldiers.Num(); ++i)
				{
					if (Soldiers[i].bAlive && Soldiers[i].ExtraIndex == HitItem) { return i; }
				}
			}
		}
	}

	// 3. 兜底 (如范围爆炸重叠等无具体个体组件时): 选取距命中点最近的存活士兵
	const FVector MyLoc = GetActorLocation();
	const FQuat MyQuat = GetActorQuat();
	int32 Best = INDEX_NONE;
	float BestDistSq = MAX_FLT;
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		const FLegionSoldier& S = Soldiers[i];
		if (!S.bAlive) { continue; }
		const FVector SPos = S.SkelComp.IsValid()
			? S.SkelComp->GetComponentLocation() + FVector(0.0f, 0.0f, 92.0f)
			: MyLoc + MyQuat.RotateVector(S.HomeOffset + FVector(0.0f, 0.0f, S.GroundZ + 92.0f));
		const float D2 = FVector::DistSquared(SPos, HitLocation);
		if (D2 < BestDistSq)
		{
			BestDistSq = D2;
			Best = i;
		}
	}
	return Best;
}

void ALegionUnit::ApplyKnockback(const FVector& WorldImpulse)
{
	if (bWiped) { return; }
	const float WeightMul = (Archetype == ELegionClass::Shieldbearer) ? 0.5f : 1.0f;
	KnockbackVelocity += FVector(WorldImpulse.X, WorldImpulse.Y, 0.0f) * WeightMul;
}

int32 ALegionUnit::PickCasualtyIndex(const FVector& FromPos) const
{
	const FVector DirL = GetActorQuat().UnrotateVector(FromPos - GetActorLocation()).GetSafeNormal2D();
	int32 Best = INDEX_NONE;
	float BestD = -MAX_FLT;
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		if (!Soldiers[i].bAlive) { continue; }
		const float D = Soldiers[i].HomeOffset.X * DirL.X + Soldiers[i].HomeOffset.Y * DirL.Y;
		if (D > BestD)
		{
			BestD = D;
			Best = i;
		}
	}
	return Best;
}

void ALegionUnit::StartDying(int32 SoldierIndex)
{
	FLegionSoldier& S = Soldiers[SoldierIndex];
	S.bAlive = false;
	S.HP = 0.0f;
	S.DieT = 0.0f;
	S.SoldierState = ELegionSoldierState::Dying;
	bSoldierTransformsDirty = true;

	// 邻兵阵亡震撼: 周围 320cm 内同团友兵受压制上升, 促发拉扯/换位动作
	const FVector2D FallenXY = S.WorldPosXY;
	for (int32 k = 0; k < Soldiers.Num(); ++k)
	{
		if (k == SoldierIndex || !Soldiers[k].bAlive) { continue; }
		if (FVector2D::DistSquared(Soldiers[k].WorldPosXY, FallenXY) <= 320.0f * 320.0f)
		{
			Soldiers[k].Suppression = FMath::Min(1.0f, Soldiers[k].Suppression + 0.55f);
		}
	}

	// 阵亡瞬间立即关闭命中胶囊, 避免后续子弹继续打到倒地尸体
	if (UCapsuleComponent* P = S.HitProxy.Get())
	{
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (bSkeletalSoldiers)
	{
		if (USkeletalMeshComponent* C = S.SkelComp.Get())
		{
			if (AnimDeathPool.Num() > 0)
			{
				// 按受击方向匹配前/后/左/右倒地动画 (AnimDeathPool 若按 Front/Back/Left/Right 加载)
				int32 PickIdx = SoldierIndex % AnimDeathPool.Num();
				if (!S.DeathImpulseDir.IsNearlyZero())
				{
					const FVector Fwd = FRotator(0.0f, S.CurYaw, 0.0f).Vector().GetSafeNormal2D();
					const FVector Right(-Fwd.Y, Fwd.X, 0.0f);
					const float DotF = FVector::DotProduct(Fwd, S.DeathImpulseDir);
					const float DotR = FVector::DotProduct(Right, S.DeathImpulseDir);
					FString WantKey = TEXT("Front");
					if (FMath::Abs(DotF) >= FMath::Abs(DotR))
					{
						WantKey = (DotF > 0.0f) ? TEXT("Back") : TEXT("Front");
					}
					else
					{
						WantKey = (DotR > 0.0f) ? TEXT("Right") : TEXT("Left");
					}
					TArray<int32> Matching;
					for (int32 a = 0; a < AnimDeathPool.Num(); ++a)
					{
						if (AnimDeathPool[a] && AnimDeathPool[a]->GetName().Contains(WantKey))
						{
							Matching.Add(a);
						}
					}
					if (Matching.Num() > 0)
					{
						PickIdx = Matching[SoldierIndex % Matching.Num()];
					}
				}
				UAnimationAsset* DeathAnim = AnimDeathPool[PickIdx].Get();
				if (LegionUnitInternal::IsSafeSingleNodeAsset(DeathAnim))
				{
					C->PlayAnimation(DeathAnim, false);
					C->SetPosition(0.0f, false);
				}
			}
			C->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void ALegionUnit::FireRangedVolley(ALegionUnit* Target)
{
	if (!Target || Target->IsWiped() || AliveCount <= 0) { return; }

	const float ElevMul = FLegionTerrainGrid::ComputeElevationMultiplier(GetActorLocation().Z, Target->GetActorLocation().Z);
	// 林地掩体: 目标站在树冠下时承受的远程火力缩水
	const float CoverMul = (Target->TerrainGrid && Target->TerrainGrid->IsBuilt())
		? Target->TerrainGrid->GetRangedCoverMultiplier(Target->GetActorLocation()) : 1.0f;
	const float VolleyDamage = (float)AliveCount * DamagePerSoldierPerSecond * ArchetypeStats.VolleyInterval * ElevMul * CoverMul;

	Target->ApplyDamage(VolleyDamage, this, true);
	if (ArchetypeStats.KnockbackImpulse > 0.0f)
	{
		const FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		Target->ApplyKnockback(Dir * ArchetypeStats.KnockbackImpulse);
	}

	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		const FLinearColor BeamCol = (Team == ELegionTeam::Player)
			? FLinearColor(0.15f, 0.78f, 1.0f, 0.95f)
			: FLinearColor(1.0f, 0.22f, 0.12f, 0.95f);

		const int32 BeamCount = FMath::Clamp(AliveCount / 3, 2, 7);
		const FQuat MyQ = GetActorQuat();
		const FQuat TgtQ = Target->GetActorQuat();
		for (int32 b = 0; b < BeamCount; ++b)
		{
			const float OffY = ((float)b / FMath::Max(1, BeamCount - 1) - 0.5f) * FormationHalfExtent.Y * 1.4f;
			const FVector SPos = GetActorLocation() + MyQ.RotateVector(FVector(40.0f, OffY, 125.0f));
			const FVector EPos = Target->GetActorLocation() + TgtQ.RotateVector(FVector(0.0f, -OffY * 0.7f, 95.0f));
			GM->AddTracerBeam(SPos, EPos, BeamCol, 0.28f);
		}
	}
}

int32 ALegionUnit::ApplyBulletHitToSoldier(int32 SoldierIndex, float DamageAmount, const FVector& HitDirection)
{
	if (bWiped || DamageAmount <= 0.0f) { return 0; }
	if (!Soldiers.IsValidIndex(SoldierIndex) || !Soldiers[SoldierIndex].bAlive) { return 0; }

	FLegionSoldier& S = Soldiers[SoldierIndex];
	const float Applied = FMath::Min(S.HP, DamageAmount);
	S.HP = FMath::Max(0.0f, S.HP - Applied);
	LegionHP = FMath::Max(0.0f, LegionHP - Applied);
	S.Suppression = FMath::Min(1.0f, S.Suppression + 0.75f);

	int32 Killed = 0;
	if (S.HP <= KINDA_SMALL_NUMBER)
	{
		S.HP = 0.0f;
		S.DeathImpulseDir = HitDirection.GetSafeNormal2D();
		AliveCount = FMath::Max(0, AliveCount - 1);
		StartDying(SoldierIndex);
		Killed = 1;
		CheckAndHandleWiped();
	}
	const FVector ShooterGuess = FVector(S.WorldPosXY.X, S.WorldPosXY.Y, GetActorLocation().Z) - HitDirection * 1200.0f;
	NotifyIncomingFire(ShooterGuess);
	return Killed;
}

bool ALegionUnit::GetAliveSoldierFeet(int32 SoldierIndex, FVector& OutFeet) const
{
	if (!Soldiers.IsValidIndex(SoldierIndex) || !Soldiers[SoldierIndex].bAlive) { return false; }
	const FLegionSoldier& S = Soldiers[SoldierIndex];
	if (USkeletalMeshComponent* C = S.SkelComp.Get())
	{
		OutFeet = C->GetComponentLocation();
		return true;
	}
	// HISM 退化路径: 兵团 Actor 保持零旋转, 世界位置可直接由局部偏移平移得到
	const FVector Off = !S.LocalOffset.IsNearlyZero(0.5f) ? S.LocalOffset : (S.HomeOffset + FVector(0.0f, 0.0f, S.GroundZ));
	const FVector LegionLoc = GetActorLocation();
	OutFeet = LegionLoc + Off;
	return true;
}

int32 ALegionUnit::PickRandomAliveSoldier() const
{
	if (AliveCount <= 0) { return INDEX_NONE; }
	// 确定性起点单向扫描, 均摊到任一存活个体
	const int32 Count = Soldiers.Num();
	const int32 StartIdx = UnitRng.RandHelper(Count);
	for (int32 k = 0; k < Count; ++k)
	{
		const int32 Idx = (StartIdx + k) % Count;
		if (Soldiers[Idx].bAlive) { return Idx; }
	}
	return INDEX_NONE;
}
