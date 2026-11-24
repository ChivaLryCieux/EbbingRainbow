// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionCommanderPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"

ALegionCommanderPawn::ALegionCommanderPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = 3600.0f;
	Move->BrakingDecelerationWalking = 3200.0f;
	Move->GroundFriction = 9.0f;
	Move->JumpZVelocity = 460.0f;
	Move->AirControl = 0.35f;
	Move->GravityScale = 1.5f;
	Move->MaxStepHeight = 70.0f;
	Move->SetWalkableFloorAngle(58.0f);
	Move->bOrientRotationToMovement = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, EyeOffsetZ));
	Camera->bUsePawnControlRotation = true;
	Camera->FieldOfView = 88.0f;

	if (USkeletalMeshComponent* M = GetMesh())
	{
		M->SetVisibility(false);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ALegionCommanderPawn::SetSprinting(bool bSprint)
{
	GetCharacterMovement()->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
}

void ALegionCommanderPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ALegionGameMode* GM = GetWorld() ? Cast<ALegionGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!GM || !GM->GetTerrainGrid().IsBuilt()) { return; }

	const FLegionTerrainGrid& Grid = GM->GetTerrainGrid();
	const FVector C = Grid.GetGridCenter();
	const float Half = Grid.GetWorldSpan() * 0.5f - BoundsMargin;

	FVector P = GetActorLocation();
	const FVector Clamped(FMath::Clamp(P.X, C.X - Half, C.X + Half), FMath::Clamp(P.Y, C.Y - Half, C.Y + Half), P.Z);
	if (!Clamped.Equals(P, 1.0f))
	{
		SetActorLocation(Clamped, false, nullptr, ETeleportType::TeleportPhysics);
		GetCharacterMovement()->Velocity.X = 0.0f;
		GetCharacterMovement()->Velocity.Y = 0.0f;
	}
}
