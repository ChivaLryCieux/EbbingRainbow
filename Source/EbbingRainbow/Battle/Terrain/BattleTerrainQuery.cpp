// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Terrain/BattleTerrainQuery.h"
#include "Battle/Units/BattleUnit.h"
#include "Core/ERCollisionChannels.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UBattleTerrainQuery* UBattleTerrainQuery::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBattleTerrainQuery>() : nullptr;
}

bool UBattleTerrainQuery::TraceLine(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	// 战斗 LOS 专用通道: 单位在初始化时显式 Ignore, 地形几何按默认响应 Block,
	// 因此不再需要 "忽略所有战斗单位" 的每查询收集 (原实现每次 Evaluate 都
	// TActorIterator 全场扫一遍 + 建忽略数组)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BattleLOS), /*bTraceComplex*/ false);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, From, To, ER_TRACE_BATTLELOS, Params);
	return !bBlocked;
}

bool UBattleTerrainQuery::EvaluateLineOfSight(const FVector& From, const FVector& To) const
{
	return TraceLine(From, To);
}

bool UBattleTerrainQuery::HasLineOfSight(ABattleUnit* Attacker, ABattleUnit* Target) const
{
	return Evaluate(Attacker, Target).bHasLineOfSight;
}

FBattleTerrainContext UBattleTerrainQuery::Evaluate(ABattleUnit* Attacker, ABattleUnit* Target) const
{
	FBattleTerrainContext Context;
	if (!Attacker || !Target)
	{
		return Context;
	}

	const FVector AttLoc = Attacker->GetActorLocation();
	const FVector TgtLoc = Target->GetActorLocation();
	const float AttHH = Attacker->GetCapsuleHalfHeight();
	const float TgtHH = Target->GetCapsuleHalfHeight();

	// 攻击者胸口高度 → 目标 脚/胸/头 三点
	const FVector From = AttLoc + FVector(0.0f, 0.0f, AttHH * 0.4f);
	const FVector Low = TgtLoc + FVector(0.0f, 0.0f, -TgtHH * 0.6f);
	const FVector Mid = TgtLoc;
	const FVector High = TgtLoc + FVector(0.0f, 0.0f, TgtHH * 0.6f);

	const int32 ClearCount =
		(TraceLine(From, Low) ? 1 : 0)
		+ (TraceLine(From, Mid) ? 1 : 0)
		+ (TraceLine(From, High) ? 1 : 0);

	if (ClearCount == 0)
	{
		Context.Cover = ECoverQuality::Full;
		Context.bHasLineOfSight = false;
	}
	else if (ClearCount == 3)
	{
		Context.Cover = ECoverQuality::None;
		Context.bHasLineOfSight = true;
	}
	else
	{
		Context.Cover = ECoverQuality::Half;
		Context.bHasLineOfSight = true;
	}

	// 高低差: 脚底 Z 差
	Context.HeightDelta = AttLoc.Z - TgtLoc.Z;

	// 朝向关系: 攻击者相对目标正面/侧翼/背后
	const FVector ToAttacker = (AttLoc - TgtLoc).GetSafeNormal2D();
	const float FacingDot = FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), ToAttacker);
	if (FacingDot > 0.5f)
	{
		Context.Facing = EFacingRelation::Front; // 目标正对攻击者
	}
	else if (FacingDot < -0.5f)
	{
		Context.Facing = EFacingRelation::Rear;  // 目标背对攻击者
	}
	else
	{
		Context.Facing = EFacingRelation::Side;
	}

	return Context;
}
