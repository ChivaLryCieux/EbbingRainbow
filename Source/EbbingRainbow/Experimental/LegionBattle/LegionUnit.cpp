// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionUnit.h"

#include "Core/ERAssetCatalog.h"
#include "Core/ERCollisionChannels.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionTerrainGrid.h"

#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/ConstructorHelpers.h"

namespace LegionUnitInternal
{
	/** 目标身高 -> 网格缩放/抬升 (让任意网格底盘贴 z=0 且等高) */
	static float NormalizeMesh(const UStaticMesh* Mesh, float TargetHeight, float& OutLift)
	{
		OutLift = 0.0f;
		if (!Mesh) { return 1.0f; }
		const FBox B = Mesh->GetBoundingBox();
		const float Z = B.GetSize().Z;
		if (Z < KINDA_SMALL_NUMBER) { return 1.0f; }
		const float S = TargetHeight / Z;
		OutLift = -B.Min.Z * S;
		return S;
	}
}

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
	SoldierHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoldierHISM->SetCanEverAffectNavigation(false);
	SoldierHISM->SetMobility(EComponentMobility::Movable);

	ExtraHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ExtraHISM"));
	ExtraHISM->SetupAttachment(PickBox);
	ExtraHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
}

void ALegionUnit::BeginPlay()
{
	Super::BeginPlay();
	UpdateFrameVisual();
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
		? FString::Printf(TEXT("%s-%s"),
			Team == ELegionTeam::Player ? TEXT("ALLY") : TEXT("FOE"),
			*ERGalacticMeta::GetClassName(InArchetype))
		: InCallsign;

	// 应用兵种原型数值与虹膜同步加成 (+8% 输出, +5% 机动, -6% 技能冷却 / 级)
	const float SyncMul = 1.0f + (SyncLevel - 1) * 0.08f;
	MoveSpeed = ArchetypeStats.MoveSpeed * (1.0f + (SyncLevel - 1) * 0.04f);
	SoldierHP = ArchetypeStats.SoldierHP;
	DamagePerSoldierPerSecond = ArchetypeStats.DamagePerSoldierPerSec * SyncMul;
	EngageRange = ArchetypeStats.EngageRange;
	FormationColumnBias = ArchetypeStats.ColumnBias;
	SoldierXYScale = ArchetypeStats.BodyXYScale;
	EffectiveSkillCooldown = FMath::Max(4.0f, ArchetypeStats.SkillCooldown * (1.0f - (SyncLevel - 1) * 0.06f));
	SkillCooldownRemaining = 0.0f;
	SkillActiveRemaining = 0.0f;
	FacingYaw = FMath::UnwindDegrees(InInitialFacingYaw);
	DesiredFacingYaw = FacingYaw;

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

	RebuildFormation();
	ConformToTerrain(10.0f);

	LegionHP = (float)Soldiers.Num() * FMath::Max(1, SoldierHP);
	AliveCount = Soldiers.Num();
	bWiped = false;
	State = ELegionState::Holding;
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
	for (int32 Idx = 0; Idx < N; ++Idx)
	{
		FLegionSoldier S;
		S.HomeOffset = Offsets[Idx];
		S.Phase = (float)((Idx * 37 + (int32)Team * 11) % 97) / 97.0f * 6.28318f;
		S.JitterYaw = (Hash01(Idx, 3, (int32)Team, (int32)Archetype) - 0.5f) * 2.0f * (SoldierYawJitterDeg + 8.0f);
		S.CurYaw = FacingYaw;

		if (bSkeletalSoldiers && SkeletalBodyMesh)
		{
			S.SkelComp = SpawnSkeletalSoldier(S);
		}
		else
		{
			FTransform BodyXf, HeadXf;
			ComputeSoldierTransforms(S, 0.0f, BodyXf, HeadXf);
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

// ---------------- 命令 ----------------

void ALegionUnit::CommandMoveTo(const FVector& Destination, float OptionalFacingYaw)
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
	State = ELegionState::Moving;
}

void ALegionUnit::CommandAttack(ALegionUnit* Target)
{
	if (bWiped || !Target || Target == this || Target->IsWiped()) { return; }
	AttackTarget = Target;
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	RepathTimer = 0.0f; // 立即触发一次路径计算
	Waypoints.Reset();
	State = ELegionState::Moving;

	// 敌方或空闲兵团受到近距离威胁时才自卫反击 (不抢占玩家现有显式移动指令)
	if (!Target->GetAttackTarget() && Target->GetLegionState() == ELegionState::Holding)
	{
		if (FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation()) <=
			FMath::Square(Target->GetEffectiveEngageRange(this) + 400.0f))
		{
			Target->CommandAttack(this);
		}
	}
}

void ALegionUnit::CommandHalt()
{
	bHasMoveDestination = false;
	bHasCustomFacing = false;
	AttackTarget = nullptr;
	Waypoints.Reset();
	if (!bWiped) { State = ELegionState::Holding; }
}

bool ALegionUnit::TriggerActiveSkill()
{
	if (!IsSkillReady()) { return false; }

	SkillCooldownRemaining = EffectiveSkillCooldown;
	SkillActiveRemaining = ArchetypeStats.SkillDuration;

	if (Archetype == ELegionClass::Pikeman)
	{
		// 拒马突刺: 瞬发前方扇形破阵冲击波
		if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			const TArray<TObjectPtr<ALegionUnit>>& Foes =
				(Team == ELegionTeam::Player) ? GM->GetEnemyLegions() : GM->GetPlayerLegions();
			const FVector Fwd = GetFacingVector();
			for (ALegionUnit* Foe : Foes)
			{
				if (!Foe || Foe->IsWiped()) { continue; }
				const FVector Delta = Foe->GetActorLocation() - GetActorLocation();
				const float Dist = Delta.Size2D();
				if (Dist <= 950.0f && FVector::DotProduct(Fwd, Delta.GetSafeNormal2D()) > 0.1f)
				{
					Foe->ApplyDamage((float)AliveCount * 14.0f, this, false);
					Foe->ApplyKnockback(Delta.GetSafeNormal2D() * 920.0f);
				}
			}
			GM->AddTracerBeam(GetActorLocation() + FVector(0, 0, 90),
				GetActorLocation() + Fwd * 900.0f + FVector(0, 0, 90),
				FLinearColor(0.96f, 0.56f, 0.04f, 1.0f), 0.45f);
		}
	}
	else if (Archetype == ELegionClass::Marksman)
	{
		// 高轨齐射: 立即额外发射一轮高压齐射
		if (ALegionUnit* Target = AttackTarget.Get())
		{
			if (!Target->IsWiped())
			{
				FireRangedVolley(Target);
			}
		}
	}

	UE_LOG(LogERLegion, Log, TEXT("[Legion] %s activated skill [%s]"),
		*Callsign, *ERGalacticMeta::GetClassSkillName(Archetype));
	return true;
}

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

