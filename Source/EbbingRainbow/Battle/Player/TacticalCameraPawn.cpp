// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/Arena/BattleArena.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATacticalCameraPawn::ATacticalCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->TargetArmLength = TargetArmLength;
	SpringArm->SetRelativeRotation(FRotator(-60.0f, TargetYaw, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ATacticalCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	TargetLocation = GetActorLocation();

	// 竞技场范围由配置确定 (无需等待生成完成),直接聚焦其中心
	for (TActorIterator<ABattleArena> It(GetWorld()); It; ++It)
	{
		SetMoveBounds(It->GetArenaBounds());
		SnapTo(It->GetArenaCenter());
		break;
	}
}

void ATacticalCameraPawn::SetMoveBounds(const FBox& InBounds)
{
	MoveBounds = InBounds;
}

void ATacticalCameraPawn::FocusOn(const FVector& WorldLocation)
{
	TargetLocation = WorldLocation;
	if (MoveBounds.IsValid)
	{
		TargetLocation.X = FMath::Clamp(TargetLocation.X, MoveBounds.Min.X, MoveBounds.Max.X);
		TargetLocation.Y = FMath::Clamp(TargetLocation.Y, MoveBounds.Min.Y, MoveBounds.Max.Y);
	}
}

void ATacticalCameraPawn::SnapTo(const FVector& WorldLocation)
{
	FocusOn(WorldLocation);
	SetActorLocation(TargetLocation);
}

void ATacticalCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ATacticalCameraPawn::ZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ATacticalCameraPawn::ZoomOut);
}

void ATacticalCameraPawn::ZoomIn()
{
	TargetArmLength = FMath::Clamp(TargetArmLength - ZoomStep, MinArmLength, MaxArmLength);
}

void ATacticalCameraPawn::ZoomOut()
{
	TargetArmLength = FMath::Clamp(TargetArmLength + ZoomStep, MinArmLength, MaxArmLength);
}

void ATacticalCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		FVector2D Input = FVector2D::ZeroVector;
		if (PC->IsInputKeyDown(EKeys::W)) Input.X += 1.0f;
		if (PC->IsInputKeyDown(EKeys::S)) Input.X -= 1.0f;
		if (PC->IsInputKeyDown(EKeys::D)) Input.Y += 1.0f;
		if (PC->IsInputKeyDown(EKeys::A)) Input.Y -= 1.0f;

		if (EdgeScrollMargin > 0.0f)
		{
			float MX, MY;
			int32 SX, SY;
			if (PC->GetMousePosition(MX, MY))
			{
				PC->GetViewportSize(SX, SY);
				if (MX <= EdgeScrollMargin) Input.Y -= 1.0f;
				if (MX >= SX - EdgeScrollMargin) Input.Y += 1.0f;
				if (MY <= EdgeScrollMargin) Input.X += 1.0f;
				if (MY >= SY - EdgeScrollMargin) Input.X -= 1.0f;
			}
		}

		const float YawNow = SpringArm->GetRelativeRotation().Yaw;
		if (!Input.IsNearlyZero())
		{
			const FRotator YawRot(0.0f, YawNow, 0.0f);
			const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			const FVector Dir = (Forward * Input.X + Right * Input.Y).GetSafeNormal();
			// 缩放越远平移越快
			const float ZoomFactor = SpringArm->TargetArmLength / 2800.0f;
			FocusOn(TargetLocation + Dir * PanSpeed * ZoomFactor * DeltaSeconds);
			// 手动平移时直接跟随,避免插值滞后
			SetActorLocation(TargetLocation);
		}

		if (PC->IsInputKeyDown(EKeys::Q)) TargetYaw -= RotateSpeed * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::E)) TargetYaw += RotateSpeed * DeltaSeconds;
	}

	// 平滑聚焦 / 缩放 / 旋转
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, FocusInterpSpeed));
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaSeconds, 8.0f);
	const float NewYaw = FMath::FInterpTo(SpringArm->GetRelativeRotation().Yaw, TargetYaw, DeltaSeconds, 8.0f);
	SpringArm->SetRelativeRotation(FRotator(-60.0f, NewYaw, 0.0f));
}
