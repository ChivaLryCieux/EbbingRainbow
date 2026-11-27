// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionUnit.h"

#include "Core/ERAssetCatalog.h"
#include "Core/ERCollisionChannels.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Legion/LegionPerf.h"
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

ALegionUnit::ALegionUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	PickBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PickBox"));
	SetRootComponent(PickBox);
	PickBox->SetBoxExtent(FVector(400.0f, 400.0f, 160.0f));
	PickBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickBox->SetCollisionResponseToChannel(ER_TRACE_BATTLELOS, ECR_Ignore);
	PickBox->SetCanEverAffectNavigation(false);

	SoldierHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SoldierHISM"));
	SoldierHISM->SetupAttachment(PickBox);
	// 只挡 WorldDynamic (指挥官手枪弹): 命中具体士兵实例而非隐形判定盒
	SoldierHISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SoldierHISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	SoldierHISM->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SoldierHISM->SetCanEverAffectNavigation(false);
	SoldierHISM->SetMobility(EComponentMobility::Movable);

	ExtraHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ExtraHISM"));
	ExtraHISM->SetupAttachment(PickBox);
	ExtraHISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExtraHISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExtraHISM->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	ExtraHISM->SetCanEverAffectNavigation(false);
	ExtraHISM->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakeFrameBar = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(PickBox);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		if (CubeFinder.Succeeded()) { C->SetStaticMesh(CubeFinder.Object); }
		if (BasicMat.Succeeded())   { C->SetMaterial(0, BasicMat.Object); }
		return C;
	};
	FrameTop       = MakeFrameBar(TEXT("FrameTop"));
	FrameBottom    = MakeFrameBar(TEXT("FrameBottom"));
	FrameLeft      = MakeFrameBar(TEXT("FrameLeft"));
	FrameRight     = MakeFrameBar(TEXT("FrameRight"));
	FrontShieldBar = MakeFrameBar(TEXT("FrontShieldBar"));

	OrderLine = MakeFrameBar(TEXT("OrderLine"));
	OrderLine->SetVisibility(false);
	MoveMarker = MakeFrameBar(TEXT("MoveMarker"));
	MoveMarker->SetVisibility(false);

	if (CylinderFinder.Succeeded())
	{
		SoldierHISM->SetStaticMesh(CylinderFinder.Object);
	}
	if (CubeFinder.Succeeded())
	{
		ExtraHISM->SetStaticMesh(CubeFinder.Object);
	}
	if (BasicMat.Succeeded())
	{
		SoldierHISM->SetMaterial(0, BasicMat.Object);
		ExtraHISM->SetMaterial(0, BasicMat.Object);
	}
	UnitRng.Initialize(1337);
}

void ALegionUnit::BeginPlay()
{
	Super::BeginPlay();
	UpdateFrameVisual();
}

void ALegionUnit::SetDeterministicSeed(int32 InSeed)
{
	UnitRng.Initialize(InSeed == 0 ? 1337 : InSeed);
}

