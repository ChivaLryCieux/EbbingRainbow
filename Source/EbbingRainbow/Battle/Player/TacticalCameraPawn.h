// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TacticalCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * 战术俯视摄像机（博德之门 3 风格手感优化版）。
 *
 * 控制说明：
 * - 鼠标中键按住拖动：博德之门 3 式轨道旋转（Yaw 绕聚焦点旋转，Pitch 俯仰微调，鼠标在旋转期间隐藏并锁定回位）。
 *   旋转读取引擎输入系统的每帧鼠标增量（MouseX/MouseY），不依赖光标绝对坐标、不做屏幕边缘传送，
 *   在视口捕获 / DPI 缩放 / 多显示器 / PIE 等任意状态下都不会产生“视角不停旋转”的巨量假增量。
 * - 滚轮上下滚动：相机平滑缩放，并可选支持 BG3 特色的“近距离自动仰角放平、远距离高俯视俯瞰”动态联动
 * - WASD / 屏幕边缘：平移相机
 * - Q / E：键盘平滑旋转视角
 * - F 键：聚焦当前行动单位
 *
 * 所有手感与上下限参数均在细节面板（Details Panel）中分组暴露并提供滑块限制。
 */
UCLASS()
class EBBINGRAINBOW_API ATacticalCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ATacticalCameraPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** 平滑移动到指定世界位置 */
	UFUNCTION(BlueprintCallable, Category = "Tactical Camera|Movement")
	void FocusOn(const FVector& WorldLocation);

	/** 立即跳转 (无插值) */
	UFUNCTION(BlueprintCallable, Category = "Tactical Camera|Movement")
	void SnapTo(const FVector& WorldLocation);

	/** 设置平移范围 (XY) */
	UFUNCTION(BlueprintCallable, Category = "Tactical Camera|Movement")
	void SetMoveBounds(const FBox& InBounds);

	// ==========================================
	// 缩放设置 (Zoom Settings)
	// ==========================================

	/** 初始臂长距离（开局默认视距） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (ClampMin = "200.0", ClampMax = "10000.0", UIMin = "300.0", UIMax = "5000.0"))
	float DefaultArmLength = 2000.0f;

	/** 镜头最小缩放距离（拉到最近时的特写距离） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (ClampMin = "100.0", ClampMax = "5000.0", UIMin = "200.0", UIMax = "2000.0"))
	float MinArmLength = 400.0f;

	/** 镜头最大缩放距离（拉到最远时的大局鸟瞰距离） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (ClampMin = "500.0", ClampMax = "20000.0", UIMin = "1500.0", UIMax = "8000.0"))
	float MaxArmLength = 3600.0f;

	/** 每次滚轮单格缩放步长 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (ClampMin = "20.0", ClampMax = "1000.0", UIMin = "50.0", UIMax = "500.0"))
	float ZoomStep = 180.0f;

	/** 缩放插值平滑速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (ClampMin = "1.0", ClampMax = "30.0", UIMin = "2.0", UIMax = "20.0"))
	float ZoomInterpSpeed = 10.0f;

	/** 是否开启博德之门3式缩放与俯仰角联动（拉近时相机自动抬平特写，拉远时自动高俯视） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom")
	bool bEnableZoomPitchCoupling = true;

	/** 联动模式下拉到最近时的俯仰角（负数，接近水平如 -25°） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (EditCondition = "bEnableZoomPitchCoupling", ClampMin = "-89.0", ClampMax = "-5.0", UIMin = "-60.0", UIMax = "-15.0"))
	float ClosePitch = -28.0f;

	/** 联动模式下拉到最远时的俯仰角（负数，高俯视如 -62°） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Zoom", meta = (EditCondition = "bEnableZoomPitchCoupling", ClampMin = "-89.0", ClampMax = "-5.0", UIMin = "-85.0", UIMax = "-30.0"))
	float FarPitch = -62.0f;

	// ==========================================
	// 旋转设置 (Orbit & Rotation Settings)
	// ==========================================

	/** 中键水平旋转灵敏度（每像素偏航转动角度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit", meta = (ClampMin = "0.01", ClampMax = "3.0", UIMin = "0.05", UIMax = "1.0"))
	float OrbitYawSpeed = 0.35f;

	/** 中键垂直旋转灵敏度（每像素俯仰角度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit", meta = (ClampMin = "0.01", ClampMax = "3.0", UIMin = "0.05", UIMax = "1.0"))
	float OrbitPitchSpeed = 0.20f;

	/** 是否反转中键俯仰方向 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit")
	bool bInvertOrbitPitch = false;

	/** 旋转平滑插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit", meta = (ClampMin = "1.0", ClampMax = "40.0", UIMin = "3.0", UIMax = "25.0"))
	float RotateInterpSpeed = 12.0f;

	/** Q/E 键键盘旋转速度 (度/秒) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit", meta = (ClampMin = "10.0", ClampMax = "360.0", UIMin = "30.0", UIMax = "180.0"))
	float RotateSpeed = 90.0f;

	/** 允许的最小俯仰角度（垂直低头最大极限，负数） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit", meta = (ClampMin = "-89.0", ClampMax = "-10.0", UIMin = "-85.0", UIMax = "-30.0"))
	float MinPitch = -85.0f;

	/** 允许的最大俯仰角度（垂直抬头最大极限，负数） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit", meta = (ClampMin = "-89.0", ClampMax = "-5.0", UIMin = "-50.0", UIMax = "-10.0"))
	float MaxPitch = -15.0f;

	/** 初始偏航角 (Yaw) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit")
	float DefaultYaw = 45.0f;

	/** 初始俯仰角 (Pitch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Orbit")
	float DefaultPitch = -55.0f;

	// ==========================================
	// 平移与聚焦设置 (Movement & Focus Settings)
	// ==========================================

	/** 基础平移移动速度 (WASD) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Movement", meta = (ClampMin = "200.0", ClampMax = "10000.0", UIMin = "500.0", UIMax = "5000.0"))
	float PanSpeed = 2200.0f;

	/** 屏幕边缘滚动边距 (像素宽度, 0 为关闭) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Movement", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "50.0"))
	float EdgeScrollMargin = 0.0f;

	/** 视角聚焦插值速度 (F键聚焦单位或事件聚焦) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Camera|Movement", meta = (ClampMin = "1.0", ClampMax = "30.0", UIMin = "2.0", UIMax = "15.0"))
	float FocusInterpSpeed = 6.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;

private:
	void ZoomIn();
	void ZoomOut();
	void StartOrbit();
	void StopOrbit();
	float GetCoupledPitchForArmLength(float InArmLength) const;

	// 兜底绝对坐标差分的单帧位移上限（像素）：正常甩动不会超过，捕获切换瞬间的坐标跳变会被丢弃
	static constexpr float MaxOrbitFallbackDeltaPixels = 300.0f;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	FVector TargetLocation = FVector::ZeroVector;
	float TargetArmLength = 2000.0f;
	float TargetYaw = 45.0f;
	float TargetPitch = -55.0f;
	float ManualPitchOffset = 0.0f;
	bool bOrbiting = false;
	FVector2D InitialOrbitMousePos = FVector2D::ZeroVector;
	FVector2D LastMousePos = FVector2D::ZeroVector;
	FBox MoveBounds = FBox(ForceInit);
};
