// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Experimental/LegionBattle/FPS/ShooterWeaponHolder.h"
#include "LegionCommanderCharacter.generated.h"

class AShooterWeapon;
class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UPawnNoiseEmitterComponent;
class UAnimMontage;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);

/**
 * 军团战场指挥官化身 (FP 模板第一人称射击栈):
 *
 *  合并自 UE FP 模板 AFPCharacter + AShooterCharacter:
 *  - 手臂网格 + head-socket 相机 (第一人称 FOV/缩放通道) 与 Enhanced Input;
 *  - IShooterWeaponHolder 武器库存, BeginPlay 按 StartingWeaponClasses 发放武器 (默认手枪);
 *  - 组件/属性名与模板 C++ 完全一致, BP_ShooterCharacter 等资产经
 *    DefaultEngine.ini 的 ActiveClassRedirects (/Script/FP.* -> 本类) 直接复用。
 *
 *  军团侧追加:
 *  - Shift 疾跑 (IA_Sprint -> SetSprinting);
 *  - Tick 内把活动范围钳在寻路网格覆盖的战场内 (保证等高线地图覆盖玩家);
 *  - DoStartFiring 开火抑制: 战略地图开着 / 光标呼出 / 准星指向我方兵团时不打;
 *  - TakeDamage/Die 表面完整保留 (目前无人对指挥官造成伤害)。
 */
UCLASS()
class EBBINGRAINBOW_API ALegionCommanderCharacter : public ACharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** AI Noise emitter component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPawnNoiseEmitterComponent* PawnNoiseEmitter;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MouseLookAction;

	/** Fire weapon input action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* FireAction;

	/** Switch weapon input action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SwitchWeaponAction;

	/** 疾跑输入动作 (IA_Sprint, ER 侧追加) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SprintAction;

	/** Name of the first person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	/** Name of the third person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	/** 出生时发放的武器 (软引用, 默认 BP_ShooterWeapon_Pistol) */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	TArray<TSoftClassPtr<AShooterWeapon>> StartingWeaponClasses;

	/** Max distance to use for aim traces */
	UPROPERTY(EditAnywhere, Category ="Aim", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	/** Max HP this character can have */
	UPROPERTY(EditAnywhere, Category="Health")
	float MaxHP = 500.0f;

	/** Current HP remaining to this character */
	float CurrentHP = 0.0f;

	/** Team ID for this character*/
	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 0;

	/** Actor tag to grant this character when it dies */
	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	/** Tag to pass to weapons and projectiles to identify their AI perception noise as player-generated */
	UPROPERTY(EditAnywhere, Category="Tags")
	FName PlayerTag = FName("Player");

	/** List of weapons picked up by the character */
	TArray<AShooterWeapon*> OwnedWeapons;

	/** Weapon currently equipped and ready to shoot with */
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	UPROPERTY(EditAnywhere, Category ="Destruction", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 5.0f;

	FTimerHandle RespawnTimer;

	// ---- 军团侧 ----

	/** 步行速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float WalkSpeed = 780.0f;

	/** 疾跑速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float SprintSpeed = 1900.0f;

	/** 战场边缘留白 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float BoundsMargin = 600.0f;

public:

	/** Bullet count updated delegate */
	FBulletCountUpdatedDelegate OnBulletCountUpdated;

	/** Damaged delegate */
	FDamagedDelegate OnDamaged;

public:

	/** Constructor */
	ALegionCommanderCharacter();

	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	/** Handle incoming damage */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Handles start firing input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	/** Handles stop firing input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStopFiring();

	/** Handles switch weapon input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSwitchWeapon();

	/** 疾跑起止 (IA_Sprint Started/Completed) */
	void DoSprintStart() { SetSprinting(true); }
	void DoSprintEnd() { SetSprinting(false); }

public:

	//~Begin IShooterWeaponHolder interface

	/** Attaches a weapon's meshes to the owner */
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	/** Plays the firing montage for the weapon */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** Applies weapon recoil to the owner */
	virtual void AddWeaponRecoil(float Recoil) override;

	/** Updates the weapon's HUD with the current ammo count */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	/** Calculates and returns the aim location for the weapon */
	virtual FVector GetWeaponTargetLocation() override;

	/** Gives a weapon of this class to the owner */
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	/** Activates the passed weapon */
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	/** Deactivates the passed weapon */
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	/** Notifies the owner that the weapon cooldown has expired and it's ready to shoot again */
	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

	// ---- 军团侧 ----

	/** Shift 疾跑开关 (IA_Sprint Started/Completed 驱动) */
	void SetSprinting(bool bSprint);

	/** 打一发手枪, 供控制台 `ce TestFire` 验证开火链路 */
	UFUNCTION(Exec)
	void TestFire();

	/** 当前装备的武器 (HUD/调试用) */
	UFUNCTION(BlueprintPure, Category = "Commander")
	AShooterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

protected:

	/** Returns true if the character already owns a weapon of the given class */
	AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;

	/** 军团侧开火许可: 地图开着 / 光标呼出 / 结算锁定时不打 */
	bool CanFireWeapon() const;

	/** 接驳屏或战终结算锁定期间彻底屏蔽第一人称移动/转向/跳跃/疾跑/切枪 */
	bool IsCommanderInputBlocked() const;

	/** Called when this character's HP is depleted */
	void Die();

	/** Called to allow Blueprint code to react to this character's death */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	/** Called from the respawn timer to destroy this character and force the PC to respawn */
	void OnRespawn();

public:

	/** Returns true if the character is dead */
	bool IsDead() const;

	/** Sets the team ID for this character */
	void SetTeam(uint8 Team);

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
};
