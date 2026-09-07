// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotorcyclePawn.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

AMotorcyclePawn::AMotorcyclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 根组件：物理碰撞盒 (预留充裕悬挂离地间隙，杜绝磕碰小石头)
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->InitBoxExtent(FVector(75.0f, 22.0f, 28.0f));
	CollisionBox->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CollisionBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // 忽略相机通道，防止镜头顶在盒体内
	CollisionBox->SetSimulatePhysics(false); // 采用平滑扫掠物理模拟，杜绝空中卡死与穿模
	RootComponent = CollisionBox;

	// 2. 车身主体网格体
	ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChassisMesh"));
	ChassisMesh->SetupAttachment(RootComponent);
	ChassisMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	ChassisMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChassisMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 3. 车把/前叉转向轴
	SteeringPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SteeringPivot"));
	SteeringPivot->SetupAttachment(ChassisMesh);
	SteeringPivot->SetRelativeLocation(FVector(55.0f, 0.0f, 15.0f));

	// 4. 车把网格体
	HandlebarsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandlebarsMesh"));
	HandlebarsMesh->SetupAttachment(SteeringPivot);
	HandlebarsMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
	HandlebarsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandlebarsMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 5. 前轮网格体 (随转向偏转)
	FrontWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontWheelMesh"));
	FrontWheelMesh->SetupAttachment(SteeringPivot);
	FrontWheelMesh->SetRelativeLocation(FVector(20.0f, 0.0f, -25.0f));
	FrontWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrontWheelMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 6. 后轮网格体
	RearWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearWheelMesh"));
	RearWheelMesh->SetupAttachment(ChassisMesh);
	RearWheelMesh->SetRelativeLocation(FVector(-65.0f, 0.0f, -10.0f));
	RearWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RearWheelMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 7. 骑手座位锚点 (位于车身座鞍位置)
	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(ChassisMesh);
	SeatPoint->SetRelativeLocation(FVector(-15.0f, 0.0f, 32.0f));

	// 8. 左/右侧下车预设落脚点
	DismountPointLeft = CreateDefaultSubobject<USceneComponent>(TEXT("DismountPointLeft"));
	DismountPointLeft->SetupAttachment(RootComponent);
	DismountPointLeft->SetRelativeLocation(FVector(-10.0f, -85.0f, 0.0f));

	DismountPointRight = CreateDefaultSubobject<USceneComponent>(TEXT("DismountPointRight"));
	DismountPointRight->SetupAttachment(RootComponent);
	DismountPointRight->SetRelativeLocation(FVector(-10.0f, 85.0f, 0.0f));

	// 9. 玩家接近感应球
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(220.0f);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 10. 载具跟随摄像机
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f)); // 抬高悬臂支点至骑手胸前高度
	CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 65.0f);        // 视线焦点处于骑手肩头上方
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 25.0f);
	CameraBoom->TargetArmLength = BaseCameraDistance;             // 默认 700.0f
	CameraBoom->bDoCollisionTest = bEnableCameraCollision;        // 默认 false，彻底杜绝镜头被骑手或车身卡扁
	CameraBoom->ProbeSize = 10.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false; // 相机不随车身横滚晃荡，保障舒适视线

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 11. 前照大灯
	Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
	Headlight->SetupAttachment(SteeringPivot);
	Headlight->SetRelativeLocation(FVector(15.0f, 0.0f, 15.0f));
	Headlight->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));
	Headlight->SetIntensity(6000.0f);
	Headlight->SetOuterConeAngle(38.0f);

	// 默认基础几何体装配 (开箱即见摩托车雏形，蓝图中可随意替换高模)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	if (CubeMeshFinder.Succeeded())
	{
		ChassisMesh->SetStaticMesh(CubeMeshFinder.Object);
		ChassisMesh->SetRelativeScale3D(FVector(1.4f, 0.35f, 0.45f));
	}
	if (CylinderMeshFinder.Succeeded())
	{
		FrontWheelMesh->SetStaticMesh(CylinderMeshFinder.Object);
		FrontWheelMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.18f));
		FrontWheelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		RearWheelMesh->SetStaticMesh(CylinderMeshFinder.Object);
		RearWheelMesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.22f));
		RearWheelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		HandlebarsMesh->SetStaticMesh(CylinderMeshFinder.Object);
		HandlebarsMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.75f));
		HandlebarsMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	}
}

