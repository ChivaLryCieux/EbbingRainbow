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

	TargetArmLength = DefaultArmLength;
	TargetYaw = DefaultYaw;
	TargetPitch = DefaultPitch;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->TargetArmLength = TargetArmLength;
	SpringArm->SetRelativeRotation(FRotator(TargetPitch, TargetYaw, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

#if WITH_EDITOR
void ATacticalCameraPawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (SpringArm)
	{
		SpringArm->TargetArmLength = DefaultArmLength;
		SpringArm->SetRelativeRotation(FRotator(DefaultPitch, DefaultYaw, 0.0f));
	}
}
#endif

void ATacticalCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	TargetLocation = GetActorLocation();
	TargetArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);
	TargetYaw = DefaultYaw;
	TargetPitch = FMath::Clamp(DefaultPitch, MinPitch, MaxPitch);

	if (bEnableZoomPitchCoupling)
	{
		TargetPitch = FMath::Clamp(GetCoupledPitchForArmLength(TargetArmLength), MinPitch, MaxPitch);
	}

	if (SpringArm)
	{
		SpringArm->TargetArmLength = TargetArmLength;
		SpringArm->SetRelativeRotation(FRotator(TargetPitch, TargetYaw, 0.0f));
	}

	// 竞技场范围由配置确定 (无需等待生成完成),直接聚焦其中心
	for (TActorIterator<ABattleArena> It(GetWorld()); It; ++It)
	{
		SetMoveBounds(It->GetArenaBounds());
		SnapTo(It->GetArenaCenter());
		break;
	}
}

void ATacticalCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopOrbit();
	Super::EndPlay(EndPlayReason);
}

void ATacticalCameraPawn::UnPossessed()
{
	StopOrbit();
	Super::UnPossessed();
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

	// 中键按住拖动：围绕当前聚焦点做轨道旋转（博德之门 3 风格）
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ATacticalCameraPawn::StartOrbit);
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &ATacticalCameraPawn::StopOrbit);
}

void ATacticalCameraPawn::StartOrbit()
{
	bOrbiting = true;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float MX = 0.0f;
		float MY = 0.0f;
		if (PC->GetMousePosition(MX, MY))
		{
			InitialOrbitMousePos = FVector2D(MX, MY);
			LastMousePos = FVector2D(MX, MY);
		}
		PC->bShowMouseCursor = false;
	}
}

void ATacticalCameraPawn::StopOrbit()
{
	if (!bOrbiting)
	{
		return;
	}

	bOrbiting = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;

		int32 SizeX = 0, SizeY = 0;
		PC->GetViewportSize(SizeX, SizeY);
		if (SizeX > 0 && SizeY > 0 && (InitialOrbitMousePos.X > 0.0f || InitialOrbitMousePos.Y > 0.0f))
		{
			const int32 SafeX = FMath::Clamp(FMath::RoundToInt(InitialOrbitMousePos.X), 5, SizeX - 5);
			const int32 SafeY = FMath::Clamp(FMath::RoundToInt(InitialOrbitMousePos.Y), 5, SizeY - 5);
			PC->SetMouseLocation(SafeX, SafeY);
		}
	}
}

float ATacticalCameraPawn::GetCoupledPitchForArmLength(float InArmLength) const
{
	if (!bEnableZoomPitchCoupling || MaxArmLength <= MinArmLength)
	{
		return TargetPitch;
	}

	const float Alpha = FMath::Clamp((InArmLength - MinArmLength) / (MaxArmLength - MinArmLength), 0.0f, 1.0f);
	return FMath::Lerp(ClosePitch, FarPitch, Alpha);
}

void ATacticalCameraPawn::ZoomIn()
{
	TargetArmLength = FMath::Clamp(TargetArmLength - ZoomStep, MinArmLength, MaxArmLength);
	if (bEnableZoomPitchCoupling)
	{
		TargetPitch = FMath::Clamp(GetCoupledPitchForArmLength(TargetArmLength) + ManualPitchOffset, MinPitch, MaxPitch);
	}
}

void ATacticalCameraPawn::ZoomOut()
{
	TargetArmLength = FMath::Clamp(TargetArmLength + ZoomStep, MinArmLength, MaxArmLength);
	if (bEnableZoomPitchCoupling)
	{
		TargetPitch = FMath::Clamp(GetCoupledPitchForArmLength(TargetArmLength) + ManualPitchOffset, MinPitch, MaxPitch);
	}
}

void ATacticalCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		// 严密防御：若中键物理抬起，立即停止旋转
		if (bOrbiting && !PC->IsInputKeyDown(EKeys::MiddleMouseButton))
		{
			StopOrbit();
		}

		if (bOrbiting)
		{
			// 主通道：读取输入系统累积的每帧鼠标增量 (MouseX/MouseY)。
			// FInputModeGameAndUI 使用 CaptureDuringMouseDown，按住中键期间视口处于捕获状态，
			// 引擎只在捕获期间累积并发送鼠标增量；该通道与光标绝对位置、屏幕边缘、
			// 窗口布局和 DPI 无关，从根源上消除“传送光标与事件回写打架导致每帧巨量增量”的狂转问题。
			float DeltaX = 0.0f;
			float DeltaY = 0.0f;
			PC->GetInputMouseDelta(DeltaX, DeltaY);

			// 兜底通道：捕获意外丢失时 (如切窗) 退化为绝对坐标差分。
			// 每帧重新锚定且不再传送光标，并钳制单帧位移 (捕获切换瞬间绝对坐标可能跳变)，
			// 保证任何情况下都不会产生持续的巨量增量。
			if (FMath::IsNearlyZero(DeltaX) && FMath::IsNearlyZero(DeltaY))
			{
				float CurX = 0.0f;
				float CurY = 0.0f;
				if (PC->GetMousePosition(CurX, CurY))
				{
					const FVector2D AbsDelta(CurX - LastMousePos.X, CurY - LastMousePos.Y);
					if (FMath::Abs(AbsDelta.X) <= MaxOrbitFallbackDeltaPixels)
					{
						DeltaX = AbsDelta.X;
					}
					if (FMath::Abs(AbsDelta.Y) <= MaxOrbitFallbackDeltaPixels)
					{
						// 绝对坐标 Y 向下为正，MouseY 轴向上为正，取反以统一方向
						DeltaY = -AbsDelta.Y;
					}
					LastMousePos = FVector2D(CurX, CurY);
				}
			}
			else
			{
				// 捕获期间绝对坐标不可信，仅用于保持兜底锚点新鲜
				float CurX = 0.0f;
				float CurY = 0.0f;
				if (PC->GetMousePosition(CurX, CurY))
				{
					LastMousePos = FVector2D(CurX, CurY);
				}
			}

			if (!FMath::IsNearlyZero(DeltaX))
			{
				TargetYaw += DeltaX * OrbitYawSpeed;
			}

			if (!FMath::IsNearlyZero(DeltaY))
			{
				// MouseY 轴向上为正：向上拖动视角放平，向下拖动俯视加深 (与原绝对坐标实现手感一致)
				const float PitchDelta = DeltaY * OrbitPitchSpeed * (bInvertOrbitPitch ? -1.0f : 1.0f);
				TargetPitch = FMath::Clamp(TargetPitch + PitchDelta, MinPitch, MaxPitch);

				if (bEnableZoomPitchCoupling)
				{
					ManualPitchOffset = TargetPitch - GetCoupledPitchForArmLength(TargetArmLength);
				}
			}
		}

		FVector2D Input = FVector2D::ZeroVector;
		if (PC->IsInputKeyDown(EKeys::W)) Input.X += 1.0f;
		if (PC->IsInputKeyDown(EKeys::S)) Input.X -= 1.0f;
		if (PC->IsInputKeyDown(EKeys::D)) Input.Y += 1.0f;
		if (PC->IsInputKeyDown(EKeys::A)) Input.Y -= 1.0f;

		if (EdgeScrollMargin > 0.0f && !bOrbiting)
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

			// 缩放越远平移越快，基准臂长按 2000 归一化
			const float ZoomFactor = FMath::Max(0.35f, SpringArm->TargetArmLength / 2000.0f);
			FocusOn(TargetLocation + Dir * PanSpeed * ZoomFactor * DeltaSeconds);
			// 手动平移时直接跟随, 避免插值滞后
			SetActorLocation(TargetLocation);
		}

		if (PC->IsInputKeyDown(EKeys::Q)) TargetYaw -= RotateSpeed * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::E)) TargetYaw += RotateSpeed * DeltaSeconds;
	}

	// 平滑聚焦 / 缩放 / 旋转
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, FocusInterpSpeed));
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);

	// Yaw 保持归一化并按最短路径插值（与 FInterpTo 同公式），
	// 防止累计圈数或归一化边界处出现“绕远路”式的多余旋转
	TargetYaw = FMath::UnwindDegrees(TargetYaw);
	const float CurrentYaw = FMath::UnwindDegrees(SpringArm->GetRelativeRotation().Yaw);
	const float CurrentPitch = SpringArm->GetRelativeRotation().Pitch;
	const float YawDist = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	const float NewYaw = (YawDist * YawDist < SMALL_NUMBER)
		? TargetYaw
		: CurrentYaw + YawDist * FMath::Clamp(DeltaSeconds * RotateInterpSpeed, 0.0f, 1.0f);
	const float NewPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaSeconds, RotateInterpSpeed);
	SpringArm->SetRelativeRotation(FRotator(NewPitch, NewYaw, 0.0f));
}