// ---------------- 选中/悬停 ----------------

void ALegionUnit::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected) { return; }
	bSelected = bInSelected;
	UpdateFrameVisual();
}

void ALegionUnit::SetHovered(bool bInHovered)
{
	if (bHovered == bInHovered) { return; }
	bHovered = bInHovered;
	UpdateFrameVisual();
}

void ALegionUnit::UpdateFrameVisual()
{
	if (!FrameMID && FrameTop)
	{
		FrameMID = FrameTop->CreateAndSetMaterialInstanceDynamic(0);
		if (FrameBottom) { FrameBottom->SetMaterial(0, FrameMID); }
		if (FrameLeft)   { FrameLeft->SetMaterial(0, FrameMID); }
		if (FrameRight)  { FrameRight->SetMaterial(0, FrameMID); }
	}
	if (!FrontBarMID && FrontShieldBar)
	{
		FrontBarMID = FrontShieldBar->CreateAndSetMaterialInstanceDynamic(0);
	}

	const FLinearColor TeamCol = (Team == ELegionTeam::Player) ? AllyColor : EnemyColor;
	if (FrameMID)
	{
		FLinearColor C = TeamCol;
		float Gain = bHovered ? 1.45f : 0.70f;
		if (bSelected) { C = SelectedColor; Gain = 1.75f; }
		if (bWiped)    { Gain = 0.08f; }
		FrameMID->SetVectorParameterValue(TEXT("Color"), C * Gain);
	}
	if (FrontBarMID)
	{
		const float FGain = bWiped ? 0.08f : (bSelected ? 2.1f : 1.15f);
		FrontBarMID->SetVectorParameterValue(TEXT("Color"), (bSelected ? SelectedColor : TeamCol) * FGain);
	}
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

		float Block = ArchetypeStats.FrontalBlockRatio;
		if (Archetype == ELegionClass::Shieldbearer && SkillActiveRemaining > 0.0f)
		{
			Block = 0.88f; // 铁壁共振
		}

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
	LegionHP = FMath::Max(0.0f, LegionHP - FinalDamage);
	AliveCount = FMath::Min(PrevAlive, FMath::CeilToInt(LegionHP / (float)FMath::Max(1, SoldierHP)));

	int32 Casualties = PrevAlive - AliveCount;
	while (Casualties-- > 0)
	{
		const FVector FromPos = InInstigator ? InInstigator->GetActorLocation()
			: GetActorLocation() + GetActorQuat().RotateVector(FVector::ForwardVector) * 100.0f;
		const int32 Victim = PickCasualtyIndex(FromPos);
		if (Victim == INDEX_NONE) { break; }
		StartDying(Victim);
	}

	if (AliveCount <= 0 && !bWiped)
	{
		bWiped = true;
		State = ELegionState::Wiped;
		bHasMoveDestination = false;
		AttackTarget = nullptr;
		Waypoints.Reset();
		UpdateFrameVisual();
		OnLegionWiped.Broadcast(this);
		UE_LOG(LogERLegion, Log, TEXT("[Legion] %s wiped"), *Callsign);
	}
	return PrevAlive - AliveCount;
}

