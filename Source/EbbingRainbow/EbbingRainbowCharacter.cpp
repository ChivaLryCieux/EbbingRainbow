// Copyright Epic Games, Inc. All Rights Reserved.

#include "EbbingRainbowCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "EbbingRainbow.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "TimerManager.h"
#include "Motorcycle/MotorcyclePawn.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"

struct FAimContext
{
	bool bIsAiming = false;
	TWeakObjectPtr<UStaticMeshComponent> PistolMesh = nullptr;
	FTimerHandle EquipTimerHandle;
};
static TMap<TWeakObjectPtr<const AEbbingRainbowCharacter>, FAimContext> GAimContextMap;

AEbbingRainbowCharacter::AEbbingRainbowCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 550.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	DefaultAirControl = GetCharacterMovement()->AirControl;

	// Create a camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AEbbingRainbowCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsClimbing)
	{
		UpdateClimbingMovement(DeltaTime);
	}
	else
	{
		// 空中运动模式检查
		if (GetCharacterMovement()->IsFalling())
		{
			// 1. 空中且有前向意图时，检测是否贴近可攀爬墙壁
			FHitResult WallHit;
			if (CheckClimbableWall(WallHit))
			{
				// 角色处于空中向前贴墙，自动吸附开始攀爬
				StartClimbing(WallHit);
				return;
			}

			// 2. 缓降中的物理维持
			if (bIsGliding)
			{
				FVector CurrentVel = GetCharacterMovement()->Velocity;

				// 垂直下沉速度平缓限制：减缓下降速度（浮力托举）
				if (CurrentVel.Z < GlideMaxSinkSpeed)
				{
					CurrentVel.Z = FMath::FInterpTo(CurrentVel.Z, GlideMaxSinkSpeed, DeltaTime, 8.0f);
				}

				// 水平方向平移处理：
				// 没有按 WASD 时：不会有平移，只会竖直下降
				// 按了 WASD 时：沿输入方向缓降 + 移动
				const FVector InputVector = GetLastMovementInputVector();
				FVector CurrentHorizontalVel = FVector(CurrentVel.X, CurrentVel.Y, 0.0f);

				if (InputVector.IsNearlyZero(0.01f))
				{
					// 无 WASD 输入：水平速度迅速制动归零，保持纯竖直下降
					CurrentHorizontalVel = FMath::VInterpTo(CurrentHorizontalVel, FVector::ZeroVector, DeltaTime, GlideBrakingFriction);
				}
				else
				{
					// 有 WASD 输入：平滑加速至目标平移速度
					const FVector TargetHorizontalVel = InputVector.GetSafeNormal() * GlideHorizontalMoveSpeed;
					CurrentHorizontalVel = FMath::VInterpTo(CurrentHorizontalVel, TargetHorizontalVel, DeltaTime, 6.0f);
				}

				CurrentVel.X = CurrentHorizontalVel.X;
				CurrentVel.Y = CurrentHorizontalVel.Y;

				GetCharacterMovement()->Velocity = CurrentVel;

				// 贴地检测：接近地面自动收起缓降
				FHitResult GroundHit;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(this);
				const FVector Start = GetActorLocation();
				const FVector End = Start - FVector(0.0f, 0.0f, 40.0f);
				if (GetWorld()->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, Params))
				{
					StopGliding();
				}
			}
		}
	}
}

void AEbbingRainbowCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bIsGliding)
	{
		StopGliding();
	}

	if (bIsClimbing)
	{
		StopClimbing();
	}
}

void AEbbingRainbowCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (PrevMovementMode == MOVE_Falling && !GetCharacterMovement()->IsFalling() && bIsGliding)
	{
		StopGliding();
	}
}

void AEbbingRainbowCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping / Gliding / WallJump
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AEbbingRainbowCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AEbbingRainbowCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEbbingRainbowCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AEbbingRainbowCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEbbingRainbowCharacter::Look);

		// Sprint (按住 Shift 疾跑，松开恢复)
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AEbbingRainbowCharacter::StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &AEbbingRainbowCharacter::StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AEbbingRainbowCharacter::StopSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AEbbingRainbowCharacter::StopSprint);
		}

		// Interact (按 F 交互/骑乘摩托车)
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AEbbingRainbowCharacter::DoInteract);
		}
	}
	else
	{
		UE_LOG(LogEbbingRainbow, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}

	// 原生按键绑定：按 F 触发交互（靠近摩托车按 F 上车）
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AEbbingRainbowCharacter::DoInteract);

	// 原生按键兜底绑定（双保险：确保无论IMC是否配置，按住LeftShift或RightShift即可疾跑，松开即恢复）
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AEbbingRainbowCharacter::StartSprint);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AEbbingRainbowCharacter::StopSprint);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Pressed, this, &AEbbingRainbowCharacter::StartSprint);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Released, this, &AEbbingRainbowCharacter::StopSprint);

	// 原生按键绑定：按住鼠标右键掏枪瞄准，松开收枪取消瞄准
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AEbbingRainbowCharacter::StartAiming);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AEbbingRainbowCharacter::StopAiming);
}

void AEbbingRainbowCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AEbbingRainbowCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AEbbingRainbowCharacter::DoMove(float Right, float Forward)
{
	if (bIsClimbing)
	{
		// 处于攀爬状态：沿当前墙面的切向平面二维移动
		const FVector ClimbUp = FVector::UpVector;
		const FVector ClimbRight = FVector::CrossProduct(WallNormal, FVector::UpVector).GetSafeNormal();

		const FVector DesiredClimbVelocity = (ClimbUp * Forward + ClimbRight * Right) * ClimbSpeed;
		GetCharacterMovement()->Velocity = DesiredClimbVelocity;
		return;
	}

	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AEbbingRainbowCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AEbbingRainbowCharacter::DoJumpStart()
{
	// 1. 如果处于爬墙状态：按空格触发向后蹬墙跳
	if (bIsClimbing)
	{
		PerformWallJump();
		return;
	}

	// 2. 如果处于空中下落状态：点按空格进入缓降，再次点按空格退出变回自由落体
	if (GetCharacterMovement()->IsFalling())
	{
		if (bIsGliding)
		{
			StopGliding(); // 再次点按空格：退出缓降，变回自由落体
			return;
		}
		else if (CanGlide())
		{
			StartGliding(); // 点按空格：进入缓降状态，减缓下降速度
			return;
		}
	}

	// 3. 正常地面起跳
	Jump();
}

void AEbbingRainbowCharacter::DoJumpEnd()
{
	StopJumping();
}

void AEbbingRainbowCharacter::DoInteract()
{
	// 查找并尝试骑乘附近可交互的摩托车
	if (AMotorcyclePawn* NearbyMotorcycle = FindNearbyMotorcycle(280.0f))
	{
		NearbyMotorcycle->MountRider(this);
	}
}

AMotorcyclePawn* AEbbingRainbowCharacter::FindNearbyMotorcycle(float SearchRadius) const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	const FVector MyLoc = GetActorLocation();
	AMotorcyclePawn* BestMotorcycle = nullptr;
	float ClosestDistSq = MAX_flt;

	// 1. 优先通过空间重叠通道探测摩托车
	TArray<FOverlapResult> Overlaps;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRadius);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FindNearbyMotorcycle), false, this);

	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		MyLoc,
		FQuat::Identity,
		ECC_Pawn,
		SphereShape,
		QueryParams
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AMotorcyclePawn* Moto = Cast<AMotorcyclePawn>(Overlap.GetActor()))
		{
			if (Moto->CanMount(const_cast<AEbbingRainbowCharacter*>(this)))
			{
				float DistSq = FVector::DistSquared(MyLoc, Moto->GetActorLocation());
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					BestMotorcycle = Moto;
				}
			}
		}
	}

	// 2. 场景 Actor 双保险遍历检测 (确保任何通道设置下均可准确识别)
	if (!BestMotorcycle)
	{
		const float SearchRadiusSq = FMath::Square(SearchRadius);
		for (TActorIterator<AMotorcyclePawn> It(GetWorld()); It; ++It)
		{
			AMotorcyclePawn* Moto = *It;
			if (Moto && Moto->CanMount(const_cast<AEbbingRainbowCharacter*>(this)))
			{
				float DistSq = FVector::DistSquared(MyLoc, Moto->GetActorLocation());
				if (DistSq <= SearchRadiusSq && DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					BestMotorcycle = Moto;
				}
			}
		}
	}

	return BestMotorcycle;
}

// -------------------------------------------------------------
// 疾跑 (Hold to Sprint)
// -------------------------------------------------------------
void AEbbingRainbowCharacter::StartSprint()
{
	if (!bIsSprinting)
	{
		bIsSprinting = true;
		if (!bIsClimbing && !bIsGliding)
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1001, 1.5f, FColor::Green, FString::Printf(TEXT("[Shift Active] Sprinting -> Speed: %.0f"), GetCharacterMovement()->MaxWalkSpeed));
		}
	}
}

