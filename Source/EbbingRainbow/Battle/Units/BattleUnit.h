// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Battle/Core/BattleTypes.h"
#include "Battle/Skills/BattleSkillTypes.h"
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

	// ---------- 核心数值 (8大属性与运行时状态) ----------
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	FBattleUnitStats Stats;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	EBattleTeam Team = EBattleTeam::Player;

	/** 当前完整度 (生命值，Float) */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	float Integrity = 0.0f;

	/** 临时护盾 / 屏障 (优先于完整度吸收伤害) */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	float Barrier = 0.0f;

	/** 当前行动点 (整型 AP) */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	int32 AP = 0;

	/** 兼容旧代码的整数 HP 视图 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	int32 HP = 0;

	/** 基础转化率系数: 1 点先攻值兑换的移动厘米数 (默认 25cm/点先攻) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Movement")
	float CmPerInitiativeUnit = 25.0f;

	/** 1 点 AP 可移动的距离 (cm): 由先攻值与转化系数决定 */
	UFUNCTION(BlueprintPure, Category = "Battle|Movement")
	float GetCmPerAP() const;

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsAlive() const { return Integrity > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsMoving() const { return bMoving; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ResetAP();

	/** 赋予临时护盾 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void AddBarrier(float Amount);

	/** 应用复合属性修饰包 (一次性影响多种属性，带浮动文字反馈) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Attributes")
	void ApplyModifierBundle(const FAttributeModifierBundle& Bundle, ABattleUnit* InInstigator = nullptr, EDamageType DamageType = EDamageType::Kinetic);

	// ---------- 移动 ----------

	/**
	 * 规划到目标点的路径, 并按剩余 AP 截断。
	 * @param OutPath       截断后的路径点 (含起点)
	 * @param OutCostAP     走完 OutPath 需要的整型 AP (向上取整)
	 * @param bOutTruncated 目标超出 AP 可达范围时为 true
	 * @return 是否存在有效路径
	 */
	bool PlanMove(const FVector& Destination, TArray<FVector>& OutPath, int32& OutCostAP, bool& bOutTruncated) const;

	/** 兼容旧浮点引用的 PlanMove 重载 */
	bool PlanMove(const FVector& Destination, TArray<FVector>& OutPath, float& OutCostAP, bool& bOutTruncated) const;

	/** 规划并立即开始移动。成功开始返回 true, AP 在开始时一次性扣除 */
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

	/** 已装备的技能数据资产 (若存在则优先执行) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Skills")
	TArray<TObjectPtr<class UBattleSkillData>> EquippedSkills;

	/** 动态装备技能资产 (自动同步至 UI 技能栏) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Skills")
	void EquipSkillData(UBattleSkillData* InSkillData);

	/** 检查是否可对目标使用技能 */
	bool CanUseSkill(int32 SkillIndex, const ABattleUnit* Target, FString& OutReason) const;

	/** 施放技能 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool ExecuteSkill(int32 SkillIndex, ABattleUnit* Target);

	/** 
	 * 受击并扣除护盾与完整度
	 * 结算流程: 1.闪避判定 -> 2.对应抗性减免 -> 3.护盾吸收 -> 4.扣除完整度 -> 5.死亡判定
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	float TakeBattleDamage(float DamageAmount, ABattleUnit* Attacker, EDamageType DamageType = EDamageType::Kinetic);

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
