// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/FPS/ShooterProjectile.h"

#include "Experimental/LegionBattle/LegionTypes.h"
#include "Experimental/LegionBattle/LegionUnit.h"
#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

AShooterProjectile::AShooterProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the collision component and assign it as the root
	RootComponent = CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Component"));

	CollisionComponent->SetSphereRadius(16.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	// create the projectile movement component. No need to attach it because it's not a Scene Component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));

	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bShouldBounce = true;

	// set the default damage type
	HitDamageType = UDamageType::StaticClass();
}

void AShooterProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// ignore the pawn that shot this projectile
	CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
}

void AShooterProjectile::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the destruction timer
	GetWorld()->GetTimerManager().ClearTimer(DestructionTimer);
}

void AShooterProjectile::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	// ignore if we've already hit something else
	if (bHit)
	{
		return;
	}

	bHit = true;

	// disable collision on the projectile
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// make AI perception noise
	MakeNoise(NoiseLoudness, GetInstigator(), GetActorLocation(), NoiseRange, NoiseTag);

	if (bExplodeOnHit)
	{
		
		// apply explosion damage centered on the projectile
		ExplosionCheck(GetActorLocation());

	} else {

		// single hit projectile. Process the collided actor
		const FVector TravelDir = (ProjectileMovement && !ProjectileMovement->Velocity.IsNearlyZero())
			? ProjectileMovement->Velocity.GetSafeNormal()
			: -Hit.ImpactNormal;
		ProcessHit(Other, OtherComp, Hit.ImpactPoint, TravelDir, Hit.Item);

	}

	// BP 事件与销毁收尾 (物理命中与数学扫描命中共用)
	FinishHit(Hit);
}

void AShooterProjectile::FinishHit(const FHitResult& Hit)
{
	// pass control to BP for any extra effects
	BP_OnProjectileHit(Hit);

	// check if we should schedule deferred destruction of the projectile
	if (DeferredDestructionTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(DestructionTimer, this, &AShooterProjectile::OnDeferredDestruction, DeferredDestructionTime, false);

	} else {

		// destroy the projectile right away
		Destroy();
	}
}

void AShooterProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bHit) { return; }

	// 逐兵物理命中胶囊已废除 (375 兵空烧 ~3.5ms/帧): 沿本帧飞行线段做数学胶囊扫描,
	// 与士兵子弹同一套胶囊参数/命中结算, 地形遮挡先于士兵判定
	const FVector Cur = GetActorLocation();
	if (bPrevPosValid)
	{
		ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
		if (GM)
		{
			ALegionUnit* HitUnit = nullptr;
			int32 HitSoldier = INDEX_NONE;
			FVector HitPos = FVector::ZeroVector;
			if (GM->ScanProjectileSoldierHit(PrevPosForScan, Cur, ELegionTeam::Player, HitUnit, HitSoldier, HitPos) && HitUnit)
			{
				bHit = true;
				CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				MakeNoise(NoiseLoudness, GetInstigator(), HitPos, NoiseRange, NoiseTag);

				const FVector TravelDir = (Cur - PrevPosForScan).GetSafeNormal();
				if (bExplodeOnHit)
				{
					ExplosionCheck(HitPos);
				}
				else
				{
					HitUnit->ApplyBulletHitToSoldier(HitSoldier, HitDamage, TravelDir);
				}

				FHitResult ScanHit;
				ScanHit.Location = HitPos;
				ScanHit.ImpactPoint = HitPos;
				ScanHit.ImpactNormal = -TravelDir;
				FinishHit(ScanHit);
				return;
			}
		}
	}
	PrevPosForScan = Cur;
	bPrevPosValid = true;
}

void AShooterProjectile::ExplosionCheck(const FVector& ExplosionCenter)
{
	// do a sphere overlap check look for nearby actors to damage
	TArray<FOverlapResult> Overlaps;

	FCollisionShape OverlapShape;
	OverlapShape.SetSphere(ExplosionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (!bDamageOwner)
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}

	GetWorld()->OverlapMultiByObjectType(Overlaps, ExplosionCenter, FQuat::Identity, ObjectParams, OverlapShape, QueryParams);

	TArray<AActor*> DamagedActors;

	// process the overlap results
	for (const FOverlapResult& CurrentOverlap : Overlaps)
	{
		// overlaps may return the same actor multiple times per each component overlapped
		// ensure we only damage each actor once by adding it to a damaged list
		if (DamagedActors.Find(CurrentOverlap.GetActor()) == INDEX_NONE)
		{
			DamagedActors.Add(CurrentOverlap.GetActor());

			// apply physics force away from the explosion
			const FVector& ExplosionDir = CurrentOverlap.GetActor()->GetActorLocation() - GetActorLocation();

			// push and/or damage the overlapped actor
			ProcessHit(CurrentOverlap.GetActor(), CurrentOverlap.GetComponent(), GetActorLocation(), ExplosionDir.GetSafeNormal());
		}

	}

	// 逐兵物理命中代理已废除: 爆炸对敌兵按半径数学结算 (与子弹扫描同一位置源, 射中谁就打谁)
	if (ALegionGameMode* GM = Cast<ALegionGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		GM->ApplyExplosionToEnemySoldiers(ExplosionCenter, ExplosionRadius, HitDamage, ELegionTeam::Player);
	}
}

void AShooterProjectile::ProcessHit(AActor* HitActor, UPrimitiveComponent* HitComp, const FVector& HitLocation, const FVector& HitDirection, int32 HitItem)
{
	// 命中兵团个体代理: 伤害精确结算到被命中的那名敌兵 (射中谁就打/死谁)
	if (ALegionUnit* HitLegion = Cast<ALegionUnit>(HitActor))
	{
		if (HitLegion->GetTeam() == ELegionTeam::Enemy && !HitLegion->IsWiped())
		{
			HitLegion->ApplySoldierProjectileHit(HitComp, HitItem, HitLocation, HitDirection, HitDamage);
		}
		return;
	}

	// have we hit a character?
	if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
	{
		// ignore the owner of this projectile
		if (HitCharacter != GetOwner() || bDamageOwner)
		{
			// apply damage to the character
			UGameplayStatics::ApplyDamage(HitCharacter, HitDamage, GetInstigator()->GetController(), this, HitDamageType);
		}
	}

	// have we hit a physics object?
	if (HitComp->IsSimulatingPhysics())
	{
		// give some physics impulse to the object
		HitComp->AddImpulseAtLocation(HitDirection * PhysicsForce, HitLocation);
	}
}

void AShooterProjectile::OnDeferredDestruction()
{
	// destroy this actor
	Destroy();
}

void AShooterProjectile::SetNoiseTag(const FName& Tag)
{
	NoiseTag = Tag;
}