void ALegionUnit::InitLegion(ELegionTeam InTeam, ELegionClass InArchetype, int32 InSoldierCount,
	UStaticMesh* InBodyMesh, UStaticMesh* InExtraMesh, UMaterialInterface* InBodyMaterial,
	USkeletalMesh* InSkeletalBody,
	UAnimationAsset* InAnimIdle,
	UAnimationAsset* InAnimMove,
	const TArray<UAnimationAsset*>& InAnimAttacks,
	const TArray<UAnimationAsset*>& InAnimDeaths,
	int32 InCampaignLegionId,
	const FString& InCallsign,
	int32 InSyncLevel,
	float InInitialFacingYaw,
	const FLinearColor& InSkeletalTint)
{
	Team = InTeam;
	Archetype = InArchetype;
	ArchetypeStats = FLegionArchetypeStats::GetForClass(InArchetype);
	CampaignLegionId = InCampaignLegionId;
	SyncLevel = FMath::Clamp(InSyncLevel, 1, 5);
	SkeletalTint = InSkeletalTint;
	Callsign = InCallsign.IsEmpty()
		? (Team == ELegionTeam::Player ? ERGalacticMeta::GetBrigadeName(1) : ERGalacticMeta::GetEnemyBrigadeName(1))
		: InCallsign;

	// 应用兵种原型数值与虹膜同步加成 (+8% 输出, +4% 机动 / 级)
	const float SyncMul = 1.0f + (SyncLevel - 1) * 0.08f;
	MoveSpeed = ArchetypeStats.MoveSpeed * (1.0f + (SyncLevel - 1) * 0.04f);
	SoldierHP = ArchetypeStats.SoldierHP;
	DamagePerSoldierPerSecond = ArchetypeStats.DamagePerSoldierPerSec * SyncMul;
	EngageRange = ArchetypeStats.EngageRange;
	FormationColumnBias = ArchetypeStats.ColumnBias;
	SoldierXYScale = ArchetypeStats.BodyXYScale;
	FacingYaw = FMath::UnwindDegrees(InInitialFacingYaw);
	DesiredFacingYaw = FacingYaw;

	// 默认确定性种子 (可由 GameMode::SpawnLegion 用 ActiveMatchSeed 覆盖)
	const int32 DefaultSeed = (InCampaignLegionId != INDEX_NONE ? InCampaignLegionId : 17) * 73856093
		^ GetTypeHash(Callsign) ^ ((int32)InTeam * 19349663) ^ ((int32)InArchetype * 83492791);
	UnitRng.Initialize(DefaultSeed == 0 ? 1337 : DefaultSeed);

	TargetSoldierCount = FMath::Max(1, InSoldierCount);
	bSkeletalSoldiers = (InSkeletalBody != nullptr);
	SkeletalBodyMesh = InSkeletalBody;
	SoldierBodyMaterial = InBodyMaterial;
	AnimIdle = InAnimIdle;
	AnimMove = InAnimMove;
	AnimAttackPool = InAnimAttacks;
	AnimDeathPool = InAnimDeaths;

	if (InBodyMesh) { SoldierHISM->SetStaticMesh(InBodyMesh); }
	ExtraHISM->SetStaticMesh(InExtraMesh);
	if (InBodyMaterial)
	{
		for (int32 i = 0; i < SoldierHISM->GetNumMaterials(); ++i)
		{
			SoldierHISM->SetMaterial(i, InBodyMaterial);
		}
		for (int32 i = 0; i < ExtraHISM->GetNumMaterials(); ++i)
		{
			ExtraHISM->SetMaterial(i, InBodyMaterial);
		}
	}

	// 敌军地面判定盒不拦截 Visibility: 防止地面移动指令误触集火 (敌军集火需抬头瞄准天空 Logo 右键)
	if (PickBox)
	{
		PickBox->SetCollisionResponseToChannel(ECC_Visibility,
			Team == ELegionTeam::Enemy ? ECR_Ignore : ECR_Block);
	}

	RebuildFormation();
	ConformToTerrain(10.0f);

	LegionHP = (float)Soldiers.Num() * FMath::Max(1, SoldierHP);
	AliveCount = Soldiers.Num();
	bWiped = false;
	State = ELegionState::Holding;
	Directive = ELegionDirective::Standby;
	Alertness = ELegionAlertness::Relaxed;
	AlertLevel = 0.0f;
	AlertDecayCooldown = 0.0f;
	DirectiveDecisionTimer = 0.0f;
	DirectiveHoldTimer = 0.0f;
	DirectiveSpeedMul = 1.0f;
	GuardAnchor = GetActorLocation();
	GuardBaseYaw = FacingYaw;
	GuardScanPhase = UnitRng.FRandRange(0.0f, 6.28318f);
	PatrolWaypoints.Reset();
	PatrolCursor = 0;
	PatrolDirection = 1;
	PatrolDwellTimer = 0.0f;
	bHasReconGoal = false;
	ReconDwellTimer = 0.0f;
	ThreatMemoryTimer = 0.0f;
	ContactBroadcastCooldown = 0.0f;
	FlankCommitTimer = 0.0f;
	AttackTarget = nullptr;
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	Waypoints.Reset();
	WaypointIndex = 0;
	bSoldierTransformsDirty = true;
	UpdateFrameVisual();
}

