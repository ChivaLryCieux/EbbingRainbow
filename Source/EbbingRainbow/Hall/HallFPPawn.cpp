// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallFPPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"

AHallFPPawn::AHallFPPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 眼位相机: yaw 由控制器转全身, pitch 只转相机 (标准 FP)
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 68.0f));
	Camera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
		Move->GravityScale = 1.0f;
		Move->AirControl = 0.3f;
	}
}

void AHallFPPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerInput)
	{
		return;
	}

	// 光标释放(UI 模式)时不吃移动/视角输入
	if (PC->bShowMouseCursor)
	{
		return;
	}

	// 鼠标视角
	float MDX = 0.0f, MDY = 0.0f;
	PC->GetInputMouseDelta(MDX, MDY);
	AddControllerYawInput(MDX * LookSensitivity);
	AddControllerPitchInput(MDY * LookSensitivity);

	// WASD 移动 (+ Shift 冲刺)
	UPlayerInput* Input = PC->PlayerInput;
	const float Fwd = (Input->IsPressed(EKeys::W) ? 1.0f : 0.0f)
		- (Input->IsPressed(EKeys::S) ? 1.0f : 0.0f);
	const float Rgt = (Input->IsPressed(EKeys::D) ? 1.0f : 0.0f)
		- (Input->IsPressed(EKeys::A) ? 1.0f : 0.0f);
	const bool bSprint = Input->IsPressed(EKeys::LeftShift) || Input->IsPressed(EKeys::RightShift);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
	}
	if (Fwd != 0.0f) { AddMovementInput(GetActorForwardVector(), Fwd); }
	if (Rgt != 0.0f) { AddMovementInput(GetActorRightVector(), Rgt); }
}