void ALegionUnit::ApplyKnockback(const FVector& WorldImpulse)
{
	if (bWiped) { return; }
	// 铁壁共振盾兵免疫击退
	if (Archetype == ELegionClass::Shieldbearer && SkillActiveRemaining > 0.0f) { return; }
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
	S.DieT = 0.0f;
	bSoldierTransformsDirty = true;

	if (bSkeletalSoldiers)
	{
		if (USkeletalMeshComponent* C = S.SkelComp.Get())
		{
			if (AnimDeathPool.Num() > 0)
			{
				C->PlayAnimation(AnimDeathPool[SoldierIndex % AnimDeathPool.Num()], false);
				C->SetPosition(0.0f, false);
			}
			C->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void ALegionUnit::FireRangedVolley(ALegionUnit* Target)
{
	if (!Target || Target->IsWiped() || AliveCount <= 0) { return; }

	const float ElevMul = FLegionTerrainGrid::ComputeElevationMultiplier(GetActorLocation().Z, Target->GetActorLocation().Z);
	const float SkillMul = (SkillActiveRemaining > 0.0f) ? 1.55f : 1.0f;
	const float VolleyDamage = (float)AliveCount * DamagePerSoldierPerSecond * ArchetypeStats.VolleyInterval * ElevMul * SkillMul;

	Target->ApplyDamage(VolleyDamage, this, true);
	if (ArchetypeStats.KnockbackImpulse > 0.0f)
	{
		const FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		Target->ApplyKnockback(Dir * ArchetypeStats.KnockbackImpulse * (SkillActiveRemaining > 0.0f ? 2.4f : 1.0f));
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

// ---------------- Tick ----------------

void ALegionUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bool bNeedTransforms = bSoldierTransformsDirty;

	if (SkillCooldownRemaining > 0.0f)
	{
		SkillCooldownRemaining = FMath::Max(0.0f, SkillCooldownRemaining - DeltaSeconds);
	}
	if (SkillActiveRemaining > 0.0f)
	{
		SkillActiveRemaining = FMath::Max(0.0f, SkillActiveRemaining - DeltaSeconds);
	}

	if (!bWiped)
	{
		// 处理击退惯性位移 + 陡坡跌落惩戒
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
					// 被推下悬崖/绝壁造成震荡伤亡并反弹停住
					ApplyDamage((float)SoldierHP * 1.5f * DeltaSeconds * 10.0f, LastAttacker.Get(), false);
					KnockbackVelocity = FVector::ZeroVector;
				}
			}
		}

		ConformToTerrain(DeltaSeconds);

		// ---- 目标获取与路径追踪 ----
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
				const FVector TargetLoc = Target->GetActorLocation();
				const float DistToEnemy = FVector::Dist2D(GetActorLocation(), TargetLoc);
				const float EffRange = GetEffectiveEngageRange(Target);

				if (DistToEnemy <= EffRange)
				{
					ImmediateGoal = TargetLoc;
				}
				else
				{
					// 追踪移动敌军时周期重算绕山路径
					RepathTimer -= DeltaSeconds;
					if (RepathTimer <= 0.0f || Waypoints.Num() == 0)
					{
						RepathTimer = 0.45f;
						if (TerrainGrid && TerrainGrid->IsBuilt())
						{
							TerrainGrid->FindPath(GetActorLocation(), TargetLoc, Waypoints);
						}
						else
						{
							Waypoints = { TargetLoc };
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
					}
					else
					{
						ImmediateGoal = TargetLoc;
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

		float ActiveSpeedMul = 1.0f;
		float ActiveDpsMul = 1.0f;
		if (Archetype == ELegionClass::Vanguard && SkillActiveRemaining > 0.0f)
		{
			ActiveSpeedMul = 1.85f;
			ActiveDpsMul = 1.55f;
		}

		if (bHasGoal)
		{
			const FVector FinalDest = bAttackGoal ? AttackTarget->GetActorLocation() : MoveDestination;
			const float DistFinal = FVector::Dist2D(GetActorLocation(), FinalDest);
			const float EffRange = bAttackGoal ? GetEffectiveEngageRange(AttackTarget.Get()) : 90.0f;
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
				const FVector Step = ToWp.GetSafeNormal2D() * MoveSpeed * ActiveSpeedMul * DeltaSeconds;
				SetActorLocation(GetActorLocation() + Step);
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

		// 兵团互斥防重叠
		ApplyPeerSeparation(DeltaSeconds);

		// 编队矩形框本身不整体旋转; 仅逻辑朝向 FacingYaw 收敛, 并由每个士兵各自独立转向
		{
			if (State == ELegionState::Holding || State == ELegionState::Engaged)
			{
				FacingYaw = FMath::FixedTurn(FacingYaw, DesiredFacingYaw, FormationTurnSpeed * DeltaSeconds);
			}

			const float ActorYaw = GetActorRotation().Yaw;
			const FQuat ActorQuat = GetActorQuat();
			bool bAnySoldierTurned = false;
			for (int32 i = 0; i < Soldiers.Num(); ++i)
			{
				FLegionSoldier& S = Soldiers[i];
				if (!S.bAlive) { continue; }
				float Want = (State == ELegionState::Holding && bHasCustomFacing) ? DesiredFacingYaw : FacingYaw;
				if (bAttackGoal && AttackTarget.IsValid())
				{
					const FVector SPos = GetActorLocation() + ActorQuat.RotateVector(S.HomeOffset);
					Want = (AttackTarget->GetActorLocation() - SPos).Rotation().Yaw;
				}
				const float PrevYaw = S.CurYaw;
				const float Rate = TurnSpeed * DeltaSeconds * (0.8f + 0.4f * FMath::Frac(S.Phase));
				S.CurYaw = FMath::FixedTurn(S.CurYaw, Want, Rate);
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(PrevYaw, S.CurYaw)) > 0.05f)
				{
					bAnySoldierTurned = true;
				}
				if (bSkeletalSoldiers)
				{
					if (USkeletalMeshComponent* C = S.SkelComp.Get())
					{
						C->SetRelativeRotation(FRotator(0.0f, S.CurYaw - ActorYaw - 90.0f + S.JitterYaw, 0.0f));
					}
				}
			}
			if (bAnySoldierTurned && !bSkeletalSoldiers)
			{
				bNeedTransforms = true;
			}
		}

		// ---- 交战结算 ----
		if (State == ELegionState::Engaged)
		{
			if (ALegionUnit* Target = AttackTarget.Get())
			{
				const float Dist = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
				if (ArchetypeStats.bRanged && Dist > 350.0f)
				{
					const float Interval = (SkillActiveRemaining > 0.0f)
						? ArchetypeStats.VolleyInterval * 0.45f : ArchetypeStats.VolleyInterval;
					VolleyTimer -= DeltaSeconds;
					if (VolleyTimer <= 0.0f)
					{
						VolleyTimer = Interval;
						FireRangedVolley(Target);
					}
				}
				else
				{
					const float ElevMul = FLegionTerrainGrid::ComputeElevationMultiplier(
						GetActorLocation().Z, Target->GetActorLocation().Z);
					float CounterMul = 1.0f;

					// 长戟静止驻守时对近战突击兵造成高额反制与击退
					if (Target->GetArchetype() == ELegionClass::Vanguard && Archetype == ELegionClass::Pikeman)
					{
						CounterMul = ArchetypeStats.BraceAntiMeleeMul;
					}
					// 射手被贴身时近战削弱
					if (ArchetypeStats.bRanged)
					{
						CounterMul *= 0.55f;
					}

					const float Dps = (float)AliveCount * DamagePerSoldierPerSecond * ActiveDpsMul * ElevMul * CounterMul;
					Target->ApplyDamage(Dps * DeltaSeconds, this, false);
				}
			}
			bNeedTransforms = true;
		}
		else if (State == ELegionState::Moving)
		{
			bNeedTransforms = true;
		}
	}

	// ---- 阵亡推进 ----
	bool bAnyDying = false;
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
				EnableSoldierRagdoll(C, i);
			}
			if (S.DieT >= 1.0f && C)
			{
				C->SetComponentTickEnabled(false);
			}
		}
	}
	bNeedTransforms |= bAnyDying;

	if (State != AppliedAnimState)
	{
		AppliedAnimState = State;
		ApplyAnimStateToSoldiers();
	}

	UpdateOrderVisuals(Now);

	if (bNeedTransforms)
	{
		WriteAllSoldierTransforms(Now);
		bSoldierTransformsDirty = bAnyDying;
	}
}

