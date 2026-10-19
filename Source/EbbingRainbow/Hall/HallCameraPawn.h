// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HallCameraPawn.generated.h"

class UCameraComponent;
class UArrowComponent;

/**
 * 空间站大厅展示镜头 Pawn:
 * 提供静谧的宇宙慢浮动动效与鼠标微视差 (Parallax) 倾斜, 让背景中的空间站与星球更具立体纵深感。
 * 支持在编辑器中指示朝向、画中画视锥体预览、自动对准空间站目标，以及视线偏置构图。
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

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UArrowComponent> DirectionArrow;
#endif

	FRotator BaseRotation = FRotator::ZeroRotator;
	FVector BaseLocation = FVector::ZeroVector;
	float RunningTime = 0.0f;
};