void AMotorcyclePawn::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionBox)
	{
		// 动态重置碰撞盒尺寸
		CollisionBox->SetBoxExtent(FVector(75.0f, 22.0f, 28.0f));
	}

	if (CameraBoom)
	{
		CameraBoom->bDoCollisionTest = bEnableCameraCollision;
		CameraBoom->TargetArmLength = BaseCameraDistance;
	}

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AMotorcyclePawn::OnInteractionSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AMotorcyclePawn::OnInteractionSphereEndOverlap);
}

void AMotorcyclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 若处于被操控状态，更新按键与鼠标视角
	if (IsOccupied())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// 原生按键输入采集 (WASD / 方向键)
			const bool bForward = PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up);
			const bool bBackward = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down);
			if (bForward || bBackward)
			{
				ThrottleInput = (bForward ? 1.0f : 0.0f) - (bBackward ? 1.0f : 0.0f);
			}
			else if (!ThrottleAction)
			{
				ThrottleInput = 0.0f;
			}

			const bool bTurnRight = PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right);
			const bool bTurnLeft = PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left);
			if (bTurnRight || bTurnLeft)
			{
				SteerInput = (bTurnRight ? 1.0f : 0.0f) - (bTurnLeft ? 1.0f : 0.0f);
			}
			else if (!SteerAction)
			{
				SteerInput = 0.0f;
			}

			if (PC->IsInputKeyDown(EKeys::SpaceBar))
			{
				bHandbrake = true;
			}
			else if (!HandbrakeAction)
			{
				bHandbrake = false;
			}

			// 鼠标平滑转动视角
			float MouseX = 0.0f, MouseY = 0.0f;
			PC->GetInputMouseDelta(MouseX, MouseY);
			if (!FMath::IsNearlyZero(MouseX) || !FMath::IsNearlyZero(MouseY))
			{
				AddControllerYawInput(MouseX);
				AddControllerPitchInput(-MouseY);
			}
		}
	}

	// 物理与运动模拟更新
	SimulateMotorcyclePhysics(DeltaTime);
}

