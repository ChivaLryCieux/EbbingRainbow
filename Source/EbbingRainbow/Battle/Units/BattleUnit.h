// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Battle/Core/BattleTypes.h"
#include "Battle/Skills/BattleSkillTypes.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "BattleUnit.generated.h"

class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAnimSequence;
class UAnimMontage;
class UPhysicsAsset;
class ABattleUnit;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitMoveFinished, ABattleUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDied, ABattleUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnitDamaged, ABattleUnit*, Unit, int32, Damage);

/**
 * 回合制战斗单位 (Manny 骨骼版)。
 * 持有 完整度/护盾/AP/先攻; 支持基于 NavMesh 的自由移动, 移动消耗 AP = 路径长度 / CmPerAP。
 *
 * 技能系统 2.0 (Docs/SkillSystem-2.0.md):
 *  - 技能实例/状态堆叠/派生属性全部委托给 UBattleSkillComponent
 *  - 结算走 UBattleResolver 管线 (命中/暴击/地形/反应表/事件)
 *  - 战斗随机数种子化 (FBattleRandomStream, 经 BattleManager)
 *
 * 表现层:
 *  - 脚底队伍光环底盘 (无资产依赖, 直接 MID 调色) 表达敌我/选中/悬停/当前回合
 *  - 攻击/受击/死亡均为一次性动画, 播完自动交还 ABP 运动混合
 *  - 技能伤害在动画命中时刻才结算 (延迟命中), 而不是按下即掉血
 *  - 阵亡: 先倒地动作, 再无缝切入 Chaos 布娃娃并沿受击方向击飞
 */
UCLASS()
class EBBINGRAINBOW_API ABattleUnit : public ACharacter
{
	GENERATED_BODY()

public:
	ABattleUnit();

	virtual void Tick(float DeltaSeconds) override;

	/** 初始化数值与阵营 (按 Stats.SkillIds 装备技能) */
	void InitUnit(const FBattleUnitStats& InStats, EBattleTeam InTeam);

	// ---------- 核心数值 (基础模板 + 运行时当前值) ----------
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	FBattleUnitStats Stats;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	EBattleTeam Team = EBattleTeam::Player;

	/** 当前完整度 (生命值, Float) */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	float Integrity = 0.0f;

	/** 临时护盾 / 屏障 (优先于完整度吸收伤害) */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	float Barrier = 0.0f;

	/** 当前行动点 (整型 AP) */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Stats")
	int32 AP = 0;

	/** 技能组件: 技能实例 + 状态堆叠 + 派生属性 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Skills")
	TObjectPtr<UBattleSkillComponent> SkillComp;

	UFUNCTION(BlueprintPure, Category = "Battle|Skills")
	UBattleSkillComponent* GetSkillComponent() const { return SkillComp; }

	/** 基础转化率系数: 1 点先攻值兑换的移动厘米数 (默认 25cm/点先攻) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Movement")
	float CmPerInitiativeUnit = 25.0f;

	/** 1 点 AP 可移动的距离 (cm): 由有效先攻值决定 (状态可改先攻) */
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

	/** 破坏护盾 (不足部分忽略, 不穿透到完整度) */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void DrainBarrier(float Amount);

	/**
	 * 结算后伤害落地 (抗性/命中判定已在 Resolver 完成):
	 * 护盾吸收 → 扣完整度 → 飘字/受击反馈 → 死亡判定。
	 * @return 总吸收量 (护盾 + 完整度)
	 */
	float ApplyDamageAmount(float DamageAmount, ABattleUnit* Attacker, EDamageType DamageType,
		bool bCrit, float* OutBarrierAbsorbed = nullptr, float* OutIntegrityDamage = nullptr);

	/** 治疗完整度 (钳制到有效上限) */
	void ApplyHeal(float Amount, ABattleUnit* Source);

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

	/** 技能实例列表 (UI/AI 统一索引) */
	const TArray<FBattleSkillSpec>& GetSkillSpecs() const { return SkillComp ? SkillComp->GetSkillSpecs() : EmptySkillSpecs; }

	/** 检查是否可对目标使用技能 (含冷却/视线/3D 射程) */
	bool CanUseSkill(int32 SkillIndex, const ABattleUnit* Target, FString& OutReason) const;

	/** 施放技能 (扣 AP → 动画 → 命中时刻经 UBattleResolver 结算) */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool ExecuteSkill(int32 SkillIndex, ABattleUnit* Target);

	/**
	 * 兼容入口: 直接伤害 (无命中判定, 仍过抗性/护盾)。
	 * 优先使用 UBattleResolver::ApplyDirectDamage。
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	float TakeBattleDamage(float DamageAmount, ABattleUnit* Attacker, EDamageType DamageType = EDamageType::Kinetic);

	/**
	 * 调试处决: 跳过闪避 / 抗性 / 护盾, 直接把完整度清零并走完整套死亡表现。
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void DebugKill();

	// ---------- 表现层 (Presentation) ----------

	/** 移动速度 (cm/s)。InitUnit 按先攻值写入, 高先攻单位跑得更快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Movement")
	float MoveSpeed = 650.0f;

	/** 动作命中时刻占动画时长的比例 (0~1)。0.45 ≈ 出拳刚打到人的瞬间 */
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

	/** 刷新头顶文字 (含状态摘要) */
	void RefreshLabel();

	/** 单位底部到场景地面的偏移 (胶囊半高) */
	float GetCapsuleHalfHeight() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 表现资产惰性加载 (骨骼网格/ABP/动画池), BeginPlay 调用一次 */
	void EnsurePresentationAssets();
	bool bPresentationReady = false;

	/** 头顶名牌朝向缓存: 镜头或自身未移动时跳过 SetWorldRotation */
	FVector LastLabelCamLoc = FVector::ZeroVector;
	FVector LastLabelSelfLoc = FVector::ZeroVector;

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

	/** 移动循环动画 (移动期间以循环蒙太奇播放在 DefaultSlot 上, 结束淡出回 ABP) */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> MoveAnimSequence;

	/** 移动循环蒙太奇实例 (惰性创建, LoopCount 给足由 StopMoveAnimation 负责停) */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> MoveMontage;

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

	// ---------- 延迟命中 (动画命中时刻结算) ----------
	TWeakObjectPtr<ABattleUnit> PendingHitTarget;
	TObjectPtr<UBattleSkillData> PendingSkillData = nullptr;
	int32 PendingSkillLevel = 1;
	FVector PendingTargetLocation = FVector::ZeroVector;
	/** 空技能规格的静态返回值 (无技能组件时兜底) */
	static const TArray<FBattleSkillSpec> EmptySkillSpecs;

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

	/** 开始/停止移动循环动画 (速度对齐播放速率) */
	void PlayMoveAnimation();
	void StopMoveAnimation();

	void ApplyPendingHit();
	void EnableRagdoll();
};