void ALegionUnit::RebuildFormation()
{
	SoldierHISM->ClearInstances();
	ExtraHISM->ClearInstances();
	for (FLegionSoldier& S : Soldiers)
	{
		if (USkeletalMeshComponent* C = S.SkelComp.Get())
		{
			C->DestroyComponent();
		}
		if (UCapsuleComponent* P = S.HitProxy.Get())
		{
			P->DestroyComponent();
		}
	}
	Soldiers.Reset();

	const int32 N = TargetSoldierCount;

	// 自然散落战团站位: 黄金角向日葵采样 + 强随机扰动 + 兵种散布 + 最小间距排斥松弛
	// 彻底消除“方格矩阵”死板感, 且保证士兵绝不相互穿模重叠
	const float ClassSpacingMul = (Archetype == ELegionClass::Marksman) ? 1.24f
		: (Archetype == ELegionClass::Vanguard ? 1.14f : 1.04f);
	const float BaseStep = SoldierSpacing * ClassSpacingMul;

	auto Hash01 = [](int32 Idx, int32 Salt, int32 InTeam, int32 InClass) -> float
	{
		uint32 H = (uint32)(Idx * 747796405u + Salt * 2891336453u + InTeam * 1664525u + InClass * 1013904223u);
		H ^= H >> 16;
		H *= 0x85ebca6bu;
		H ^= H >> 13;
		return (float)(H & 0xFFFF) / 65535.0f;
	};

	TArray<FVector> Offsets;
	Offsets.SetNum(N);
	constexpr float GoldenAngle = 2.39996323f;
	const float RadiusScale = BaseStep * 0.62f;
	const float AspectX = (Archetype == ELegionClass::Vanguard) ? 1.15f : 0.92f;
	const float AspectY = (Archetype == ELegionClass::Shieldbearer || Archetype == ELegionClass::Marksman) ? 1.22f : 1.02f;

	for (int32 i = 0; i < N; ++i)
	{
		const float RandR = 0.72f + 0.58f * Hash01(i, 1, (int32)Team, (int32)Archetype);
		const float RandA = (Hash01(i, 2, (int32)Team, (int32)Archetype) - 0.5f) * 0.95f;
		const float Angle = (float)i * GoldenAngle + RandA;
		const float Rad = RadiusScale * FMath::Sqrt((float)i + 0.65f) * RandR;
		Offsets[i] = FVector(
			FMath::Cos(Angle) * Rad * AspectX + (Hash01(i, 4, (int32)Team, 0) - 0.5f) * 65.0f,
			FMath::Sin(Angle) * Rad * AspectY + (Hash01(i, 5, (int32)Team, 0) - 0.5f) * 65.0f,
			0.0f);
	}

	// 4 轮最小间距排斥松弛, 确保自然分散且不重合
	const float MinDist = 128.0f;
	const float MinDistSq = MinDist * MinDist;
	for (int32 Iter = 0; Iter < 4; ++Iter)
	{
		for (int32 i = 0; i < N; ++i)
		{
			for (int32 j = i + 1; j < N; ++j)
			{
				const FVector D = Offsets[j] - Offsets[i];
				const float D2 = D.SizeSquared2D();
				if (D2 < MinDistSq)
				{
					const float Dist = FMath::Max(1.0f, FMath::Sqrt(D2));
					const FVector Push = (D2 > 1.0f ? D / Dist : FVector(1.0f, 0.0f, 0.0f)) * ((MinDist - Dist) * 0.5f);
					Offsets[i] -= Push;
					Offsets[j] += Push;
				}
			}
		}
	}

	// 重新质心归零并统计自然包围盒
	FVector Mean = FVector::ZeroVector;
	for (const FVector& P : Offsets) { Mean += P; }
	if (N > 0) { Mean /= (float)N; }

	float MaxAbsX = 240.0f;
	float MaxAbsY = 240.0f;
	for (FVector& P : Offsets)
	{
		P -= Mean;
		MaxAbsX = FMath::Max(MaxAbsX, FMath::Abs(P.X) + 110.0f);
		MaxAbsY = FMath::Max(MaxAbsY, FMath::Abs(P.Y) + 110.0f);
	}

	Soldiers.Reserve(N);
	const FVector LegionLoc = GetActorLocation();
	const FLegionInstancedTransformParams InstancedParams = BuildInstancedTransformParams();
	for (int32 Idx = 0; Idx < N; ++Idx)
	{
		FLegionSoldier S;
		S.HomeOffset = Offsets[Idx];
		S.LocalOffset = Offsets[Idx];
		// 槽位不随军团扫视整体旋转, 防止扇面警戒变成旋转木马
		const FVector InitWorld = LegionLoc + Offsets[Idx];
		S.WorldPosXY = FVector2D(InitWorld.X, InitWorld.Y);
		S.WorldVelXY = FVector2D::ZeroVector;
		S.TacticalOffsetXY = FVector2D::ZeroVector;
		S.MoveGoalOffsetXY = FVector2D::ZeroVector;

		float InitZ = LegionLoc.Z;
		if (TraceGroundZ(InitWorld, InitZ))
		{
			S.WorldZ = InitZ;
			float GridZ = InitZ;
			if (TerrainGrid && TerrainGrid->IsBuilt() && TerrainGrid->SampleHeightBilinear(InitWorld, GridZ))
			{
				S.RayGroundOffset = FMath::Clamp(InitZ - GridZ, -120.0f, 120.0f);
				S.SmoothedRayOffset = S.RayGroundOffset;
			}
		}
		else if (TerrainGrid && TerrainGrid->IsBuilt() && TerrainGrid->SampleHeightBilinear(InitWorld, InitZ))
		{
			S.WorldZ = InitZ;
		}
		else
		{
			S.WorldZ = LegionLoc.Z;
		}
		S.GroundZ = S.WorldZ - LegionLoc.Z;
		S.LocalOffset.Z = S.GroundZ;
		S.bWorldZInitialized = true;

		S.HP = (float)FMath::Max(1, SoldierHP);
		S.DeathImpulseDir = FVector::ZeroVector;
		S.Phase = (float)((Idx * 37 + (int32)Team * 11) % 97) / 97.0f * 6.28318f;
		S.JitterYaw = (Hash01(Idx, 3, (int32)Team, (int32)Archetype) - 0.5f) * 2.0f * (SoldierYawJitterDeg + 14.0f);
		S.CurYaw = FMath::UnwindDegrees(FacingYaw + S.JitterYaw * 0.65f);
		S.StepFacingYaw = S.CurYaw;

		// 个体确定性性格与噪声参数: 同团内同时有人好动漫步、有人沉稳驻足
		const float HRest = Hash01(Idx, 11, (int32)Team, (int32)Archetype);
		const float HAgg  = Hash01(Idx, 13, (int32)Team, (int32)Archetype);
		const float HRate = Hash01(Idx, 17, (int32)Team, (int32)Archetype);
		S.NoiseSeedX = Hash01(Idx, 19, (int32)Team, (int32)Archetype) * 197.0f + 11.3f;
		S.NoiseSeedY = Hash01(Idx, 23, (int32)Team, (int32)Archetype) * 211.0f + 37.9f;
		S.TraitRestlessness = 0.55f + 0.95f * HRest;
		S.TraitAggression   = 0.65f + 0.75f * HAgg;
		S.TraitPlayRate     = 0.92f + 0.16f * HRate;

		// 初始开局拆成异步的待机/漫步双态 (漫步者选定明确、无碰撞的短途落脚点)
		const bool bStartWander = (Hash01(Idx, 29, (int32)Team, (int32)Archetype) > 0.62f);
		S.SoldierState = bStartWander ? ELegionSoldierState::RelaxedWander : ELegionSoldierState::RelaxedIdle;
		S.PerceivedAlertness = ELegionAlertness::Relaxed;
		S.StateTimer = 1.6f + 3.8f * Hash01(Idx, 31, (int32)Team, (int32)Archetype);
		S.GlanceTimer = 1.8f + 3.6f * Hash01(Idx, 37, (int32)Team, (int32)Archetype);
		S.GlanceYawOffset = (Hash01(Idx, 41, (int32)Team, (int32)Archetype) - 0.5f) * 44.0f;
		S.FireTimer = 0.25f + 1.4f * Hash01(Idx, 43, (int32)Team, (int32)Archetype);
		S.BurstShotsLeft = 3 + (int32)(Hash01(Idx, 47, (int32)Team, (int32)Archetype) * 4.0f);
		if (bStartWander)
		{
			const float Ang = Hash01(Idx, 53, (int32)Team, 0) * 6.28318f;
			const float Dist = 75.0f + 55.0f * Hash01(Idx, 59, (int32)Team, 0);
			S.MoveGoalOffsetXY = FVector2D(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist);
			S.TacticalOffsetXY = S.MoveGoalOffsetXY;
			S.StepFacingYaw = FMath::RadiansToDegrees(Ang);
		}

		if (bSkeletalSoldiers && SkeletalBodyMesh)
		{
			SpawnSkeletalSoldier(S);
		}
		else
		{
			FTransform BodyXf, HeadXf;
			ComputeSoldierTransforms(S, 0.0f, InstancedParams, BodyXf, HeadXf);
			S.InstanceIndex = SoldierHISM->AddInstance(BodyXf, false);
			if (ExtraHISM->GetStaticMesh())
			{
				S.ExtraIndex = ExtraHISM->AddInstance(HeadXf, false);
			}
		}

		Soldiers.Add(S);
	}

	FormationHalfExtent = FVector(MaxAbsX, MaxAbsY, 0.0f);
	PickBox->SetBoxExtent(FVector(MaxAbsX, MaxAbsY, 1800.0f));

	// 隐藏生硬的 3D 矩形边框木棍, 改由 HUD 投影微细战术角标与地面朝向箭头呈现
	for (UStaticMeshComponent* Bar : { FrameTop, FrameBottom, FrameLeft, FrameRight, FrontShieldBar })
	{
		if (Bar) { Bar->SetVisibility(false); }
	}

	UE_LOG(LogERLegion, Log, TEXT("[Legion] %s init: class=%s team=%d soldiers=%d organic extent=(%.0f,%.0f)"),
		*Callsign, *ERGalacticMeta::GetClassCode(Archetype), (int32)Team, N, MaxAbsX, MaxAbsY);
}

void ALegionUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LegionPerf::FScope PerfScope(LegionPerf::Get().UnitTickMs);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bool bNeedTransforms = bSoldierTransformsDirty;
	ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);

	if (!bWiped)
	{
		// 1. 军团弱 AI 决策树与态势感知 (仅在开战后推进)
		{ LegionPerf::FScope S_(LegionPerf::Get().DirectiveMs); UpdateWeakAIDirective(DeltaSeconds, GM); }

		// 2. 处理击退惯性位移 + 陡坡跌落惩戒
		if (!KnockbackVelocity.IsNearlyZero(8.0f))
		{
			const FVector Next = GetActorLocation() + KnockbackVelocity * DeltaSeconds;
			SetActorLocation(Next);
			KnockbackVelocity = FMath::VInterpTo(KnockbackVelocity, FVector::ZeroVector, DeltaSeconds, 5.5f);

			if (TerrainGrid && TerrainGrid->IsBuilt())
			{
				float Z = 0.0f, Slope = 0.0f;
				if (!TerrainGrid->SampleHeightAndSlope(Next, Z, Slope) || Slope > 44.0f)
				{
					ApplyDamage((float)SoldierHP * 1.5f * DeltaSeconds * 10.0f, LastAttacker.Get(), false);
					KnockbackVelocity = FVector::ZeroVector;
				}
			}
		}

		{ LegionPerf::FScope S_(LegionPerf::Get().TerrainConformMs); ConformToTerrain(DeltaSeconds); }

		// 3. 目标获取与路径追踪 (带射程滞回 + 突击刃兵侧翼绕行航点)
		FVector ImmediateGoal = FVector::ZeroVector;
		bool bHasGoal = false;
		bool bAttackGoal = false;

		if (ALegionUnit* Target = AttackTarget.Get())
		{
			if (Target->IsWiped())
			{
				AttackTarget = nullptr;
				Waypoints.Reset();
				State = ELegionState::Holding;
			}
			else
			{
				bAttackGoal = true;
				bHasGoal = true;
				const FVector NavDest = (FlankCommitTimer > 0.0f && !FlankWaypoint.IsNearlyZero())
					? FlankWaypoint : Target->GetActorLocation();
				const float DistToEnemy = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
				const float EffRange = GetEffectiveEngageRange(Target);
				const float StopRange = (State == ELegionState::Engaged) ? (EffRange * 0.96f) : (EffRange * 0.85f);

				if (DistToEnemy <= StopRange)
				{
					ImmediateGoal = Target->GetActorLocation();
					FlankCommitTimer = 0.0f;
				}
				else
				{
					RepathTimer -= DeltaSeconds;
					if (RepathTimer <= 0.0f || Waypoints.Num() == 0)
					{
						RepathTimer = 0.45f;
						if (TerrainGrid && TerrainGrid->IsBuilt())
						{
							TerrainGrid->FindPath(GetActorLocation(), NavDest, Waypoints);
						}
						else
						{
							Waypoints = { NavDest };
						}
						WaypointIndex = 0;
					}
					if (Waypoints.IsValidIndex(WaypointIndex))
					{
						ImmediateGoal = Waypoints[WaypointIndex];
						if (FVector::Dist2D(GetActorLocation(), ImmediateGoal) <= 180.0f && WaypointIndex + 1 < Waypoints.Num())
						{
							++WaypointIndex;
							ImmediateGoal = Waypoints[WaypointIndex];
						}
						if (FlankCommitTimer > 0.0f && FVector::Dist2D(GetActorLocation(), FlankWaypoint) <= 320.0f)
						{
							FlankCommitTimer = 0.0f;
						}
					}
					else
					{
						ImmediateGoal = NavDest;
					}
				}
			}
		}
		else if (bHasMoveDestination)
		{
			bHasGoal = true;
			if (Waypoints.IsValidIndex(WaypointIndex))
			{
				ImmediateGoal = Waypoints[WaypointIndex];
				if (FVector::Dist2D(GetActorLocation(), ImmediateGoal) <= 160.0f && WaypointIndex + 1 < Waypoints.Num())
				{
					++WaypointIndex;
					ImmediateGoal = Waypoints[WaypointIndex];
				}
			}
			else
			{
				ImmediateGoal = MoveDestination;
			}
		}

		if (bHasGoal)
		{
			const FVector FinalDest = bAttackGoal ? AttackTarget->GetActorLocation() : MoveDestination;
			const float DistFinal = FVector::Dist2D(GetActorLocation(), FinalDest);
			const float RawRange = bAttackGoal ? GetEffectiveEngageRange(AttackTarget.Get()) : 95.0f;
			const float EffRange = bAttackGoal
				? ((State == ELegionState::Engaged) ? RawRange * 0.96f : RawRange * 0.85f)
				: RawRange;
			const bool bInRange = (DistFinal <= EffRange);

			if (bInRange)
			{
				if (bAttackGoal)
				{
					State = ELegionState::Engaged;
					DesiredFacingYaw = (FinalDest - GetActorLocation()).Rotation().Yaw;
				}
				else
				{
					bHasMoveDestination = false;
					Waypoints.Reset();
					State = ELegionState::Holding;
				}
			}
			else
			{
				State = ELegionState::Moving;
				const FVector ToWp = ImmediateGoal - GetActorLocation();
				const float CurMoveSpeed = MoveSpeed * FMath::Clamp(DirectiveSpeedMul, 0.45f, 1.25f);
				const FVector Step = ToWp.GetSafeNormal2D() * CurMoveSpeed * DeltaSeconds;
				{ LegionPerf::FScope S_(LegionPerf::Get().ActorMoveMs); SetActorLocation(GetActorLocation() + Step); }
				if (!ToWp.IsNearlyZero(10.0f))
				{
					const float MoveYaw = ToWp.Rotation().Yaw;
					FacingYaw = MoveYaw;
					if (!bHasCustomFacing)
					{
						DesiredFacingYaw = MoveYaw;
					}
				}
			}
		}

		// 4. 兵团互斥防重叠
		ApplyPeerSeparation(DeltaSeconds);

		// 5. 军团逻辑朝向平滑收敛
		if (State == ELegionState::Holding || State == ELegionState::Engaged)
		{
			FacingYaw = FMath::FixedTurn(FacingYaw, DesiredFacingYaw, FormationTurnSpeed * DeltaSeconds);
		}

		// 6. 单兵自治微状态机 + 世界空间噪声走位 + 抢射界 + 2D BlendSpace + 真实弹道开火
		UpdateSoldiersMicroAI(DeltaSeconds, Now, GM);
		bNeedTransforms = true;
	}

	// ---- 阵亡推进 ----
	bool bAnyDying = false;
	{ LegionPerf::FScope DeathScope_(LegionPerf::Get().DeathLoopMs);
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		if (S.DieT >= 0.0f && S.DieT < 1.0f)
		{
			const float Dur = bSkeletalSoldiers ? 2.6f : 0.55f;
			S.DieT = FMath::Min(1.0f, S.DieT + DeltaSeconds / Dur);
			bAnyDying = true;

			USkeletalMeshComponent* C = S.SkelComp.Get();
			if (bSkeletalSoldiers && C && !S.bRagdolled && S.DieT * Dur >= 0.5f)
			{
				S.bRagdolled = true;
				if (UCapsuleComponent* P = S.HitProxy.Get())
				{
					P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
				EnableSoldierRagdoll(C, i);
			}
			if (S.DieT >= 1.0f && C)
			{
				C->SetComponentTickEnabled(false);
			}
		}
	}
	} // DeathScope_
	bNeedTransforms |= bAnyDying;

	UpdateOrderVisuals(Now);

	if (bNeedTransforms)
	{
		if (bSkeletalSoldiers)
		{
			WriteSkeletalSoldierRepresentation(DeltaSeconds);
		}
		else
		{
			WriteAllSoldierTransforms(Now);
			bSoldierTransformsDirty = bAnyDying;
		}
	}
}