void AMotorcyclePawn::SimulateMotorcyclePhysics(float DeltaTime)
{
	// 1. 油门、刹车与自然滑行减速计算
	if (!bIsAirborne)
	{
		float TargetSpeed = 0.0f;
		if (bHandbrake)
		{
			// 手刹制动
			CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, HandbrakeDeceleration);
		}
		else if (FMath::Abs(ThrottleInput) > 0.01f)
		{
			if (ThrottleInput > 0.0f)
			{
				// 向前加速
				TargetSpeed = ThrottleInput * MaxForwardSpeed;
				if (CurrentSpeed < -5.0f)
				{
					// 倒车中踩前油门 -> 先刹停
					CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, BrakingDeceleration);
				}
				else
				{
					CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, (AccelerationRate / MaxForwardSpeed) * 3.5f);
				}
			}
			else
			{
				// 倒车或前行踩刹车
				if (CurrentSpeed > 50.0f)
				{
					// 前进中踩后油门 -> 强力刹车
					CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, BrakingDeceleration);
				}
				else
				{
					// 静止或低速 -> 倒车行驶
					TargetSpeed = ThrottleInput * MaxReverseSpeed;
					CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, (AccelerationRate / MaxReverseSpeed) * 2.5f);
				}
			}
		}
		else
		{
			// 无油门：自然地面滚动阻力与风阻滑行
			CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, CoastingDeceleration);
		}
	}
	else
	{
		// 滞空空中状态：风阻自然减速，手刹可轻微空中制动
		if (bHandbrake)
		{
			CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, BrakingDeceleration * 0.4f);
		}
		else
		{
			CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, AirDragDeceleration);
		}
	}

	// 2. 地面悬挂射线探测 (前后轮双点探地)
	const FVector ActorLoc = GetActorLocation();
	const FVector Forward = GetActorForwardVector();

	// 探针从车身与车轮上方 50cm 处向下探射，即便颠簸或下坡也能从上方稳定探地
	const FVector FrontWheelWorldLoc = ActorLoc + Forward * 70.0f + FVector(0.0f, 0.0f, 50.0f);
	const FVector RearWheelWorldLoc = ActorLoc - Forward * 65.0f + FVector(0.0f, 0.0f, 50.0f);

	FHitResult FrontHit, RearHit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MotorcycleSuspension), false, this);
	if (CurrentRider.IsValid())
	{
		TraceParams.AddIgnoredActor(CurrentRider.Get());
	}

	const float TraceDistance = 160.0f; // 向上 50cm + 向下 110cm，覆盖安全稳定的悬挂检测区间
	const bool bFrontHit = GetWorld()->LineTraceSingleByChannel(FrontHit, FrontWheelWorldLoc, FrontWheelWorldLoc - FVector(0, 0, TraceDistance), ECC_Visibility, TraceParams);
	const bool bRearHit = GetWorld()->LineTraceSingleByChannel(RearHit, RearWheelWorldLoc, RearWheelWorldLoc - FVector(0, 0, TraceDistance), ECC_Visibility, TraceParams);

	// 计算前后轮到地面的垂向距离 (相对车身中心高差)
	const float FrontGroundDist = bFrontHit ? (ActorLoc.Z - FrontHit.ImpactPoint.Z) : 9999.0f;
	const float RearGroundDist = bRearHit ? (ActorLoc.Z - RearHit.ImpactPoint.Z) : 9999.0f;

	// 悬挂最大有效下行程边界: RideHeight + MaxSuspensionExtension
	const float MaxEffectiveGroundDist = RideHeight + MaxSuspensionExtension;
	const bool bFrontGrounded = bFrontHit && (FrontGroundDist <= MaxEffectiveGroundDist);
	const bool bRearGrounded = bRearHit && (RearGroundDist <= MaxEffectiveGroundDist);

	FRotator CurrentRot = GetActorRotation();

	// 计算当前沿坡度向上的爬坡垂直分速度 (cm/s)
	// 当车头上扬且前进时，摩托车拥有向上的真实物理动量
	const float GroundSlopeClimbVel = (CurrentSpeed > 0.0f) ? (CurrentSpeed * FMath::Sin(FMath::DegreesToRadians(CurrentRot.Pitch))) : 0.0f;

	// 3. 冲坡腾空与离地判定
	if (!bIsAirborne)
	{
		// 检查是否冲坡腾空 (Crest Launch / Ramp Jump)
		bool bCrestLaunch = false;
		if (GroundSlopeClimbVel >= MinLaunchClimbSpeed && CurrentSpeed > 350.0f)
		{
			// 若前轮悬挂被拉伸超过 RideHeight + 6cm，或者两轮支撑坡度比车身俯仰角骤降 6 度以上 (冲过飞包坡顶)
			if (!bFrontGrounded || FrontGroundDist > (RideHeight + 6.0f))
			{
				bCrestLaunch = true;
			}
			else if (bFrontGrounded && bRearGrounded)
			{
				const FVector ContactLine = FrontHit.ImpactPoint - RearHit.ImpactPoint;
				const float GroundPitch = FMath::RadiansToDegrees(FMath::Atan2(ContactLine.Z, ContactLine.Size2D()));
				if (GroundPitch < CurrentRot.Pitch - 6.0f)
				{
					bCrestLaunch = true;
				}
			}
		}

		// 飞出悬崖/台阶断层 (Cliff / Drop-off)
		const bool bCliffDrop = (!bFrontGrounded && !bRearGrounded);

		if (bCrestLaunch || bCliffDrop)
		{
			bIsAirborne = true;
			bIsOnGround = false;
			AirborneDuration = 0.0f;

			// 冲坡发射初速度：保留真实垂直动量，并乘以 JumpLaunchMultiplier 获得具有视觉冲击力的飞跃高度
			if (bCrestLaunch)
			{
				VerticalVelocity = GroundSlopeClimbVel * JumpLaunchMultiplier;
			}
			else
			{
				VerticalVelocity = GroundSlopeClimbVel;
			}
		}
	}

	// 4. 转向角计算与车把偏转
	const float SpeedRatio = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxForwardSpeed, 0.0f, 1.0f);
	const float CurrentMaxSteer = FMath::Lerp(MaxSteerAngle, HighSpeedSteerAngle, SpeedRatio);
	const float DesiredSteerAngle = SteerInput * CurrentMaxSteer;

	// 5. 分支物理动力学模拟 (空中 vs 地面)
	if (bIsAirborne)
	{
		AirborneDuration += DeltaTime;
		bIsOnGround = false;

		// 5.1 施加真实重力
		VerticalVelocity += GravityZ * DeltaTime;

		// 5.2 空中偏航转向微调 (衰减转向控制权，防空中旋转失控)
		if (FMath::Abs(SteerInput) > 0.01f)
		{
			const float YawDelta = SteerInput * TurnRate * AirControlRatio * DeltaTime;
			CurrentRot.Yaw += YawDelta;

			// 同步旋转玩家控制器与摄像机视角
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				FRotator ControlRot = PC->GetControlRotation();
				ControlRot.Yaw += YawDelta;
				PC->SetControlRotation(ControlRot);
			}
		}

		// 5.3 空中压弯姿态自适应回正 (Gyro Self-Leveling)
		CurrentLeanRoll = FMath::FInterpTo(CurrentLeanRoll, 0.0f, DeltaTime, 4.0f);

		// 5.4 空中车身俯仰对齐飞行抛物线切线 (Motocross Jump Arc)
		const float FlightTrajectoryPitch = FMath::RadiansToDegrees(FMath::Atan2(VerticalVelocity, FMath::Max(FMath::Abs(CurrentSpeed), 200.0f)));
		float AirPitchControl = 0.0f;
		if (FMath::Abs(ThrottleInput) > 0.05f)
		{
			AirPitchControl = ThrottleInput * AirPitchControlRate;
		}
		CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, FlightTrajectoryPitch + AirPitchControl * 0.2f, DeltaTime, AirPitchInterpSpeed);
		CurrentRot.Roll = 0.0f;
		SetActorRotation(CurrentRot);

		// 5.5 空中 3D 抛物线扫掠移动 (解耦水平前进与垂直升降)
		const FVector HorizontalMove = GetActorForwardVector().GetSafeNormal2D() * CurrentSpeed * DeltaTime;
		const FVector VerticalMove = FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime);
		const FVector AirDelta = HorizontalMove + VerticalMove;

		FHitResult AirMoveHit;
		AddActorWorldOffset(AirDelta, true, &AirMoveHit);

		// 5.6 着陆检测 (Touchdown Detection)
		// 仅当下落阶段 (VerticalVelocity <= 50.0f) 且车轮进入悬挂受力区间，或扫掠碰撞到底部地面时着陆
		bool bShouldLand = false;
		if (VerticalVelocity <= 50.0f)
		{
			if ((bFrontHit && FrontGroundDist <= (RideHeight + 6.0f)) ||
				(bRearHit && RearGroundDist <= (RideHeight + 6.0f)))
			{
				bShouldLand = true;
			}
			else if (AirMoveHit.bBlockingHit && AirMoveHit.ImpactNormal.Z > 0.5f)
			{
				bShouldLand = true;
			}
		}

		if (bShouldLand)
		{
			bIsAirborne = false;
			bIsOnGround = true;
			const float ImpactSpeed = FMath::Abs(VerticalVelocity);
			VerticalVelocity = 0.0f;

			// 着陆姿态贴合地面
			if (bFrontHit && bRearHit)
			{
				const FVector ContactLine = FrontHit.ImpactPoint - RearHit.ImpactPoint;
				const float LandPitch = FMath::RadiansToDegrees(FMath::Atan2(ContactLine.Z, ContactLine.Size2D()));
				CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, LandPitch, DeltaTime, 12.0f);
				SetActorRotation(CurrentRot);
			}

			// 触发落地广播事件
			OnMotorcycleLanded.Broadcast(AirborneDuration, ImpactSpeed);
			AirborneDuration = 0.0f;
		}
	}
	else
	{
		bIsOnGround = (bFrontGrounded || bRearGrounded);

		// 地面偏航转向角速度计算
		// 低速时赋予充沛的初始转向能力 (保底 0.45 转向权威度)，即便静止或慢速也能原地打方向/掉头
		const float TurnAuthority = FMath::Clamp(FMath::Abs(CurrentSpeed) / 250.0f, 0.45f, 1.0f);
		if (FMath::Abs(SteerInput) > 0.01f)
		{
			// 前进或静止正向偏航，倒车反向偏航
			const float DirectionSign = (CurrentSpeed < -10.0f) ? -1.0f : 1.0f;
			const float YawDelta = SteerInput * TurnRate * TurnAuthority * DirectionSign * DeltaTime;
			CurrentRot.Yaw += YawDelta;

			// 同步旋转摄像机/控制器视角，使视角自然跟随车头偏转
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				FRotator ControlRot = PC->GetControlRotation();
				ControlRot.Yaw += YawDelta;
				PC->SetControlRotation(ControlRot);
			}
		}

		// 入弯压弯倾斜 (Motorcycle Lean: 右转右倾，左转左倾)
		const float TargetLean = SteerInput * MaxLeanAngle * SpeedRatio;
		CurrentLeanRoll = FMath::FInterpTo(CurrentLeanRoll, TargetLean, DeltaTime, LeanInterpSpeed);

		// 坡度贴合与悬挂高度修正
		if (bFrontGrounded && bRearGrounded)
		{
			// 沿前后两轮落地点连接线平滑拟合车辆 Pitch 俯仰角
			const FVector ContactLine = FrontHit.ImpactPoint - RearHit.ImpactPoint;
			const float DesiredPitch = FMath::RadiansToDegrees(FMath::Atan2(ContactLine.Z, ContactLine.Size2D()));
			CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, DesiredPitch, DeltaTime, 14.0f);

			// 离地悬挂高度稳定 (带 sweep 扫掠，防止物理穿插)
			const float DesiredZ = (FrontHit.ImpactPoint.Z + RearHit.ImpactPoint.Z) * 0.5f + RideHeight;
			FVector NewLoc = GetActorLocation();
			NewLoc.Z = FMath::FInterpTo(NewLoc.Z, DesiredZ, DeltaTime, 16.0f);
			SetActorLocation(NewLoc, true);

			VerticalVelocity = GroundSlopeClimbVel;
		}
		else if (bFrontGrounded || bRearGrounded)
		{
			const FHitResult& SingleHit = bFrontGrounded ? FrontHit : RearHit;
			const float DesiredZ = SingleHit.ImpactPoint.Z + RideHeight;
			FVector NewLoc = GetActorLocation();
			NewLoc.Z = FMath::FInterpTo(NewLoc.Z, DesiredZ, DeltaTime, 12.0f);
			SetActorLocation(NewLoc, true);

			VerticalVelocity = GroundSlopeClimbVel;
		}
		else
		{
			// 虽非跳跃但已离地，进入自由落体
			VerticalVelocity += GravityZ * DeltaTime;
			AddActorWorldOffset(FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime), true);
			CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, 0.0f, DeltaTime, 3.5f);
		}

		CurrentRot.Roll = 0.0f;
		SetActorRotation(CurrentRot);

		// 地面前进后退扫掠位移与越障处理 (平滑翻越小石头/路肩/台阶)
		if (FMath::Abs(CurrentSpeed) > 1.0f)
		{
			const FVector MoveDelta = GetActorForwardVector() * CurrentSpeed * DeltaTime;
			FHitResult MoveHit;
			AddActorWorldOffset(MoveDelta, true, &MoveHit);

			if (MoveHit.bBlockingHit)
			{
				bool bSteppedOver = false;

				// 越障检测：如果遇到矮小障碍物 (小于等于 MaxStepUpHeight，如小石头、石阶、路肩)，平滑向上抬升越过
				if (CurrentSpeed > 0.0f)
				{
					const float GroundZ = GetActorLocation().Z - RideHeight;
					const float ObstacleHeight = MoveHit.ImpactPoint.Z - GroundZ;

					if (ObstacleHeight > 0.0f && ObstacleHeight <= MaxStepUpHeight)
					{
						// Step 1: 向上试探抬升
						const FVector StepUpDelta = FVector(0.0f, 0.0f, ObstacleHeight + 4.0f);
						FHitResult UpHit;
						AddActorWorldOffset(StepUpDelta, true, &UpHit);

						if (!UpHit.bBlockingHit)
						{
							// Step 2: 向前推进一步越过障碍
							FHitResult FwdHit;
							AddActorWorldOffset(MoveDelta, true, &FwdHit);

							// Step 3: 向下探测并平稳落回新表面
							const FVector StepDownDelta = FVector(0.0f, 0.0f, -(ObstacleHeight + 8.0f));
							FHitResult DownHit;
							AddActorWorldOffset(StepDownDelta, true, &DownHit);

							bSteppedOver = true;
						}
					}
				}

				if (!bSteppedOver)
				{
					// 遭遇高墙或垂直大岩石等不可逾越障碍：沿碰撞法线切向滑动并适度衰减车速
					const FVector SlideDelta = FVector::VectorPlaneProject(MoveDelta, MoveHit.Normal);
					AddActorWorldOffset(SlideDelta * 0.5f, true);

					const float HitDot = FVector::DotProduct(-MoveHit.Normal, GetActorForwardVector());
					if (HitDot > 0.5f)
					{
						CurrentSpeed *= (1.0f - HitDot * 0.6f);
					}
				}
			}
		}
	}

	// 6. 视觉网格体姿态同步
	if (ChassisMesh)
	{
		ChassisMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, CurrentLeanRoll));
	}

	if (SteeringPivot)
	{
		SteeringPivot->SetRelativeRotation(FRotator(0.0f, DesiredSteerAngle, 0.0f));
	}

	// 车轮依行进距离旋转 (Pitch滚动)
	const float AngleDelta = (CurrentSpeed * DeltaTime / WheelRadius) * (180.0f / PI);
	WheelRotationAngle = FMath::Fmod(WheelRotationAngle + AngleDelta, 360.0f);

	if (FrontWheelMesh)
	{
		FrontWheelMesh->SetRelativeRotation(FRotator(90.0f, WheelRotationAngle, 0.0f));
	}
	if (RearWheelMesh)
	{
		RearWheelMesh->SetRelativeRotation(FRotator(90.0f, WheelRotationAngle, 0.0f));
	}

	// 7. 摄像机随车速自适应平滑拉远与空中广角飞跃拉远
	if (CameraBoom)
	{
		const float AirborneExtraDist = bIsAirborne ? 120.0f : 0.0f;
		const float DesiredArmLength = FMath::Lerp(BaseCameraDistance, HighSpeedCameraDistance, SpeedRatio) + AirborneExtraDist;
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, DesiredArmLength, DeltaTime, CameraZoomInterpSpeed);
	}
}

void AMotorcyclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 1. Enhanced Input 绑定支持
	if (UEnhancedInputComponent* EnhancedInputComp = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ThrottleAction)
		{
			EnhancedInputComp->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AMotorcyclePawn::OnEnhancedThrottle);
			EnhancedInputComp->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AMotorcyclePawn::OnEnhancedThrottle);
		}
		if (SteerAction)
		{
			EnhancedInputComp->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AMotorcyclePawn::OnEnhancedSteer);
			EnhancedInputComp->BindAction(SteerAction, ETriggerEvent::Completed, this, &AMotorcyclePawn::OnEnhancedSteer);
		}
		if (LookAction)
		{
			EnhancedInputComp->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMotorcyclePawn::OnEnhancedLook);
		}
		if (HandbrakeAction)
		{
			EnhancedInputComp->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &AMotorcyclePawn::OnEnhancedHandbrake);
			EnhancedInputComp->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &AMotorcyclePawn::OnEnhancedHandbrake);
		}
		if (DismountAction)
		{
			EnhancedInputComp->BindAction(DismountAction, ETriggerEvent::Started, this, &AMotorcyclePawn::OnEnhancedDismount);
		}
	}

	// 2. 原生键位双保险：F 键直接绑定下车
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMotorcyclePawn::InputDismount);
}

