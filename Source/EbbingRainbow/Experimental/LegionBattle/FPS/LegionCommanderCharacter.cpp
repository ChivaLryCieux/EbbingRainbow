// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/FPS/LegionCommanderCharacter.h"

#include "Core/ERLog.h"
#include "Experimental/LegionBattle/FPS/ShooterWeapon.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Legion/Terrain/LegionTerrainGrid.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ALegionCommanderCharacter::ALegionCommanderCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule (FP 模板值)
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// Configure character movement: 模板手感 + 兵团战场调参 (速度/台阶/可行走坡度沿用旧指挥官)
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = 3600.0f;
	Move->BrakingDecelerationWalking = 3200.0f;
	Move->BrakingDecelerationFalling = 1500.0f;
	Move->GroundFriction = 9.0f;
	Move->JumpZVelocity = 460.0f;
	Move->AirControl = 0.5f;
	Move->GravityScale = 1.5f;
	Move->MaxStepHeight = 70.0f;
	Move->SetWalkableFloorAngle(58.0f);
	Move->bOrientRotationToMovement = false;
	Move->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	// 默认发放手枪 (BP 可用同名属性覆盖)
	StartingWeaponClasses.Add(TSoftClassPtr<AShooterWeapon>(
		FSoftObjectPath(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol.BP_ShooterWeapon_Pistol_C"))));

	// IA 默认值: BP 序列化值优先, 这里是 C++ 兜底 (独立使用该 C++ 类时也能跑)
	static ConstructorHelpers::FObjectFinder<UInputAction> JumpFinder(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveFinder(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookFinder(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookFinder(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> FireFinder(TEXT("/Game/Variant_Shooter/Input/Actions/IA_Shoot.IA_Shoot"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SwapFinder(TEXT("/Game/Variant_Shooter/Input/Actions/IA_SwapWeapon.IA_SwapWeapon"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SprintFinder(TEXT("/Game/Input/Actions/IA_Sprint.IA_Sprint"));
	if (JumpFinder.Succeeded()) { JumpAction = JumpFinder.Object; }
	if (MoveFinder.Succeeded()) { MoveAction = MoveFinder.Object; }
	if (LookFinder.Succeeded()) { LookAction = LookFinder.Object; }
	if (MouseLookFinder.Succeeded()) { MouseLookAction = MouseLookFinder.Object; }
	if (FireFinder.Succeeded()) { FireAction = FireFinder.Object; }
	if (SwapFinder.Succeeded()) { SwitchWeaponAction = SwapFinder.Object; }
	if (SprintFinder.Succeeded()) { SprintAction = SprintFinder.Object; }
}

void ALegionCommanderCharacter::BeginPlay()
{
	Super::BeginPlay();

	// reset HP to max
	CurrentHP = MaxHP;

	// update the HUD
	OnDamaged.Broadcast(1.0f);

	// 发放出生武器 (模板里靠关卡拾取物, 指挥官直接配枪入场)
	for (const TSoftClassPtr<AShooterWeapon>& WeaponPtr : StartingWeaponClasses)
	{
		if (UClass* WeaponClass = WeaponPtr.LoadSynchronous())
		{
			AddWeaponClass(WeaponClass);
		}
		else
		{
			UE_LOG(LogERLegion, Warning, TEXT("[Legion] Starting weapon failed to load: %s"), *WeaponPtr.ToString());
		}
	}
}

void ALegionCommanderCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void ALegionCommanderCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 把指挥官活动范围钳在寻路网格覆盖的战场内 (沿用旧 LegionCommanderPawn 的逻辑)
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

void ALegionCommanderCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ALegionCommanderCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ALegionCommanderCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::LookInput);

		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ALegionCommanderCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ALegionCommanderCharacter::DoStopFiring);

		// Switch weapon
		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::DoSwitchWeapon);

		// Sprint
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ALegionCommanderCharacter::DoSprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ALegionCommanderCharacter::DoSprintEnd);
	}
	else
	{
		UE_LOG(LogERLegion, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This pawn uses the Enhanced Input system."), *GetNameSafe(this));
	}
}

float ALegionCommanderCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// Reduce HP
	CurrentHP -= Damage;

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// update the HUD
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void ALegionCommanderCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ALegionCommanderCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void ALegionCommanderCharacter::DoAim(float Yaw, float Pitch)
{
	// 军团侧: Alt 呼出光标 / 地图开启时鼠标用于点选, 不转视角 (等价旧 TickGround 的光标守卫)
	if (const ALegionPlayerController* PC = GetController<ALegionPlayerController>())
	{
		if (PC->bShowMouseCursor || PC->IsStrategicMapOpen()) { return; }
	}

	// only route inputs if the character is not dead
	if (GetController() && !IsDead())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ALegionCommanderCharacter::DoMove(float Right, float Forward)
{
	// only route inputs if the character is not dead
	if (GetController() && !IsDead())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ALegionCommanderCharacter::DoJumpStart()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		// pass Jump to the character
		Jump();
	}
}

void ALegionCommanderCharacter::DoJumpEnd()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		// pass StopJumping to the character
		StopJumping();
	}
}

bool ALegionCommanderCharacter::CanFireWeapon() const
{
	const ALegionPlayerController* PC = GetController<ALegionPlayerController>();
	if (!PC)
	{
		// 非军团控制器附身 (编辑器预览等): 不压制
		return true;
	}

	// 战略地图开着 / 光标呼出时, 左键属于点选, 不是开火
	if (PC->IsStrategicMapOpen() || PC->bShowMouseCursor)
	{
		return false;
	}

	return true;
}

void ALegionCommanderCharacter::DoStartFiring()
{
	// fire the current weapon
	if (CurrentWeapon && !IsDead() && CanFireWeapon())
	{
		CurrentWeapon->StartFiring();
	}
}

void ALegionCommanderCharacter::DoStopFiring()
{
	// stop firing the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StopFiring();
	}
}

void ALegionCommanderCharacter::DoSwitchWeapon()
{
	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1 && !IsDead())
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon(PlayerTag);
	}
}