void ALegionUnit::ResetMotionTelemetry()
{
	for (FLegionSoldier& S : Soldiers)
	{
		S.MaxFrameJumpZ = 0.0f;
		S.MaxFrameJumpAnimDir = 0.0f;
		S.ZFlipCount = 0;
		S.YawFlipCount = 0;
		S.PrevDeltaZ = 0.0f;
		S.PrevDeltaYaw = 0.0f;
		S.PrevTelZ = S.WorldZ;
		S.PrevTelYaw = S.CurYaw;
		S.PrevTelAnimDir = S.SmoothedAnimDir;
		S.bTelPrimed = false;
	}
}

bool ALegionUnit::EvaluateMotionStability(FString& OutSummary) const
{
	float MaxJumpZ = 0.0f;
	float MaxJumpAnimDir = 0.0f;
	int32 MaxZFlips = 0;
	int32 MaxYawFlips = 0;
	int32 PlantedChecked = 0;
	int32 PlantedZeroVel = 0;

	for (const FLegionSoldier& S : Soldiers)
	{
		if (!S.bAlive) { continue; }
		MaxJumpZ = FMath::Max(MaxJumpZ, S.MaxFrameJumpZ);
		MaxJumpAnimDir = FMath::Max(MaxJumpAnimDir, S.MaxFrameJumpAnimDir);
		MaxZFlips = FMath::Max(MaxZFlips, S.ZFlipCount);
		MaxYawFlips = FMath::Max(MaxYawFlips, S.YawFlipCount);

		const bool bPlanted = (S.SoldierState == ELegionSoldierState::RelaxedIdle
			|| S.SoldierState == ELegionSoldierState::AlertScan
			|| S.SoldierState == ELegionSoldierState::CombatAimFire);
		if (bPlanted)
		{
			++PlantedChecked;
			if (S.WorldVelXY.Size() < 14.0f) { ++PlantedZeroVel; }
		}
	}

	const bool bOk = (MaxJumpZ < 4.5f) && (MaxZFlips <= 3) && (MaxYawFlips <= 3) && (MaxJumpAnimDir < 30.0f);
	OutSummary = FString::Printf(TEXT("maxJumpZ=%.2fcm maxZFlips=%d maxYawFlips=%d maxAnimDirJump=%.1fdeg plantedZero=%d/%d"),
		MaxJumpZ, MaxZFlips, MaxYawFlips, MaxJumpAnimDir, PlantedZeroVel, PlantedChecked);
	return bOk;
}