void AEbbingRainbowCharacter::StopSprint()
{
	if (bIsSprinting)
	{
		bIsSprinting = false;
		if (!bIsClimbing && !bIsGliding)
		{
			GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1001, 1.5f, FColor::Yellow, FString::Printf(TEXT("[Shift Released] Normal -> Speed: %.0f"), GetCharacterMovement()->MaxWalkSpeed));
		}
	}
}

// -------------------------------------------------------------
// 滑翔 (Gliding)
// -------------------------------------------------------------
bool AEbbingRainbowCharacter::CanGlide() const
{
	if (!GetCharacterMovement()->IsFalling() || bIsClimbing)
	{
		return false;
	}

	// 离地高度检查
	FHitResult GroundHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.0f, 0.0f, MinGlideHeightAboveGround);

	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, Params))
	{
		return false; // 离地过近，不触发滑翔
	}

	return true;
}

void AEbbingRainbowCharacter::StartGliding()
{
	if (bIsGliding) return;

	if (IsAiming())
	{
		StopAiming();
	}

	bIsGliding = true;
	GetCharacterMovement()->GravityScale = GlideGravityScale;
	GetCharacterMovement()->AirControl = GlideAirControl;

	// 展开缓降瞬间，如果正在快速下坠，将垂直速度平稳缓冲至 GlideMaxSinkSpeed
	FVector CurrentVel = GetCharacterMovement()->Velocity;
	if (CurrentVel.Z < GlideMaxSinkSpeed)
	{
		CurrentVel.Z = GlideMaxSinkSpeed;
	}
	GetCharacterMovement()->Velocity = CurrentVel;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1002, 2.0f, FColor::Cyan, TEXT("[Glide Active] Slow Fall - Space to Drop, WASD to Move"));
	}
}

void AEbbingRainbowCharacter::StopGliding()
{
	if (!bIsGliding) return;

	bIsGliding = false;
	GetCharacterMovement()->GravityScale = DefaultGravityScale;
	GetCharacterMovement()->AirControl = DefaultAirControl;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1002, 1.5f, FColor::Orange, TEXT("[Glide Cancelled] Free Falling"));
	}
}

// -------------------------------------------------------------
// 爬墙 (Climbing)
// -------------------------------------------------------------
bool AEbbingRainbowCharacter::CheckClimbableWall(FHitResult& OutHit)
{
	const FVector Start = GetActorLocation();
	const FVector ForwardVector = GetActorForwardVector();
	const FVector End = Start + ForwardVector * ClimbTraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(ClimbTraceRadius);
	const bool bHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldStatic, SphereShape, Params);

	if (bHit)
	{
		// 判定表面法线：垂直墙体法线 Z 接近 0
		const float AbsZ = FMath::Abs(OutHit.ImpactNormal.Z);
		if (AbsZ < 0.25f)
		{
			return true;
		}
	}

	return false;
}

void AEbbingRainbowCharacter::StartClimbing(const FHitResult& WallHit)
{
	if (bIsClimbing) return;

	if (IsAiming())
	{
		StopAiming();
	}

	if (bIsGliding)
	{
		StopGliding();
	}

	bIsClimbing = true;
	WallNormal = WallHit.ImpactNormal;
	WallLocation = WallHit.ImpactPoint;

	// 切换为 Flying 模式实现沿墙面任意方向攀爬且不受重力下坠
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	// 角色朝向正对墙面
	const FRotator TargetRot = (-WallNormal).Rotation();
	SetActorRotation(FRotator(0.0f, TargetRot.Yaw, 0.0f));
}

void AEbbingRainbowCharacter::StopClimbing()
{
	if (!bIsClimbing) return;

	bIsClimbing = false;
	WallNormal = FVector::ZeroVector;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}

void AEbbingRainbowCharacter::PerformWallJump()
{
	const FVector JumpDirection = WallNormal;
	StopClimbing();

	const FVector LaunchVelocity = JumpDirection * WallJumpHorizontalImpulse + FVector::UpVector * WallJumpVerticalImpulse;
	LaunchCharacter(LaunchVelocity, true, true);

	// 转身背对原墙面
	SetActorRotation(JumpDirection.Rotation());
}

