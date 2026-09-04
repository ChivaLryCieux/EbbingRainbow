// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "EbbingRainbowCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/** 开放世界运动状态枚举 */
UENUM(BlueprintType)
enum class EOpenWorldMovementMode : uint8
{
	Walking UMETA(DisplayName = "Walking/Running"),
	Sprinting UMETA(DisplayName = "Sprinting"),
	Falling UMETA(DisplayName = "Falling"),
	Gliding UMETA(DisplayName = "Gliding"),
	Climbing UMETA(DisplayName = "Climbing")
};

/**
 *  A player-controllable third person character with Open World 3C mechanics:
 *  - Move and Look with Trajectory tracking
 *  - Jump, Fall, and Space-to-Glide while in air
 *  - Hold Shift to Sprint
 *  - Wall detection, climbing in 2D surface plane, and Wall Jump
 */
UCLASS(abstract)
class AEbbingRainbowCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Sprint Input Action (Hold Shift to sprint) */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SprintAction;

public:

	/** Constructor */
	AEbbingRainbowCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces (Jumping / Gliding / Wall Jump) */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	// -------------------------------------------------------------
	// 开放世界 3C 功能：疾跑 (Sprint)
	// -------------------------------------------------------------
public:
	/** 开始按住疾跑 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Movement")
	void StartSprint();

	/** 结束疾跑 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Movement")
	void StopSprint();

	/** 是否正在疾跑 */
	UFUNCTION(BlueprintPure, Category="OpenWorld|Movement")
	bool IsSprinting() const { return bIsSprinting; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Movement")
	float JogSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Movement")
	float SprintSpeed = 900.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="OpenWorld|Movement")
	bool bIsSprinting = false;

	// -------------------------------------------------------------
	// 开放世界 3C 功能：滑翔 (Gliding)
	// -------------------------------------------------------------
public:
	/** 开启滑翔 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Gliding")
	void StartGliding();

	/** 停止滑翔 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Gliding")
	void StopGliding();

	/** 当前是否可以进入滑翔 */
	UFUNCTION(BlueprintPure, Category="OpenWorld|Gliding")
	bool CanGlide() const;

	/** 是否正在滑翔 */
	UFUNCTION(BlueprintPure, Category="OpenWorld|Gliding")
	bool IsGliding() const { return bIsGliding; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="OpenWorld|Gliding")
	bool bIsGliding = false;

	/** 滑翔/缓降时的重力缩放 (接近漂浮) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Gliding")
	float GlideGravityScale = 0.15f;

	/** 缓降状态下的最大下沉速度 (负值，如 -150 cm/s，提供平缓下落浮力) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Gliding")
	float GlideMaxSinkSpeed = -150.0f;

	/** 缓降状态下按住 WASD 时的水平平移移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Gliding")
	float GlideHorizontalMoveSpeed = 500.0f;

	/** 缓降状态下松开 WASD 时水平制动减速阻力 (越大刹车越快) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Gliding")
	float GlideBrakingFriction = 8.0f;

	/** 能够触发滑翔的最小离地高度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Gliding")
	float MinGlideHeightAboveGround = 120.0f;

	/** 滑翔时的空中控制力 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Gliding")
	float GlideAirControl = 1.0f;

	/** 默认重力缩放缓存 */
	float DefaultGravityScale = 1.0f;

	/** 默认空中控制力缓存 */
	float DefaultAirControl = 0.35f;

	// -------------------------------------------------------------
	// 开放世界 3C 功能：爬墙 (Climbing)
	// -------------------------------------------------------------
public:
	/** 检测前方是否有可攀爬墙体 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Climbing")
	bool CheckClimbableWall(FHitResult& OutHit);

	/** 开始爬墙 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Climbing")
	void StartClimbing(const FHitResult& WallHit);

	/** 退出爬墙 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Climbing")
	void StopClimbing();

	/** 执行蹬墙跳 */
	UFUNCTION(BlueprintCallable, Category="OpenWorld|Climbing")
	void PerformWallJump();

	/** 是否正在爬墙 */
	UFUNCTION(BlueprintPure, Category="OpenWorld|Climbing")
	bool IsClimbing() const { return bIsClimbing; }

	/** 获取当前爬墙面的法线 */
	UFUNCTION(BlueprintPure, Category="OpenWorld|Climbing")
	FVector GetWallNormal() const { return WallNormal; }

protected:
	/** 攀爬中的物理/朝向更新 */
	void UpdateClimbingMovement(float DeltaTime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="OpenWorld|Climbing")
	bool bIsClimbing = false;

	/** 爬墙移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Climbing")
	float ClimbSpeed = 250.0f;

	/** 前方墙体探测距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Climbing")
	float ClimbTraceDistance = 75.0f;

	/** 探测胶囊/球体半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Climbing")
	float ClimbTraceRadius = 35.0f;

	/** 贴墙维持间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Climbing")
	float WallOffset = 45.0f;

	/** 蹬墙跳水平反推力 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Climbing")
	float WallJumpHorizontalImpulse = 650.0f;

	/** 蹬墙跳垂直弹跳力 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenWorld|Climbing")
	float WallJumpVerticalImpulse = 650.0f;

	/** 当前攀爬墙体法线 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="OpenWorld|Climbing")
	FVector WallNormal = FVector::ZeroVector;

	/** 当前墙面接触点位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="OpenWorld|Climbing")
	FVector WallLocation = FVector::ZeroVector;

	// -------------------------------------------------------------
	// 开放世界 3C 功能：手枪瞄准与掏枪 (Pistol Aiming)
	// -------------------------------------------------------------
public:
	/** 按住右键开始掏枪瞄准 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void StartAiming();

	/** 松开右键收枪取消瞄准 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void StopAiming();

	/** 是否正在瞄准 */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsAiming() const { return bIsAiming; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	bool bIsAiming = false;

	// -------------------------------------------------------------
	// 综合状态与蓝图事件通知
	// -------------------------------------------------------------
public:
	/** 获取当前综合移动状态 */
	UFUNCTION(BlueprintPure, Category="OpenWorld|Movement")
	EOpenWorldMovementMode GetOpenWorldMovementMode() const;

	/** 移动状态改变通知事件，供动画蓝图或UI扩展绑定 */
	UFUNCTION(BlueprintImplementableEvent, Category="OpenWorld|Events")
	void OnOpenWorldMovementModeChanged(EOpenWorldMovementMode PreviousMode, EOpenWorldMovementMode NewMode);

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