void ALegionUnit::UpdateOrderVisuals(float TimeSeconds)
{
	if (!OrderVisualMID && OrderLine)
	{
		OrderVisualMID = OrderLine->CreateAndSetMaterialInstanceDynamic(0);
		if (MoveMarker) { MoveMarker->SetMaterial(0, OrderVisualMID); }
		if (OrderVisualMID)
		{
			const FLinearColor C = (Team == ELegionTeam::Player) ? SelectedColor : EnemyColor;
			OrderVisualMID->SetVectorParameterValue(TEXT("Color"), C * 1.4f);
		}
	}

	if (OrderLine)
	{
		ALegionUnit* Target = AttackTarget.Get();
		if (Target && !bWiped && !Target->IsWiped())
		{
			const FVector A = GetActorLocation();
			const FVector B = Target->GetActorLocation();
			const float Len = FMath::Max(10.0f, FVector::Dist2D(A, B));
			const float LZ = (A.Z + B.Z) * 0.5f + 42.0f;
			OrderLine->SetWorldLocation(FVector((A.X + B.X) * 0.5f, (A.Y + B.Y) * 0.5f, LZ));
			OrderLine->SetWorldRotation(FRotator(0.0f, (B - A).Rotation().Yaw, 0.0f));
			OrderLine->SetWorldScale3D(FVector(Len / 100.0f, 0.08f, 0.08f));
			OrderLine->SetVisibility(true);
		}
		else
		{
			OrderLine->SetVisibility(false);
		}
	}

	if (MoveMarker)
	{
		if (bHasMoveDestination && !bWiped)
		{
			const float Pulse = 1.0f + 0.12f * FMath::Sin(TimeSeconds * 6.0f);
			float MZ = MoveDestination.Z;
			TraceGroundZ(MoveDestination, MZ);
			MoveMarker->SetWorldLocation(FVector(MoveDestination.X, MoveDestination.Y, MZ + 18.0f));
			MoveMarker->SetWorldRotation(FRotator(0.0f, bHasCustomFacing ? DesiredFacingYaw : 45.0f, 0.0f));
			MoveMarker->SetWorldScale3D(FVector(1.8f, 1.8f, 0.12f) * Pulse);
			MoveMarker->SetVisibility(true);
		}
		else
		{
			MoveMarker->SetVisibility(false);
		}
	}
}

