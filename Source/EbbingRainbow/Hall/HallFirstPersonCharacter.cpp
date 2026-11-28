// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallFirstPersonCharacter.h"

#include "Components/InputComponent.h"
#include "Core/ERLog.h"
#include "EnhancedInputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hall/HallPlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"

AHallFirstPersonCharacter::AHallFirstPersonCharacter()
{
	// 大厅不是射击场景: 清空军团指挥官的出生手枪, 输入层也不再绑定开火/切枪。
	StartingWeaponClasses.Reset();

	// 备战大厅空间远小于军团战场; 保留同一控制器/手感曲线, 仅把速度压到室内步行尺度。
	WalkSpeed = 560.0f;
	SprintSpeed = 1050.0f;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}

	// 复用 BP_ShooterCharacter 的第一人称 Manny 配置, 使相机 head socket / 手臂动画
	// 与 Lvl_Legion 的指挥官方位完全一致。网格资产缺失时仍退化为无手臂相机, 不影响玩法。
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyFinder.Object);
		if (USkeletalMeshComponent* FPMesh = GetFirstPersonMesh())
		{
			FPMesh->SetSkeletalMesh(MannyFinder.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UClass> UnarmedAnimFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	if (UnarmedAnimFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(UnarmedAnimFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UClass> FPAnimFinder(
		TEXT("/Game/FirstPerson/Anims/ABP_FP_Copy.ABP_FP_Copy_C"));
	if (FPAnimFinder.Succeeded())
	{
		if (USkeletalMeshComponent* FPMesh = GetFirstPersonMesh())
		{
			FPMesh->SetAnimInstanceClass(FPAnimFinder.Object);
		}
	}

	// 与 BP_ShooterCharacter 序列化值一致的身体网格基准 (FP mesh 挂在其 head 骨骼链上)。
	GetMesh()->SetRelativeLocation(FVector(-10.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}

void AHallFirstPersonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogERHall, Error, TEXT("'%s' requires Enhanced Input Component."), *GetNameSafe(this));
		return;
	}

	// 与军团指挥官同一组 IA/触发态; 差异仅在大厅不绑定 Fire / SwitchWeapon。
	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AHallFirstPersonCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHallFirstPersonCharacter::DoJumpEnd);
	}
	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::MoveInput);
	}
	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::LookInput);
	}
	if (MouseLookAction)
	{
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALegionCommanderCharacter::LookInput);
	}
	if (SprintAction)
	{
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ALegionCommanderCharacter::DoSprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ALegionCommanderCharacter::DoSprintEnd);
	}
}

bool AHallFirstPersonCharacter::IsHallInputBlocked() const
{
	const AHallPlayerController* PC = GetController<AHallPlayerController>();
	if (!PC)
	{
		return false;
	}
	return !PC->IsFirstPersonActive() || PC->bShowMouseCursor || PC->IsAnyHallPanelOpen();
}

void AHallFirstPersonCharacter::DoAim(float Yaw, float Pitch)
{
	if (IsHallInputBlocked()) { return; }
	ALegionCommanderCharacter::DoAim(Yaw, Pitch);
}

void AHallFirstPersonCharacter::DoMove(float Right, float Forward)
{
	if (IsHallInputBlocked()) { return; }
	ALegionCommanderCharacter::DoMove(Right, Forward);
}

void AHallFirstPersonCharacter::DoJumpStart()
{
	if (IsHallInputBlocked()) { return; }
	ALegionCommanderCharacter::DoJumpStart();
}

void AHallFirstPersonCharacter::DoJumpEnd()
{
	// Completed 事件即使在面板刚打开的一帧内也必须送达, 否则 Jump 输入会卡住。
	ALegionCommanderCharacter::DoJumpEnd();
}

void AHallFirstPersonCharacter::SetSprinting(bool bSprint)
{
	if (bSprint && IsHallInputBlocked()) { return; }
	ALegionCommanderCharacter::SetSprinting(bSprint);
}
