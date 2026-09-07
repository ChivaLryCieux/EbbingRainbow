// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MotorcyclePawn.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UInputAction;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMotorcycleOccupantEvent, ACharacter*, Rider);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMotorcycleLandedEvent, float, AirTime, float, ImpactVelocity);

/**
 * 摩托车高品质载具 Pawn
 * 支持：
 * - 靠近按 F 上车，就座于特定锚点，并锁定角色坐标与姿势
 * - 高速行驶、平滑油门、转向衰减、入弯真实车身倾斜 (Lean/Roll)
 * - 前后轮旋转与转向车把偏转驱动
 * - 地面悬挂射线检测与坡度法线贴合
 * - 再次按 F 安全下车避障落地
 * - 同时兼容 Enhanced Input 资产与原生键位双保险
 */
UCLASS()
class EBBINGRAINBOW_API AMotorcyclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMotorcyclePawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;

	// -------------------------------------------------------------
	// 组件定义
	// -------------------------------------------------------------
protected:
	/** 摩托车主体碰撞盒 (Root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** 车身主体网格体 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChassisMesh;

	/** 车把与前叉转向轴 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SteeringPivot;

	/** 车把网格体 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HandlebarsMesh;

	/** 前轮网格体 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrontWheelMesh;

	/** 后轮网格体 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RearWheelMesh;

	/** 骑手座位锚点 (决定骑手坐姿的世界/相对坐标) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SeatPoint;

	/** 左侧下车安全点 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> DismountPointLeft;

	/** 右侧下车安全点 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> DismountPointRight;

	/** 玩家接近交互检测球 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** 载具第三人称弹簧臂 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 载具跟随相机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 前照大灯 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Headlight;

	// -------------------------------------------------------------
	// 驾驶物理与手感配置
	// -------------------------------------------------------------
protected:
	/** 最大前进速度 (cm/s，默认 2400 约为 86.4 km/h，远快于角色奔跑速度 900) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float MaxForwardSpeed = 2400.0f;

	/** 最大倒车速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float MaxReverseSpeed = 500.0f;

	/** 加速度 (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float AccelerationRate = 1800.0f;

	/** 刹车制动力减速度 (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float BrakingDeceleration = 2800.0f;

	/** 空挡滑行自然阻力减速度 (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float CoastingDeceleration = 400.0f;

	/** 手刹制动力减速度 (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float HandbrakeDeceleration = 3600.0f;

	/** 低速最大转向角度 (度) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float MaxSteerAngle = 40.0f;

	/** 高速最大转向角度 (高速防侧滑失控自适应降角度) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float HighSpeedSteerAngle = 15.0f;

	/** 转向转弯基础角速度 (度/秒) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float TurnRate = 85.0f;

	/** 入弯最大压弯倾斜角度 (Lean/Roll Angle, 度) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float MaxLeanAngle = 28.0f;

	/** 压弯倾斜回正平滑插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float LeanInterpSpeed = 8.0f;

	/** 悬挂离地目标高度 (Ride Height, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float RideHeight = 48.0f;

	/** 能够平滑直接开过去的石头/台阶/路肩最大越障高度 (cm, 默认 40.0cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float MaxStepUpHeight = 40.0f;

	/** 悬挂地面检测射线长度 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float SuspensionTraceLength = 85.0f;

	/** 车轮物理有效半径 (cm, 用于精确计算转速滚圈动画) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float WheelRadius = 32.0f;

	/** 重力加速度 (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics")
	float GravityZ = -980.0f;

	/** 悬挂最大下行程 (cm，探地距离超出 RideHeight + 此值即视为悬空) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float MaxSuspensionExtension = 22.0f;

	/** 冲坡离地冲量倍率 (1.0~1.3，赋予飞跃爽快感) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float JumpLaunchMultiplier = 1.15f;

	/** 触发冲坡起飞的最小向上攀升垂直分速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float MinLaunchClimbSpeed = 80.0f;

	/** 滞空中车身俯仰对齐飞行抛物线弧度的平滑插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float AirPitchInterpSpeed = 3.5f;

	/** 滞空中空中转向偏航控制权比例 (0.0~1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float AirControlRatio = 0.28f;

	/** 滞空中油门/刹车微调车身抬头/压头角速度 (度/秒) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float AirPitchControlRate = 35.0f;

	/** 滞空风阻自然减速度 (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Physics|Jump")
	float AirDragDeceleration = 60.0f;

	/** 角色在座位上的相对偏移位置微调 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Rider")
	FVector RiderRelativeLocationOffset = FVector(-5.0f, 0.0f, 15.0f);

	/** 角色在座位上的相对朝向微调 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Rider")
	FRotator RiderRelativeRotationOffset = FRotator(0.0f, 0.0f, 0.0f);

	// -------------------------------------------------------------
	// 摄像机镜头配置
	// -------------------------------------------------------------
protected:
	/** 基础摄像机距离 (低速或停驻时，默认 680.0，明显远于角色步行的 400.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Camera")
	float BaseCameraDistance = 680.0f;

	/** 高速飞驰时的摄像机距离 (动态自适应拉远，速度越快拉得越远) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Camera")
	float HighSpeedCameraDistance = 900.0f;

	/** 镜头随速度拉远与收缩的平滑插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Camera")
	float CameraZoomInterpSpeed = 4.0f;

	/** 是否开启摄像机碰撞探测 (载具中默认设为 false，彻底杜绝镜头被车身或骑手顶入体内) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Camera")
	bool bEnableCameraCollision = false;

	// -------------------------------------------------------------
	// Enhanced Input 资产指针 (可选配置)
	// -------------------------------------------------------------
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Input")
	TObjectPtr<UInputAction> HandbrakeAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motorcycle|Input")
	TObjectPtr<UInputAction> DismountAction;

	// -------------------------------------------------------------
	// 运行时运行状态
	// -------------------------------------------------------------
protected:
	/** 当前标量线速度 (cm/s, 正为前进，负为倒车) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	float CurrentSpeed = 0.0f;

	/** 当前油门输入 [-1.0, 1.0] */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	float ThrottleInput = 0.0f;

	/** 当前转向输入 [-1.0, 1.0] */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	float SteerInput = 0.0f;

	/** 手刹是否生效中 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	bool bHandbrake = false;

	/** 当前车身压弯横滚角度 (Roll) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	float CurrentLeanRoll = 0.0f;

	/** 车轮当前累计旋转角 (度) */
	float WheelRotationAngle = 0.0f;

	/** 垂直速度 (滞空自由落体用) */
	float VerticalVelocity = 0.0f;

	/** 是否正接触地面 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	bool bIsOnGround = true;

	/** 是否正处于腾空飞跃状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	bool bIsAirborne = false;

	/** 当前腾空滞空累计时长 (秒) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	float AirborneDuration = 0.0f;

	/** 当前骑乘者 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	TWeakObjectPtr<ACharacter> CurrentRider = nullptr;

	/** 记录进入交互范围的角色 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motorcycle|State")
	TWeakObjectPtr<ACharacter> NearbyCharacter = nullptr;

	// -------------------------------------------------------------
	// 事件委托
	// -------------------------------------------------------------
public:
	/** 骑手就坐上车事件 */
	UPROPERTY(BlueprintAssignable, Category = "Motorcycle|Events")
	FOnMotorcycleOccupantEvent OnRiderMounted;

	/** 骑手离开下车事件 */
	UPROPERTY(BlueprintAssignable, Category = "Motorcycle|Events")
	FOnMotorcycleOccupantEvent OnRiderDismounted;

	/** 摩托车腾空落地着陆事件 (参数: 滞空总时长 秒, 触地冲击垂直速度 cm/s) */
	UPROPERTY(BlueprintAssignable, Category = "Motorcycle|Events")
	FOnMotorcycleLandedEvent OnMotorcycleLanded;

	// -------------------------------------------------------------
	// 乘车 / 下车 API
	// -------------------------------------------------------------
