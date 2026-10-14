// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HallCameraPawn.generated.h"

class UCameraComponent;

/**
 * 空间站大厅展示镜头 Pawn:
 * 提供静谧的宇宙慢浮动动效与鼠标微视差 (Parallax) 倾斜, 让背景中的空间站与星球更具立体纵深感。
 */
UCLASS()
class EBBINGRAINBOW_API AHallCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AHallCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	FRotator BaseRotation = FRotator::ZeroRotator;
	FVector BaseLocation = FVector::ZeroVector;
	float RunningTime = 0.0f;
};
