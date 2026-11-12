// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/FX/BattleFloatingText.h"
#include "Battle/FX/UBattleFXPool.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

ABattleFloatingText::ABattleFloatingText()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	TextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRender"));
	TextRender->SetupAttachment(RootComponent);
	TextRender->SetHorizontalAlignment(EHTA_Center);
	TextRender->SetVerticalAlignment(EVRTA_TextCenter);
	TextRender->SetCanEverAffectNavigation(false);

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
	PopStartScale = InWorldSize >= 44.0f ? 1.6f : 1.4f;
	LastWrittenAlpha = 255;

	SetActorLocation(Location);
	SetActorScale3D(FVector::OneVector);
	SetActorRotation(FRotator::ZeroRotator);

	if (TextRender)
	{
		TextRender->SetText(FText::FromString(Text));
		TextRender->SetTextRenderColor(BaseColor);
		TextRender->SetWorldSize(InWorldSize); // 基础字号只在这里写, 之后用 ActorScale 做弹跳
	}

	// 同一位置连冒多个跳字时错峰, 避免完全重叠
	FVector Scatter(FMath::FRandRange(-70.0f, 70.0f), FMath::FRandRange(-70.0f, 70.0f), FMath::FRandRange(-18.0f, 18.0f));

	// 顺着摄像机右方向再扇开一点, 读感更接近主流 ARPG 跳字
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			const FVector CamRight = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector);
			Scatter += CamRight * FMath::FRandRange(-60.0f, 60.0f);
		}
	}

	const float UpSpeed = FMath::FRandRange(150.0f, 190.0f);
	Velocity = FVector(Scatter.X, Scatter.Y, UpSpeed);

	// 大数字 (重击/弱点) 多停留一会, 小数字快速退场
	LifeTime = InWorldSize >= 44.0f ? 1.35f : 1.1f;
	ElapsedTime = 0.0f;

	AddActorWorldOffset(Scatter * 0.15f);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void ABattleFloatingText::PoolRelease()
{
	SetActorHiddenInGame(true);
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
	Velocity.Z -= 130.0f * DeltaSeconds;
	SetActorLocation(GetActorLocation() + Velocity * DeltaSeconds);

	// 始终朝向玩家摄像机
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
			const FVector ToCam = CamLoc - GetActorLocation();
			SetActorRotation(ToCam.Rotation());
		}
	}

	// 动态缩放冲击: 前 0.2 秒从 PopStartScale 弹回 1.0x。
	// 用 ActorScale 代替每帧 SetWorldSize —— 后者每帧重建文字几何体
	if (ElapsedTime < 0.2f)
	{
		const float ScaleFactor = FMath::Lerp(PopStartScale, 1.0f, ElapsedTime / 0.2f);
		SetActorScale3D(FVector(ScaleFactor));
	}
	else if (!GetActorScale3D().Equals(FVector::OneVector))
	{
		SetActorScale3D(FVector::OneVector);
	}

	// 淡出透明度: 后半段平滑降低 Alpha; 量化到 1/16 档, 档位没变就跳过
	// SetTextRenderColor (它每次调用都触发渲染状态重建)
	float Alpha = 1.0f;
	const float FadeStartTime = LifeTime * 0.55f;
	if (ElapsedTime > FadeStartTime)
	{
		Alpha = FMath::Clamp(1.0f - (ElapsedTime - FadeStartTime) / (LifeTime - FadeStartTime), 0.0f, 1.0f);
	}
	const uint8 QuantAlpha = static_cast<uint8>(FMath::RoundToInt(Alpha * 16.0f)) * 16;
	if (TextRender && QuantAlpha != LastWrittenAlpha)
	{
		LastWrittenAlpha = QuantAlpha;
		FColor CurrColor = BaseColor;
		CurrColor.A = QuantAlpha;
		TextRender->SetTextRenderColor(CurrColor);
	}
}
