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
class UAnimSequence;
class UPhysicsAsset;
class ABattleUnit;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitMoveFinished, ABattleUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDied, ABattleUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnitDamaged, ABattleUnit*, Unit, int32, Damage);

/**
 * 回合制战斗单位 (Manny 骨骼版)。
 * 持有 完整度 / AP / 先攻; 支持基于 NavMesh 的自由移动, 移动消耗 AP = 路径长度 / CmPerAP。
 * 具备技能施放、受击掉血、死亡布娃娃处理。
 *
 * 表现层:
 *  - 脚底队伍光环底盘 (无资产依赖, 直接 MID 调色) 表达敌我/选中/悬停/当前回合
 *  - 攻击/受击/死亡均为一次性动画, 播完自动交还 ABP 运动混合
 *  - 技能伤害在攻击动画的命中时刻才结算 (延迟命中), 而不是按下即掉血
 *  - 阵亡: 先倒地动作, 再无缝切入 Chaos 布娃娃并沿受击方向击飞
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

	/** 是否已进入死亡表现流程 (倒地/布娃娃) */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsDying() const { return bDying; }

	/** 从 Run 的持久状态恢复完整度（跨战斗继承血量）。 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ApplyPersistentIntegrity(float InIntegrity);

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

	// ---------- 表现层 (Presentation) ----------

	/** 移动速度 (cm/s)。InitUnit 按先攻值写入, 高先攻单位跑得更快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Movement")
	float MoveSpeed = 650.0f;

	/** 攻击动作命中时刻占动画时长的比例 (0~1)。0.45 ≈ 出拳刚打到人的瞬间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Animation", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float AttackHitFraction = 0.45f;

	/** 阵亡后延迟多久切入布娃娃 (秒)。先播倒地动作, 过渡更自然 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RagdollBlendDelay = 0.30f;

	/** 布娃娃击飞强度系数 (每点伤害换算的瞬时速度) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Animation")
	float RagdollImpulsePerDamage = 42.0f;

	/**
	 * 播放一段一次性动画 (攻击/受击/施法), 播完自动交还 ABP 运动混合。
	 * 由技能资产与战斗逻辑共用, 避免各自处理恢复时机。
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Animation")
	void PlayOneShotAnimation(UAnimSequence* Animation);
	/** 播放攻击动作, 返回伤害应落地的延迟 (秒); 无动画资产时返回 0 (立即结算) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Animation")
	float PlayAttackAnimation();

	/** 按来袭方向播放受击反应 (正面/背面) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Animation")
	void PlayHitReaction(const FVector& FromWorldLocation);

	/** 阵亡表现: 倒地动作 -> 布娃娃 -> 沿受击方向击飞 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Animation")
	void StartDeathRagdoll(AActor* DamageCauser, float DamageAmount);

	/** 取消尚未落地的延迟伤害 (单位被打断/离场时) */
	UFUNCTION(BlueprintCallable, Category = "Battle|Animation")
	void CancelPendingHit();

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

	/** 脚底队伍光环底盘: 敌我识别 + 选中/悬停/当前回合高亮 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeamDisc;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiscMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SkelMeshMIDs;

	UPROPERTY(Transient)
	TObjectPtr<class UAnimSequence> AttackAnimSequence;

	// ---------- 动画资产 ----------

	/** 攻击动作池 (随机取一段, 避免连段重复感) */
	UPROPERTY(EditAnywhere, Category = "Battle|Animation")
	TArray<TObjectPtr<UAnimSequence>> AttackAnimations;

	/** 正面受击反应池 */
	UPROPERTY(EditAnywhere, Category = "Battle|Animation")
	TArray<TObjectPtr<UAnimSequence>> HitReactionFrontAnimations;

	/** 背面受击反应池 */
	UPROPERTY(EditAnywhere, Category = "Battle|Animation")
	TArray<TObjectPtr<UAnimSequence>> HitReactionBackAnimations;

	/** 死亡倒地动作池 */
	UPROPERTY(EditAnywhere, Category = "Battle|Animation")
	TArray<TObjectPtr<UAnimSequence>> DeathAnimations;

	/**
	 * 动画序列 -> 动态蒙太奇缓存。
	 * Manny 的受击/死亡动画是叠加动画, AnimSingleNodeInstance 不支持 (打包后失效),
	 * 必须包成 DefaultSlot 蒙太奇经 ABP 播放。
	 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UAnimSequence>, TObjectPtr<UAnimMontage>> MontageCache;

	FTimerHandle AttackAnimTimer;
	FTimerHandle OneShotAnimTimerHandle;
	FTimerHandle PendingHitTimerHandle;
	FTimerHandle RagdollTimerHandle;

	/** 延迟伤害: 攻击动画命中时刻才结算 */
	TWeakObjectPtr<ABattleUnit> PendingHitTarget;
	int32 PendingHitSkillIndex = INDEX_NONE;

	FVector PendingRagdollImpulse = FVector::ZeroVector;
	bool bRagdollActive = false;
	bool bDying = false;

	TArray<FVector> MovePath;
	int32 MovePathIndex = 0;
	bool bMoving = false;
	bool bSelected = false;
	bool bHovered = false;
	FTimerHandle MoveWatchdogTimer;

	// 光环表现状态
	float DiscFlashTimer = 0.0f;
	float DiscPulseTime = 0.0f;
	static constexpr float DiscFlashDuration = 0.22f;

	void FinishMove();
	void UpdateVisualColor();
	void UpdateDiscVisual(float DeltaSeconds);
	void TriggerDiscFlash();

	/** 队伍基色 (不含选中/悬停/闪烁修饰) */
	FLinearColor GetTeamBaseColor() const;

	void RestoreAnimBlueprint();

	/** 取 (或建) 一段动画对应的 DefaultSlot 动态蒙太奇 */
	UAnimMontage* GetOrCreateMontage(UAnimSequence* Animation);

	void ResolveSkillImpact(const FBattleSkillDef& Skill, ABattleUnit* Target);
	void ApplyPendingHit();
	void EnableRagdoll();
};