void ALegionUnit::ConformToTerrain(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	const FVector Loc = GetActorLocation();
	float GZ;
	if (TraceGroundZ(Loc, GZ))
	{
		const float NewZ = (DeltaSeconds > 1.0f) ? GZ
			: FMath::FInterpTo(Loc.Z, GZ, DeltaSeconds, GroundConformSpeed * 1.5f);
		SetActorLocation(FVector(Loc.X, Loc.Y, NewZ));
	}

	GroundConformTimer -= DeltaSeconds;
	if (GroundConformTimer > 0.0f) { return; }
	GroundConformTimer = GroundConformInterval;

	const FQuat Q = GetActorQuat();
	const FVector SelfLoc = GetActorLocation();
	bool bChanged = false;
	for (FLegionSoldier& S : Soldiers)
	{
		if (!S.bAlive) { continue; }
		const FVector WPos = SelfLoc + Q.RotateVector(S.HomeOffset);
		float SZ;
		const float Target = TraceGroundZ(WPos, SZ) ? (SZ - SelfLoc.Z) : 0.0f;
		const float NewG = (DeltaSeconds > 1.0f) ? Target
			: FMath::FInterpTo(S.GroundZ, Target, GroundConformInterval, GroundConformSpeed);
		if (FMath::Abs(NewG - S.GroundZ) > 0.5f) { bChanged = true; }
		S.GroundZ = NewG;
		if (bSkeletalSoldiers)
		{
			if (USkeletalMeshComponent* C = S.SkelComp.Get())
			{
				C->SetRelativeLocation(S.HomeOffset + FVector(0.0f, 0.0f, S.GroundZ));
			}
		}
	}
	if (bChanged) { bSoldierTransformsDirty = true; }
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
		OutZ = Hit.Location.Z;
		return true;
	}
	return false;
}