void ALegionUnit::GetSoldierStateDistribution(TMap<ELegionSoldierState, int32>& OutCounts, int32& OutMovingCount, float& OutYawSpreadDeg) const
{
	OutCounts.Reset();
	OutMovingCount = 0;
	OutYawSpreadDeg = 0.0f;
	float MinDelta = 180.0f;
	float MaxDelta = -180.0f;
	int32 AliveSeen = 0;

	for (const FLegionSoldier& S : Soldiers)
	{
		if (!S.bAlive) { continue; }
		OutCounts.FindOrAdd(S.SoldierState) += 1;
		if (S.WorldVelXY.SizeSquared() > 400.0f
			|| S.SoldierState == ELegionSoldierState::RelaxedWander
			|| S.SoldierState == ELegionSoldierState::AlertPatrolStep
			|| S.SoldierState == ELegionSoldierState::SeekingFireLine
			|| S.SoldierState == ELegionSoldierState::CombatSkirmish
			|| S.SoldierState == ELegionSoldierState::FormationMarch)
		{
			++OutMovingCount;
		}
		const float Delta = FMath::FindDeltaAngleDegrees(FacingYaw, S.CurYaw);
		MinDelta = FMath::Min(MinDelta, Delta);
		MaxDelta = FMath::Max(MaxDelta, Delta);
		++AliveSeen;
	}
	if (AliveSeen > 1)
	{
		OutYawSpreadDeg = MaxDelta - MinDelta;
	}
}