public:
	/** 当前是否可上车 */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Interaction")
	bool CanMount(ACharacter* InCharacter) const;

	/** 执行骑手上车 */
	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Interaction")
	bool MountRider(ACharacter* InCharacter);

	/** 执行骑手下车 */
	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Interaction")
	bool DismountRider();

	/** 是否已被占用骑乘 */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Interaction")
	bool IsOccupied() const { return CurrentRider.IsValid(); }

	/** 获取当前骑手 */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Interaction")
	ACharacter* GetCurrentRider() const { return CurrentRider.Get(); }

	/** 获取当前车速 (cm/s) */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Physics")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	/** 获取当前车速 (km/h) */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Physics")
	float GetCurrentSpeedKmH() const { return (CurrentSpeed * 3600.0f) / 100000.0f; }

	/** 是否正处于腾空飞跃状态 */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Physics")
	bool IsAirborne() const { return bIsAirborne; }

	/** 获取当前腾空已滞空时长 (秒) */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Physics")
	float GetAirborneDuration() const { return AirborneDuration; }

	/** 获取当前垂直速度 (cm/s, 正为上升，负为下落) */
	UFUNCTION(BlueprintPure, Category = "Motorcycle|Physics")
	float GetVerticalVelocity() const { return VerticalVelocity; }

	// -------------------------------------------------------------
	// 操控输入接口 (供按键或 Enhanced Input 绑定)
	// -------------------------------------------------------------
public:
	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Input")
	void SetThrottleInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Input")
	void SetSteerInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Input")
	void SetHandbrakeInput(bool bPressed);

	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Input")
	void LookUp(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Input")
	void Turn(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motorcycle|Input")
	void InputDismount();

	// Enhanced Input 响应包装
	void OnEnhancedThrottle(const FInputActionValue& Value);
	void OnEnhancedSteer(const FInputActionValue& Value);
	void OnEnhancedLook(const FInputActionValue& Value);
	void OnEnhancedHandbrake(const FInputActionValue& Value);
	void OnEnhancedDismount(const FInputActionValue& Value);

	// -------------------------------------------------------------
	// 内部物理与辅助计算
	// -------------------------------------------------------------
protected:
	virtual void BeginPlay() override;

	/** 计算模拟更新 */
	void SimulateMotorcyclePhysics(float DeltaTime);

	/** 探测安全下车落地坐标 */
	bool FindSafeDismountTransform(FVector& OutLocation, FRotator& OutRotation) const;

	/** 交互感应球重叠事件 */
	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
