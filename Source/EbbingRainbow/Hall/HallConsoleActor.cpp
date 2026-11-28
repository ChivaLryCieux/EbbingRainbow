// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallConsoleActor.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/ERLog.h"
#include "Engine/StaticMesh.h"
#include "Hall/HallHUD.h"
#include "Hall/HallPlayerController.h"
#include "UObject/ConstructorHelpers.h"

AHallConsoleActor::AHallConsoleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ConsoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMesh"));
	SetRootComponent(ConsoleMesh);
	ConsoleMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ConsoleMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConsoleMeshFinder(
		TEXT("/Game/WFCKit/Meshes/Props/SM_prop_console.SM_prop_console"));
	if (ConsoleMeshFinder.Succeeded())
	{
		ConsoleMesh->SetStaticMesh(ConsoleMeshFinder.Object);
	}

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(ConsoleMesh);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphere->SetGenerateOverlapEvents(false);
	InteractionSphere->SetHiddenInGame(true);

	ScreenLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScreenLight"));
	ScreenLight->SetupAttachment(ConsoleMesh);
	ScreenLight->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	ScreenLight->SetIntensity(180.0f);
	ScreenLight->SetAttenuationRadius(260.0f);
	ScreenLight->SetLightColor(FColor(72, 184, 255));
	ScreenLight->SetCastShadows(false);
}

void AHallConsoleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(FMath::Max(40.0f, InteractionRadius));
	}
}

void AHallConsoleActor::Interact(AHallPlayerController* RequestingController)
{
	if (!RequestingController)
	{
		return;
	}
	if (AHallHUD* HallHUD = Cast<AHallHUD>(RequestingController->GetHUD()))
	{
		HallHUD->OpenGalacticPanel(FMath::Clamp(InitialTab, 0, 3));
		return;
	}
	UE_LOG(LogERHall, Warning, TEXT("[HallConsole] Interact ignored: owning HUD is not AHallHUD."));
}

bool AHallConsoleActor::IsInteractionCandidate(const FVector& ViewLocation, const FVector& ViewForward, float& OutDistanceSq) const
{
	const FVector Center = GetActorLocation();
	OutDistanceSq = FVector::DistSquared(ViewLocation, Center);
	const float Radius = FMath::Max(40.0f, InteractionRadius);
	if (OutDistanceSq > FMath::Square(Radius))
	{
		return false;
	}
	if (OutDistanceSq <= FMath::Square(FMath::Max(0.0f, CloseRangeOverride)))
	{
		return true;
	}

	FVector ToConsole2D = Center - ViewLocation;
	ToConsole2D.Z = 0.0f;
	FVector View2D = ViewForward;
	View2D.Z = 0.0f;
	if (!ToConsole2D.Normalize() || !View2D.Normalize())
	{
		return true;
	}
	return FVector::DotProduct(View2D, ToConsole2D) >= MinViewDot;
}