void ALegionCommanderCharacter::SetSprinting(bool bSprint)
{
	GetCharacterMovement()->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
}

void ALegionCommanderCharacter::TestFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFiring();
		CurrentWeapon->StopFiring();
	}
	else
	{
		UE_LOG(LogERLegion, Warning, TEXT("[Legion] TestFire: no weapon equipped"));
	}
}

void ALegionCommanderCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, FirstPersonWeaponSocket);
	
}

void ALegionCommanderCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	// 在第一人称手臂上播放开火蒙太奇 (模板是空 stub, 我们直接播)
	if (Montage && FirstPersonMesh)
	{
		if (UAnimInstance* Anim = FirstPersonMesh->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
}

void ALegionCommanderCharacter::AddWeaponRecoil(float Recoil)
{
	// apply the recoil as pitch input
	AddControllerPitchInput(Recoil);
}

void ALegionCommanderCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector ALegionCommanderCharacter::GetWeaponTargetLocation()
{
	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void ALegionCommanderCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		// spawn the new weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			// add the weapon to the owned list
			OwnedWeapons.Add(AddedWeapon);

			// if we have an existing weapon, deactivate it
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			// switch to the new weapon
			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon(PlayerTag);
		}
	}
}

void ALegionCommanderCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	// set the character mesh AnimInstances
	GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
}

void ALegionCommanderCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// unused
}

void ALegionCommanderCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* ALegionCommanderCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void ALegionCommanderCharacter::Die()
{
	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// grant the death tag to the character
	Tags.Add(DeathTag);
		
	// stop character movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// call the BP handler
	BP_OnDeath();

	// schedule character respawn
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &ALegionCommanderCharacter::OnRespawn, RespawnTime, false);
}

void ALegionCommanderCharacter::OnRespawn()
{
	// destroy the character to force the PC to respawn
	Destroy();
}

bool ALegionCommanderCharacter::IsDead() const
{
	// the character is dead if their current HP drops to zero
	return CurrentHP <= 0.0f;
}

void ALegionCommanderCharacter::SetTeam(uint8 Team)
{
	TeamByte = Team;
}
