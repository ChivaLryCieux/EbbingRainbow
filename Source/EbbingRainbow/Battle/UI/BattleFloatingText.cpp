// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/UI/BattleFloatingText.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

ABattleFloatingText::ABattleFloatingText()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	TextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRender"));
	TextRender->SetupAttachment(RootComponent);
	TextRender->SetHorizontalAlignment(EHTA_Center);
	TextRender->SetVerticalAlignment(EVRTA_TextCenter);
	TextRender->SetWorldSize(36.0f);
	TextRender->SetCanEverAffectNavigation(false);

	SetActorEnableCollision(false);
}

void ABattleFloatingText::BeginPlay()
{
	Super::BeginPlay();

	// 兜底初始速度: 具体散布在 SetupFloatingText 里按尺寸重算
	const float UpSpeed = FMath::FRandRange(150.0f, 190.0f);
	Velocity = FVector(FMath::FRandRange(-70.0f, 70.0f), FMath::FRandRange(-70.0f, 70.0f), UpSpeed);
}

void ABattleFloatingText::SetupFloatingText(const FString& Text, const FColor& InColor, float InWorldSize)
{
	BaseColor = InColor;
	BaseWorldSize = InWorldSize;
	PopStartScale = InWorldSize >= 44.0f ? 1.6f : 1.4f;

	if (TextRender)
	{
		TextRender->SetText(FText::FromString(Text));
		TextRender->SetTextRenderColor(BaseColor);
		TextRender->SetWorldSize(BaseWorldSize);
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
}

ABattleFloatingText* ABattleFloatingText::SpawnFloatingText(UWorld* World, const FVector& Location, const FString& Text, const FColor& Color, float WorldSize)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABattleFloatingText* FloatingActor = World->SpawnActor<ABattleFloatingText>(ABattleFloatingText::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	if (FloatingActor)
	{
		FloatingActor->SetupFloatingText(Text, Color, WorldSize);
	}
	return FloatingActor;
}

void ABattleFloatingText::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	if (ElapsedTime >= LifeTime)
	{
		Destroy();
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

	// 动态缩放冲击: 前 0.2 秒从 PopStartScale 弹回 1.0x
	float ScaleFactor = 1.0f;
	if (ElapsedTime < 0.2f)
	{
		ScaleFactor = FMath::Lerp(PopStartScale, 1.0f, ElapsedTime / 0.2f);
	}

	// 淡出透明度: 后半段平滑降低 Alpha
	float Alpha = 1.0f;
	const float FadeStartTime = LifeTime * 0.55f;
	if (ElapsedTime > FadeStartTime)
	{
		Alpha = FMath::Clamp(1.0f - (ElapsedTime - FadeStartTime) / (LifeTime - FadeStartTime), 0.0f, 1.0f);
	}

	if (TextRender)
	{
		TextRender->SetWorldSize(BaseWorldSize * ScaleFactor);
		FColor CurrColor = BaseColor;
		CurrColor.A = static_cast<uint8>(Alpha * 255.0f);
		TextRender->SetTextRenderColor(CurrColor);
	}
}