USkeletalMeshComponent* ALegionUnit::SpawnSkeletalSoldier(const FLegionSoldier& S)
{
	USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
	C->SetupAttachment(PickBox);
	C->SetSkeletalMeshAsset(SkeletalBodyMesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetComponentTickEnabled(true);
	C->SetRelativeLocationAndRotation(S.HomeOffset,
		FRotator(0.0f, S.CurYaw - GetActorRotation().Yaw - 90.0f + S.JitterYaw, 0.0f));
	const float OrganicScale = 0.93f + 0.14f * FMath::Frac(S.Phase * 2.13f);
	C->SetRelativeScale3D(FVector(OrganicScale));
	if (SoldierBodyMaterial)
	{
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			C->SetMaterial(i, SoldierBodyMaterial);
		}
	}
	else if (SkeletalTint.R >= 0.0f)
	{
		// 保留 SKM_Manny_Simple 原生材质槽, 经 MID 的 "Paint Tint" 参数统一染出敌我阵营色
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = C->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetVectorParameterValue(TEXT("Paint Tint"), SkeletalTint);
				MID->SetVectorParameterValue(TEXT("Tint"), SkeletalTint);
				MID->SetVectorParameterValue(TEXT("Color"), SkeletalTint);
			}
		}
	}
	C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (AnimIdle)
	{
		C->SetAnimation(AnimIdle);
	}
	C->SetPosition(S.Phase * 0.61f, false);
	C->Play(true);
	C->RegisterComponent();
	return C;
}

