#include "Hall/HallCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AHallCameraPawn::AHallCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(RootComponent);
	CameraComponent->SetFieldOfView(65.0f);
	CameraComponent->bUsePawnControlRotation = false;

#if WITH_EDITORONLY_DATA
	DirectionArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	if (DirectionArrow)
	{
		DirectionArrow->SetupAttachment(RootComponent);
		DirectionArrow->ArrowColor = FColor(70, 160, 255);
		DirectionArrow->ArrowSize = 2.0f;
		DirectionArrow->bIsScreenSizeScaled = true;
	}
#endif
}

void AHallCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// 忠实记录关卡设计中人工摆放的初始机位与朝向
	BaseRotation = GetActorRotation();
	BaseLocation = GetActorLocation();

	if (AController* C = GetController())
	{
		C->SetControlRotation(BaseRotation);
	}
}

void AHallCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RunningTime += DeltaSeconds;

	// 1. 宇宙悬浮呼吸动效 (极缓起伏)
	const float FloatZ = FMath::Sin(RunningTime * 0.4f) * 6.0f;
	const float FloatY = FMath::Cos(RunningTime * 0.25f) * 4.0f;
	const FVector TargetLoc = BaseLocation + FVector(0.0f, FloatY, FloatZ);
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLoc, DeltaSeconds, 2.0f));

	// 2. 鼠标移动引起的轻微视角视差 (Parallax)
	FRotator TargetRot = BaseRotation;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float MouseX, MouseY;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			int32 SizeX, SizeY;
			PC->GetViewportSize(SizeX, SizeY);
			if (SizeX > 0 && SizeY > 0)
			{
				const float NormX = (MouseX / (float)SizeX - 0.5f) * 2.0f; // -1.0 ~ 1.0
				const float NormY = (MouseY / (float)SizeY - 0.5f) * 2.0f;

				const float ParallaxYaw = NormX * 2.0f;
				const float ParallaxPitch = -NormY * 1.5f;
				TargetRot = BaseRotation + FRotator(ParallaxPitch, ParallaxYaw, 0.0f);
			}
		}
	}

	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaSeconds, 3.5f));
}
