// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RouteCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

ARouteCameraPawn::ARouteCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetRelativeLocation(FVector::ZeroVector);
	Camera->SetFieldOfView(90.0f);
}