void ALegionUnit::ApplyAnimStateToSoldiers()
{
	if (!bSkeletalSoldiers) { return; }
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		USkeletalMeshComponent* C = S.SkelComp.Get();
		if (!C || !S.bAlive) { continue; }
		if (UAnimationAsset* A = PickAnimForState(State, i))
		{
			C->PlayAnimation(A, true);
			C->SetPosition(S.Phase * 1.37f, false);
		}
	}
}

UAnimationAsset* ALegionUnit::PickAnimForState(ELegionState InState, int32 SoldierIndex) const
{
	switch (InState)
	{
	case ELegionState::Moving:
		return AnimMove ? AnimMove.Get() : AnimIdle.Get();
	case ELegionState::Engaged:
		if (AnimAttackPool.Num() > 0)
		{
			return AnimAttackPool[SoldierIndex % AnimAttackPool.Num()].Get();
		}
		return AnimIdle ? AnimIdle.Get() : AnimMove.Get();
	case ELegionState::Holding:
	default:
		return AnimIdle ? AnimIdle.Get() : AnimMove.Get();
	}
}

void ALegionUnit::EnableSoldierRagdoll(USkeletalMeshComponent* C, int32 SoldierIndex)
{
	if (!C || C->IsSimulatingPhysics()) { return; }

	if (USkeletalMesh* Mesh = C->GetSkeletalMeshAsset())
	{
		if (!Mesh->GetPhysicsAsset())
		{
			if (!RagdollPhysicsAsset)
			{
				if (UERAssetCatalog* Catalog = UERAssetCatalog::Get(this))
				{
					RagdollPhysicsAsset = Catalog->LoadAsset<UPhysicsAsset>(
						TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
				}
			}
			if (RagdollPhysicsAsset)
			{
				C->SetPhysicsAsset(RagdollPhysicsAsset);
			}
		}
	}

	C->bBlendPhysics = true;
	C->SetCollisionProfileName(TEXT("Ragdoll"));
	C->SetSimulatePhysics(true);

	FVector Launch(1.0f, 0.0f, 0.0f);
	if (const ALegionUnit* Attacker = LastAttacker.Get())
	{
		Launch = (C->GetComponentLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
		if (Launch.IsNearlyZero()) { Launch = FVector(1.0f, 0.0f, 0.0f); }
	}
	const float Phase = Soldiers.IsValidIndex(SoldierIndex) ? Soldiers[SoldierIndex].Phase : 0.0f;
	const float Side = FMath::Sin(Phase * 3.1f) * 0.45f;
	Launch = (Launch + FVector(0.0f, Side, 0.35f)).GetSafeNormal();
	C->AddImpulse(Launch * 560.0f, NAME_None, true);
}

void ALegionUnit::WriteAllSoldierTransforms(float TimeSeconds)
{
	if (bSkeletalSoldiers) { return; }
	bool bWroteBody = false;
	bool bWroteHead = false;
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		FTransform BodyXf, HeadXf;
		ComputeSoldierTransforms(S, TimeSeconds, BodyXf, HeadXf);
		if (S.InstanceIndex != INDEX_NONE)
		{
			SoldierHISM->UpdateInstanceTransform(S.InstanceIndex, BodyXf, false, false, true);
			bWroteBody = true;
		}
		if (S.ExtraIndex != INDEX_NONE)
		{
			ExtraHISM->UpdateInstanceTransform(S.ExtraIndex, HeadXf, false, false, true);
			bWroteHead = true;
		}
	}
	if (bWroteBody) { SoldierHISM->MarkRenderStateDirty(); }
	if (bWroteHead) { ExtraHISM->MarkRenderStateDirty(); }
}

void ALegionUnit::ComputeSoldierTransforms(const FLegionSoldier& S, float TimeSeconds,
	FTransform& OutBody, FTransform& OutHead) const
{
	FVector Offset = S.HomeOffset + FVector(0.0f, 0.0f, S.GroundZ);
	FVector Shrink = FVector(1.0f);
	const float BaseYaw = S.CurYaw - GetActorRotation().Yaw + S.JitterYaw;
	FRotator Rot = FRotator(0.0f, BaseYaw, 0.0f);

	if (S.DieT >= 0.0f)
	{
		const float T = S.DieT;
		const float K = FMath::Max(0.001f, (1.0f - T) * (1.0f - T));
		Rot = FRotator(85.0f * T, BaseYaw, 0.0f);
		Offset.Z -= 46.0f * T;
		Shrink = FVector(K);
	}
	else if (State == ELegionState::Moving)
	{
		const float W = TimeSeconds * 7.2f + S.Phase;
		Offset.Z += FMath::Abs(FMath::Sin(W)) * MarchBobAmplitude;
		Offset.X += FMath::Sin(W) * 6.0f;
		Rot = FRotator(FMath::Sin(W) * 6.0f, BaseYaw, 0.0f);
	}
	else if (State == ELegionState::Engaged)
	{
		const float W = TimeSeconds * 9.5f + S.Phase;
		FVector DirL = FVector::ForwardVector;
		if (const ALegionUnit* T = AttackTarget.Get())
		{
			DirL = GetActorQuat().UnrotateVector(T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		}
		const float MaxR = FMath::Max(1.0f, FMath::Max(FormationHalfExtent.X, FormationHalfExtent.Y) - 120.0f);
		const float Frontness = FMath::Clamp(
			(S.HomeOffset.X * DirL.X + S.HomeOffset.Y * DirL.Y) / MaxR, 0.0f, 1.0f);
		Offset += DirL * (FMath::Max(0.0f, FMath::Sin(W)) * EngageLungeAmplitude * (0.4f + 0.6f * Frontness));
		Offset.Z += FMath::Abs(FMath::Sin(W * 0.5f)) * MarchBobAmplitude * 0.5f;
		Rot = FRotator(FMath::Sin(W) * 8.0f, BaseYaw, FMath::Sin(W * 0.7f) * 14.0f);
	}

	const float SoldierHeight = 165.0f * SoldierScale;
	float LiftB = 0.0f;
	const float SB = LegionUnitInternal::NormalizeMesh(SoldierHISM->GetStaticMesh(), SoldierHeight, LiftB);

	// 附属层按兵种塑造明显剪影差异 (盾墙 / 长戟 / 刀锋 / 狙击枪)
	FVector GearOffset(48.0f, 0.0f, 95.0f);
	FVector GearScale(0.18f, 0.65f, 1.0f);
	FRotator GearRot = FRotator::ZeroRotator;
	if (Archetype == ELegionClass::Shieldbearer)
	{
		GearOffset = FVector(52.0f, 10.0f, 88.0f);
		GearScale = FVector(0.14f, 0.85f, 1.25f); // 宽大防暴盾牌
	}
	else if (Archetype == ELegionClass::Pikeman)
	{
		GearOffset = FVector(68.0f, 22.0f, 120.0f);
		GearScale = FVector(1.85f, 0.08f, 0.08f); // 前伸长戟
		GearRot = FRotator(-18.0f, 0.0f, 0.0f);
	}
	else if (Archetype == ELegionClass::Vanguard)
	{
		GearOffset = FVector(45.0f, -20.0f, 102.0f);
		GearScale = FVector(0.75f, 0.10f, 0.14f); // 突击战刃
		GearRot = FRotator(12.0f, -15.0f, 0.0f);
	}
	else if (Archetype == ELegionClass::Marksman)
	{
		GearOffset = FVector(56.0f, 8.0f, 112.0f);
		GearScale = FVector(1.05f, 0.10f, 0.12f); // 长管步枪
	}

	const FTransform SoldierFrame(Rot, Offset, Shrink);
	OutBody = SoldierFrame * FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, LiftB),
		FVector(SB * SoldierXYScale, SB * SoldierXYScale, SB));
	OutHead = SoldierFrame * FTransform(GearRot, GearOffset * SoldierScale, GearScale * SoldierScale);
}
