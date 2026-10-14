// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Battle/Core/BattleTypes.h"
#include "BattleUnit.generated.h"

class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ABattleUnit;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitMoveFinished, ABattleUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDied, ABattleUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnitDamaged, ABattleUnit*, Unit, int32, Damage);

/**
 * 回合制战斗单位 (白模)。
 * 持有 HP / AP / Speed; 支持基于 NavMesh 的自由移动, 移动消耗 AP = 路径长度 / CmPerAP。
 * 具备技能施放、受击掉血、死亡处理。
 */
UCLASS()
class EBBINGRAINBOW_API ABattleUnit : public ACharacter
{
	GENERATED_BODY()

public:
	ABattleUnit();

	virtual void Tick(float DeltaSeconds) override;

	/** 初始化数值与阵营 */
	void InitUnit(const FBattleUnitStats& InStats, EBattleTeam InTeam);

	// ---------- 数值 ----------
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FBattleUnitStats Stats;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EBattleTeam Team = EBattleTeam::Player;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	int32 HP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	float AP = 0.0f;

	/** 1 点 AP 可移动的距离 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	float CmPerAP = 200.0f;

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsAlive() const { return HP > 0; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsMoving() const { return bMoving; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ResetAP();

	// ---------- 移动 ----------

	/**
	 * 规划到目标点的路径,并按剩余 AP 截断。
	 * @param OutPath    截断后的路径点 (含起点)
	 * @param OutCostAP  走完 OutPath 需要的 AP
	 * @param bOutTruncated 目标超出 AP 可达范围时为 true
	 * @return 是否存在有效路径
	 */
	bool PlanMove(const FVector& Destination, TArray<FVector>& OutPath, float& OutCostAP, bool& bOutTruncated) const;

	/** 规划并立即开始移动。成功开始返回 true,AP 在开始时一次性扣除 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool MoveTo(const FVector& Destination);

	/** 停止当前正在进行的移动并广播完成 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StopMovement();

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnUnitMoveFinished OnMoveFinished;

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnUnitDied OnUnitDied;

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnUnitDamaged OnUnitDamaged;

	// ---------- 技能与战斗 ----------

	/** 检查是否可对目标使用技能 */
	bool CanUseSkill(int32 SkillIndex, const ABattleUnit* Target, FString& OutReason) const;

	/** 施放技能 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool ExecuteSkill(int32 SkillIndex, ABattleUnit* Target);

	/** 受击并扣除 HP */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	int32 TakeBattleDamage(int32 DamageAmount, ABattleUnit* Attacker);

	// ---------- 视觉状态 ----------

	void SetSelected(bool bInSelected);
	void SetHovered(bool bInHovered);

	/** 刷新头顶文字 */
	void RefreshLabel();

	/** 单位底部到场景地面的偏移 (胶囊半高) */
	float GetCapsuleHalfHeight() const;

	const TArray<FBattleSkillDef>& GetSkills() const { return Stats.Skills; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SkelMeshMIDs;

	UPROPERTY(Transient)
	TObjectPtr<class UAnimSequence> AttackAnimSequence;

	FTimerHandle AttackAnimTimer;

	TArray<FVector> MovePath;
	int32 MovePathIndex = 0;
	bool bMoving = false;
	bool bSelected = false;
	bool bHovered = false;
	FTimerHandle MoveWatchdogTimer;

	void FinishMove();
	void UpdateVisualColor();
};