// -------------------------------------------------------------
// 乘车 / 下车核心逻辑
// -------------------------------------------------------------
bool AMotorcyclePawn::CanMount(ACharacter* InCharacter) const
{
	return InCharacter != nullptr && !IsOccupied();
}

bool AMotorcyclePawn::MountRider(ACharacter* InCharacter)
{
	if (!CanMount(InCharacter))
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(InCharacter->GetController());
	if (!PC)
	{
		return false;
	}

	CurrentRider = InCharacter;

	// 1. 禁用角色自身移动与碰撞体，防止发生物理冲撞爆飞
	if (UCapsuleComponent* Capsule = InCharacter->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* MoveComp = InCharacter->GetCharacterMovement())
	{
		MoveComp->DisableMovement();
		MoveComp->StopMovementImmediately();
	}

	// 2. 核心防顶镜头机制：将角色身上的所有网格体和图元组件对 ECC_Camera 设为 Ignore
	// 彻底杜绝弹簧臂探针碰撞角色后背导致臂长被压缩为 0 顶入体内的 Bug
	TArray<UPrimitiveComponent*> CharPrimitives;
	InCharacter->GetComponents<UPrimitiveComponent>(CharPrimitives);
	for (UPrimitiveComponent* Prim : CharPrimitives)
	{
		Prim->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// 3. 停用角色的全部相机与弹簧臂，彻底杜绝角色组件抢占视角
	TArray<UCameraComponent*> CharCameras;
	InCharacter->GetComponents<UCameraComponent>(CharCameras);
	for (UCameraComponent* Cam : CharCameras)
	{
		Cam->Deactivate();
	}

	TArray<USpringArmComponent*> CharBooms;
	InCharacter->GetComponents<USpringArmComponent>(CharBooms);
	for (USpringArmComponent* Boom : CharBooms)
	{
		Boom->Deactivate();
	}

	// 4. 将角色附着于车身座鞍组件 (SeatPoint)
	InCharacter->AttachToComponent(SeatPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InCharacter->SetActorRelativeLocation(RiderRelativeLocationOffset);
	InCharacter->SetActorRelativeRotation(RiderRelativeRotationOffset);

	// 5. 激活摩托车专用相机并确保悬臂展开
	if (CameraBoom)
	{
		CameraBoom->Activate();
		CameraBoom->bDoCollisionTest = bEnableCameraCollision;
		CameraBoom->TargetArmLength = BaseCameraDistance;
	}
	if (FollowCamera)
	{
		FollowCamera->Activate();
	}

	// 6. 将玩家控制权平滑过渡至摩托车，并显式将 ViewTarget 混合切换至摩托车自身
	PC->UnPossess();
	PC->Possess(this);
	PC->SetViewTargetWithBlend(this, 0.35f, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.0f);

	// 7. 广播事件并输出驾驶交互提示
	OnRiderMounted.Broadcast(InCharacter);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8801, 5.0f, FColor::Cyan,
			TEXT("[Motorcycle] Riding! [WASD] Drive | [Space] Handbrake | [F] Dismount"));
	}

	return true;
}

