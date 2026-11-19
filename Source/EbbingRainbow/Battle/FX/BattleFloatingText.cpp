// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/FX/BattleFloatingText.h"
#include "Battle/FX/UBattleFXPool.h"
#include "UI/ERFloatingText.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

ABattleFloatingText::ABattleFloatingText()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	WidgetComp->SetupAttachment(RootComponent);
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetDrawAtDesiredSize(true);
	WidgetComp->SetPivot(FVector2D(0.5f, 0.5f));
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->SetCanEverAffectNavigation(false);

	FloatingWidgetClass = UERFloatingText::StaticClass();

	TextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRender"));
	TextRender->SetupAttachment(RootComponent);
	TextRender->SetHorizontalAlignment(EHTA_Center);
	TextRender->SetVerticalAlignment(EVRTA_TextCenter);
	TextRender->SetCanEverAffectNavigation(false);
	TextRender->SetHiddenInGame(true);

	SetActorEnableCollision(false);
}

void ABattleFloatingText::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false); // 池化: 出生即休眠, 等 PoolAcquire 唤醒
}

void ABattleFloatingText::PoolAcquire(const FVector& Location, const FString& Text,
	const FColor& InColor, float InWorldSize)
{
	BaseColor = InColor;
	PopStartScale = InWorldSize >= CritSizeThreshold ? CritPopScale : NormalPopScale;
	LastWrittenAlpha = 255;

	SetActorLocation(Location);
	SetActorScale3D(FVector::OneVector);
	SetActorRotation(FRotator::ZeroRotator);

	const bool bUseUMG = (WidgetComp != nullptr && FloatingWidgetClass != nullptr);
	if (bUseUMG)
	{
		if (WidgetComp->GetWidgetClass() != FloatingWidgetClass)
		{
			WidgetComp->SetWidgetClass(FloatingWidgetClass);
		}
		WidgetComp->InitWidget();
		WidgetComp->SetHiddenInGame(false);
		if (TextRender) { TextRender->SetHiddenInGame(true); }

		if (UERFloatingText* FT = Cast<UERFloatingText>(WidgetComp->GetUserWidgetObject()))
		{
			FT->SetFloatingData(FText::FromString(Text), FLinearColor::FromSRGBColor(BaseColor), InWorldSize);
			FT->SetPopupTransform(PopStartScale, 1.0f);
		}
	}
	else if (TextRender)
	{
		if (WidgetComp) { WidgetComp->SetHiddenInGame(true); }
		TextRender->SetHiddenInGame(false);
		TextRender->SetText(FText::FromString(Text));
		TextRender->SetTextRenderColor(BaseColor);
		TextRender->SetWorldSize(InWorldSize);
	}

	// 同一位置连冒多个跳字时错峰, 避免完全重叠
	FVector Scatter(
		FMath::FRandRange(-ScatterXY, ScatterXY),
		FMath::FRandRange(-ScatterXY, ScatterXY),
		FMath::FRandRange(-ScatterZ, ScatterZ));

	// 顺着摄像机右方向再扇开一点, 读感更接近主流 ARPG 跳字
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			const FVector CamRight = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector);
			Scatter += CamRight * FMath::FRandRange(-CameraRightScatter, CameraRightScatter);
		}
	}

	const float UpSpeed = FMath::FRandRange(UpSpeedMin, UpSpeedMax);
	Velocity = FVector(Scatter.X, Scatter.Y, UpSpeed);

	LifeTime = InWorldSize >= CritSizeThreshold ? CritLifeTime : NormalLifeTime;
	ElapsedTime = 0.0f;

	AddActorWorldOffset(Scatter * 0.15f);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void ABattleFloatingText::PoolRelease()
{
	SetActorHiddenInGame(true);
	if (WidgetComp) { WidgetComp->SetHiddenInGame(true); }
	SetActorTickEnabled(false);
	Velocity = FVector::ZeroVector;
	ElapsedTime = 0.0f;
}

ABattleFloatingText* ABattleFloatingText::SpawnFloatingText(UWorld* World, const FVector& Location, const FString& Text, const FColor& Color, float WorldSize)
{
	if (UBattleFXPool* Pool = UBattleFXPool::Get(World))
	{
		return Pool->SpawnFloatingText(Location, Text, Color, WorldSize);
	}
	return nullptr;
}

void ABattleFloatingText::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	if (ElapsedTime >= LifeTime)
	{
		if (UBattleFXPool* Pool = UBattleFXPool::Get(GetWorld()))
		{
			Pool->ReleaseFloatingText(this);
		}
		else
		{
			Destroy();
		}
		return;
	}

	// 物理弹跳轨迹 (重力衰减)
	Velocity.Z -= Gravity * DeltaSeconds;
	SetActorLocation(GetActorLocation() + Velocity * DeltaSeconds);

	// 弹跳缩放 + 淡出计算
	const float PopDur = FMath::Max(0.01f, PopDuration);
	const float ScaleFactor = (ElapsedTime < PopDur)
		? FMath::Lerp(PopStartScale, 1.0f, ElapsedTime / PopDur)
		: 1.0f;

	float Alpha = 1.0f;
	const float FadeStartTime = LifeTime * FadeStartFraction;
	if (ElapsedTime > FadeStartTime)
	{
		Alpha = FMath::Clamp(1.0f - (ElapsedTime - FadeStartTime) / FMath::Max(0.01f, LifeTime - FadeStartTime), 0.0f, 1.0f);
	}

	if (WidgetComp && !WidgetComp->bHiddenInGame)
	{
		if (UERFloatingText* FT = Cast<UERFloatingText>(WidgetComp->GetUserWidgetObject()))
		{
			FT->SetPopupTransform(ScaleFactor, Alpha);
		}
		return;
	}

	// 兜底 TextRender: 始终朝向玩家摄像机
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
			const FVector ToCam = CamLoc - GetActorLocation();
			SetActorRotation(ToCam.Rotation());
		}
	}

	SetActorScale3D(FVector(ScaleFactor));

	const uint8 QuantAlpha = static_cast<uint8>(FMath::RoundToInt(Alpha * 16.0f)) * 16;
	if (TextRender && QuantAlpha != LastWrittenAlpha)
	{
		LastWrittenAlpha = QuantAlpha;
		FColor CurrColor = BaseColor;
		CurrColor.A = QuantAlpha;
		TextRender->SetTextRenderColor(CurrColor);
	}
}
