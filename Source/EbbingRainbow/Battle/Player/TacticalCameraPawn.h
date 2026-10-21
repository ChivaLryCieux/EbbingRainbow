// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TacticalCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * 战术俯视摄像机。
 * WASD / 屏幕边缘平移,Q/E 旋转,滚轮缩放;支持平滑聚焦到指定位置并限制在竞技场范围内。
 * 输入通过 BindKey/按键轮询实现,不依赖 InputAction 资产。
 */
UCLASS()
class EBBINGRAINBOW_API ATacticalCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ATacticalCameraPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 平滑移动到指定世界位置 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FocusOn(const FVector& WorldLocation);

	/** 立即跳转 (无插值) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SnapTo(const FVector& WorldLocation);

	/** 设置平移范围 (XY) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetMoveBounds(const FBox& InBounds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PanSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float RotateSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomStep = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinArmLength = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxArmLength = 9000.0f;

	/** 中键轨道旋转：水平拖动每像素转多少度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float OrbitYawSpeed = 0.22f;

	/** 中键轨道旋转：垂直拖动每像素改多少俯仰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float OrbitPitchSpeed = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinPitch = -85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxPitch = -18.0f;

	/** 屏幕边缘滚动 (像素宽度,0 关闭) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float EdgeScrollMargin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FocusInterpSpeed = 5.0f;

protected:
	virtual void BeginPlay() override;

private:
	void ZoomIn();
	void ZoomOut();
	void StartOrbit();
	void StopOrbit();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	FVector TargetLocation = FVector::ZeroVector;
	float TargetArmLength = 4200.0f;
	float TargetYaw = 45.0f;
	float TargetPitch = -60.0f;
	bool bOrbiting = false;
	FBox MoveBounds = FBox(ForceInit);
};