bool AMotorcyclePawn::DismountRider()
{
	if (!CurrentRider.IsValid())
	{
		return false;
	}

	ACharacter* Rider = CurrentRider.Get();
	APlayerController* PC = Cast<APlayerController>(GetController());

	// 1. 寻找安全下车避障着陆点
	FVector SafeLandingLocation;
	FRotator SafeLandingRotation;
	FindSafeDismountTransform(SafeLandingLocation, SafeLandingRotation);

	// 2. 将角色脱离附着，瞬移并贴合安全地面
	Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Rider->SetActorLocationAndRotation(SafeLandingLocation, SafeLandingRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// 3. 恢复角色正常胶囊体碰撞与行走移动模式
	if (UCapsuleComponent* Capsule = Rider->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (UCharacterMovementComponent* MoveComp = Rider->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// 4. 恢复角色所有图元对 ECC_Camera 的正常阻挡响应
	TArray<UPrimitiveComponent*> CharPrimitives;
	Rider->GetComponents<UPrimitiveComponent>(CharPrimitives);
	for (UPrimitiveComponent* Prim : CharPrimitives)
	{
		Prim->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	}

	// 5. 停用摩托车相机，重新激活角色自身的相机与弹簧臂
	if (FollowCamera)
	{
		FollowCamera->Deactivate();
	}

	TArray<UCameraComponent*> CharCameras;
	Rider->GetComponents<UCameraComponent>(CharCameras);
	for (UCameraComponent* Cam : CharCameras)
	{
		Cam->Activate();
	}

	TArray<USpringArmComponent*> CharBooms;
	Rider->GetComponents<USpringArmComponent>(CharBooms);
	for (USpringArmComponent* Boom : CharBooms)
	{
		Boom->Activate();
	}

	// 6. 控制权切回角色，显式将 ViewTarget 切回角色自身
	if (PC)
	{
		PC->UnPossess();
		PC->Possess(Rider);
		PC->SetViewTargetWithBlend(Rider, 0.3f, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.0f);
	}

	// 6. 载具归位与状态重置
	CurrentRider = nullptr;
	ThrottleInput = 0.0f;
	SteerInput = 0.0f;
	bHandbrake = false;

	OnRiderDismounted.Broadcast(Rider);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(8802, 3.5f, FColor::Yellow,
			TEXT("[Motorcycle] Dismounted safely."));
	}

	return true;
}

void AMotorcyclePawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	// 确保始终以摩托车自备的 FollowCamera (依托 CameraBoom 悬臂) 提供视口
	if (FollowCamera && FollowCamera->IsActive())
	{
		FollowCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

bool AMotorcyclePawn::FindSafeDismountTransform(FVector& OutLocation, FRotator& OutRotation) const
{
	const USceneComponent* TestPoints[2] = { DismountPointLeft, DismountPointRight };
	const float CapsuleRadius = 42.0f;
	const float CapsuleHalfHeight = 96.0f;

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MotorcycleDismountTrace), false, this);
	if (CurrentRider.IsValid())
	{
		TraceParams.AddIgnoredActor(CurrentRider.Get());
	}

	// 优先检测左侧、右侧落脚点
	for (int32 i = 0; i < 2; ++i)
	{
		if (!TestPoints[i]) continue;

		FVector TestLoc = TestPoints[i]->GetComponentLocation();

		// 地面射线探测
		FHitResult GroundHit;
		const FVector TraceStart = TestLoc + FVector(0.0f, 0.0f, 60.0f);
		const FVector TraceEnd = TestLoc - FVector(0.0f, 0.0f, 160.0f);

		if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
		{
			FVector CandidateLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.0f);

			// 胶囊体扫掠探测：确保落脚区域无墙壁或岩石卡死
			FHitResult ObstacleHit;
			FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

			bool bBlocked = GetWorld()->SweepSingleByChannel(
				ObstacleHit,
				CandidateLocation,
				CandidateLocation,
				FQuat::Identity,
				ECC_Pawn,
				CapsuleShape,
				TraceParams
			);

			if (!bBlocked)
			{
				OutLocation = CandidateLocation;
				OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
				return true;
			}
		}
	}

	// 备选方案：车后安全距离落地
	FVector RearLocation = GetActorLocation() - GetActorForwardVector() * 150.0f;
	FHitResult RearGroundHit;
	if (GetWorld()->LineTraceSingleByChannel(RearGroundHit, RearLocation + FVector(0, 0, 60), RearLocation - FVector(0, 0, 160), ECC_Visibility, TraceParams))
	{
		OutLocation = RearGroundHit.ImpactPoint + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.0f);
	}
	else
	{
		OutLocation = RearLocation + FVector(0.0f, 0.0f, CapsuleHalfHeight);
	}
	OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
	return true;
}