void AEbbingRainbowCharacter::UpdateClimbingMovement(float DeltaTime)
{
	// 持续向前方检测墙壁，确保没有脱离墙壁
	FHitResult Hit;
	const FVector Start = GetActorLocation();
	const FVector End = Start - WallNormal * (WallOffset + 20.0f);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		// 更新法线与位置
		WallNormal = Hit.ImpactNormal;
		WallLocation = Hit.ImpactPoint;

		// 约束角色贴合墙面间距
		const FVector DesiredLocation = WallLocation + WallNormal * WallOffset;
		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), FVector(DesiredLocation.X, DesiredLocation.Y, GetActorLocation().Z), DeltaTime, 10.0f);
		SetActorLocation(NewLocation, true);

		// 约束角色面朝墙壁
		const FRotator TargetRot = (-WallNormal).Rotation();
		SetActorRotation(FRotator(0.0f, TargetRot.Yaw, 0.0f));

		// 检查登顶（Ledge Mantle 检测）：头顶上方无碰撞且上方前向下方有水平地面
		const FVector HeadStart = GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
		const FVector HeadForward = HeadStart - WallNormal * (WallOffset + 30.0f);
		FHitResult HeadHit;
		if (!GetWorld()->LineTraceSingleByChannel(HeadHit, HeadStart, HeadForward, ECC_WorldStatic, Params))
		{
			// 前方头部无阻挡，向下探测是否有平台
			const FVector LedgeDownEnd = HeadForward - FVector(0.0f, 0.0f, 60.0f);
			FHitResult LedgeHit;
			if (GetWorld()->LineTraceSingleByChannel(LedgeHit, HeadForward, LedgeDownEnd, ECC_WorldStatic, Params))
			{
				if (LedgeHit.ImpactNormal.Z > 0.7f)
				{
					// 到达墙顶水平平台，登顶翻越
					SetActorLocation(LedgeHit.ImpactPoint + FVector(0.0f, 0.0f, 96.0f));
					StopClimbing();
					GetCharacterMovement()->SetMovementMode(MOVE_Walking);
					return;
				}
			}
		}
	}
	else
	{
		// 失去墙体接触，脱离攀爬
		StopClimbing();
	}
}

EOpenWorldMovementMode AEbbingRainbowCharacter::GetOpenWorldMovementMode() const
{
	if (bIsClimbing) return EOpenWorldMovementMode::Climbing;
	if (bIsGliding) return EOpenWorldMovementMode::Gliding;
	if (GetCharacterMovement()->IsFalling()) return EOpenWorldMovementMode::Falling;
	if (bIsSprinting) return EOpenWorldMovementMode::Sprinting;
	return EOpenWorldMovementMode::Walking;
}

// -------------------------------------------------------------
// 持枪与瞄准 (Pistol Aiming)
// -------------------------------------------------------------
void AEbbingRainbowCharacter::StartAiming()
{
	if (bIsClimbing || bIsGliding)
	{
		return;
	}

	if (!bIsAiming)
	{
		bIsAiming = true;

		// 瞄准行走移动速度（保持角色原本朝向独立）
		GetCharacterMovement()->MaxWalkSpeed = 350.0f;

		// 确保手枪道具已创建并挂载到右手
		FAimContext& Context = GAimContextMap.FindOrAdd(this);
		if (!Context.PistolMesh.IsValid())
		{
			UStaticMeshComponent* PistolComp = NewObject<UStaticMeshComponent>(this, TEXT("PistolPropMesh"));
			if (PistolComp)
			{
				PistolComp->RegisterComponent();
				PistolComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
				
				UStaticMesh* BaseMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
				if (BaseMesh)
				{
					PistolComp->SetStaticMesh(BaseMesh);
					PistolComp->SetRelativeLocation(FVector(10.0f, 4.0f, -2.0f));
					PistolComp->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
					PistolComp->SetRelativeScale3D(FVector(0.18f, 0.05f, 0.12f));
				}
				PistolComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Context.PistolMesh = PistolComp;
			}
		}

		if (Context.PistolMesh.IsValid())
		{
			Context.PistolMesh->SetVisibility(true);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1003, 2.0f, FColor::Emerald, TEXT("[Aiming Active] BS_Pistol_Locomotion Engaged"));
		}
	}
}

void AEbbingRainbowCharacter::StopAiming()
{
	if (bIsAiming)
	{
		bIsAiming = false;

		// 恢复常规移动速度
		GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : JogSpeed;

		// 收枪：隐藏右手手枪模型
		FAimContext* ContextPtr = GAimContextMap.Find(this);
		if (ContextPtr && ContextPtr->PistolMesh.IsValid())
		{
			ContextPtr->PistolMesh->SetVisibility(false);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1003, 1.5f, FColor::Orange, TEXT("[Aiming Ended] Unarmed Restored"));
		}
	}
}