// -------------------------------------------------------------
// 输入响应与事件
// -------------------------------------------------------------
void AMotorcyclePawn::SetThrottleInput(float Value)
{
	ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AMotorcyclePawn::SetSteerInput(float Value)
{
	SteerInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AMotorcyclePawn::SetHandbrakeInput(bool bPressed)
{
	bHandbrake = bPressed;
}

void AMotorcyclePawn::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void AMotorcyclePawn::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void AMotorcyclePawn::InputDismount()
{
	DismountRider();
}

void AMotorcyclePawn::OnEnhancedThrottle(const FInputActionValue& Value)
{
	SetThrottleInput(Value.Get<float>());
}

void AMotorcyclePawn::OnEnhancedSteer(const FInputActionValue& Value)
{
	SetSteerInput(Value.Get<float>());
}

void AMotorcyclePawn::OnEnhancedLook(const FInputActionValue& Value)
{
	FVector2D LookVal = Value.Get<FVector2D>();
	Turn(LookVal.X);
	LookUp(LookVal.Y);
}

void AMotorcyclePawn::OnEnhancedHandbrake(const FInputActionValue& Value)
{
	SetHandbrakeInput(Value.Get<bool>());
}

void AMotorcyclePawn::OnEnhancedDismount(const FInputActionValue& Value)
{
	InputDismount();
}

void AMotorcyclePawn::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Char = Cast<ACharacter>(OtherActor))
	{
		if (Char != CurrentRider.Get())
		{
			NearbyCharacter = Char;
			if (GEngine && !IsOccupied())
			{
				GEngine->AddOnScreenDebugMessage(8800, 3.0f, FColor::Emerald,
					TEXT("Press [F] to Ride Motorcycle"));
			}
		}
	}
}

void AMotorcyclePawn::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == NearbyCharacter.Get())
	{
		NearbyCharacter = nullptr;
	}
}
