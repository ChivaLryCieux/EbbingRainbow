// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Experimental/LegionBattle/LegionTypes.h"
#include "LegionUnit.generated.h"

class FLegionTerrainGrid;
class ALegionGameMode;
class UAnimationAsset;
class UBlendSpace;
class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 虹膜链接大兵团单位 (TABS / Bad North 混合架构):
 *  - 单 Actor 管理整支兵团 (10~150+ 士兵), 支援多兵种克制、正面朝向格挡、
 *    地形网格绕山寻路、高低差加成、击退与主动 AI 超频协议。
 */
UCLASS(BlueprintType)
class EBBINGRAINBOW_API ALegionUnit : public AActor
{
	GENERATED_BODY()

public:
	ALegionUnit();

	/** 初始化兵团编成、兵种原型、战役编号与可视化资产 */
	void InitLegion(ELegionTeam InTeam, ELegionClass InArchetype, int32 InSoldierCount,
		UStaticMesh* InBodyMesh, UStaticMesh* InExtraMesh, UMaterialInterface* InBodyMaterial,
		USkeletalMesh* InSkeletalBody = nullptr,
		UAnimationAsset* InAnimIdle = nullptr,
		UAnimationAsset* InAnimMove = nullptr,
		const TArray<UAnimationAsset*>& InAnimAttacks = {},
		const TArray<UAnimationAsset*>& InAnimDeaths = {},
		int32 InCampaignLegionId = INDEX_NONE,
		const FString& InCallsign = TEXT(""),
		int32 InSyncLevel = 1,
		float InInitialFacingYaw = 90.0f,
		const FLinearColor& InSkeletalTint = FLinearColor(-1.0f, 0.0f, 0.0f, 1.0f));

	void SetTerrainGrid(const FLegionTerrainGrid* InGrid) { TerrainGrid = InGrid; }

	/** 绑定动画混合空间与放松站姿 (由 GameMode 统一注入, 支持 8 向步法 + 速率混合) */
	void SetAnimationSuites(UBlendSpace* InRelaxedBS, UBlendSpace* InCombatBS, UAnimationAsset* InRelaxedIdle);

	/** 初始化本兵团确定性随机流 (保证同种子下巡逻/侦察/单兵走位完全可复现) */
	void SetDeterministicSeed(int32 InSeed);

	// ---------------- 命令与弱 AI 指令接口 ----------------

	/** 移动到目标点 (可附带自定义驻守朝向角度; OptionalFacingYaw > 999 表示自动面向行军方向) */
	void CommandMoveTo(const FVector& Destination, float OptionalFacingYaw = 1000.0f);

	/** 进攻另一个兵团 */
	void CommandAttack(ALegionUnit* Target);

	/** 停驻当前位置, 清除目标与直属机动命令, 切回休整待命 */
	void CommandHalt();

	/** 戒备固守: 锚定指定阵地扇面扫视警戒, 迎击近敌但不过度追击 */
	void CommandGuard(const FVector& AnchorPos, float OptionalFacingYaw = 1000.0f);

	/** 防区巡逻: 沿巡逻航点列表往返巡弋并在航点驻留观察 */
	void CommandPatrol(const TArray<FVector>& InPoints);

	/** 探索侦察: 自主在战区高地/侧翼/林缘间穿插侦察并向友军通报敌情 */
	void CommandRecon();

	/** 弱 AI 主动接战 (不覆盖玩家直属命令标记外的战术语义, 支持侧翼包抄) */
	void CommandAutonomousAssault(ALegionUnit* PreferredTarget = nullptr);

	/** 接收目击或友军广播的敌情通报 */
	void NotifyEnemyContact(ALegionUnit* SpottedEnemy, const FVector& ContactWorldPos, bool bFromDirectFire = false);

	/** 受击瞬间拉响警戒并记录威胁方位 */
	void NotifyIncomingFire(const FVector& ThreatWorldPos);

	/** 由 GameMode 在战斗打响后逐帧推进军团弱 AI 决策树 */
	void UpdateWeakAIDirective(float DeltaSeconds, ALegionGameMode* GM);

	// ---------------- 选中/悬停视觉 ----------------

	void SetSelected(bool bInSelected);
	void SetHovered(bool bInHovered);

	// ---------------- 查询 ----------------

	int32 GetAliveCount() const { return AliveCount; }
	int32 GetSoldierCount() const { return Soldiers.Num(); }
	bool IsWiped() const { return bWiped; }
	bool IsSelected() const { return bSelected; }
	ELegionTeam GetTeam() const { return Team; }
	ELegionClass GetArchetype() const { return Archetype; }
	const FLegionArchetypeStats& GetArchetypeStats() const { return ArchetypeStats; }
	ELegionState GetLegionState() const { return State; }
	ELegionDirective GetDirective() const { return Directive; }
	ELegionAlertness GetAlertness() const { return Alertness; }
	float GetAlertLevel() const { return AlertLevel; }
	const TArray<FVector>& GetPatrolWaypoints() const { return PatrolWaypoints; }
	FVector GetGuardAnchor() const { return GuardAnchor; }
	ALegionUnit* GetAttackTarget() const { return AttackTarget.Get(); }

	/** 统计团内存活士兵各微观状态分布 (用于 HUD 诊断与自动化弱 AI 自检) */
	void GetSoldierStateDistribution(TMap<ELegionSoldierState, int32>& OutCounts, int32& OutMovingCount, float& OutYawSpreadDeg) const;

	/** 重置多帧运动稳定性遥测窗口 */
	void ResetMotionTelemetry();

	/** 评估多帧运动稳定性 (跳动/抖动/方向抽搐是否在阈值内) */
	bool EvaluateMotionStability(FString& OutSummary) const;

	// ---- 当前命令 (供地图 / HUD 绘制路线、终点与朝向箭头) ----
	bool HasMoveOrder() const { return bHasMoveDestination && !bWiped; }
	FVector GetMoveDestination() const { return MoveDestination; }
	bool HasCustomFacing() const { return bHasCustomFacing; }
	float GetDesiredFacingYaw() const { return DesiredFacingYaw; }
	/** 剩余路径点 (已走过的不含; 末点为终点) */
	void GetRemainingPath(TArray<FVector>& OutPoints) const
	{
		OutPoints.Reset();
		for (int32 i = FMath::Max(0, WaypointIndex); i < Waypoints.Num(); ++i) { OutPoints.Add(Waypoints[i]); }
		if (OutPoints.Num() == 0 && bHasMoveDestination) { OutPoints.Add(MoveDestination); }
	}
	float GetLegionHP() const { return LegionHP; }
	float GetLegionMaxHP() const { return (float)Soldiers.Num() * FMath::Max(1, SoldierHP); }
	int32 GetCampaignLegionId() const { return CampaignLegionId; }
	const FString& GetCallsign() const { return Callsign; }
	int32 GetSyncLevel() const { return SyncLevel; }
	FVector GetFormationHalfExtent() const { return FormationHalfExtent; }
	float GetFacingYaw() const { return FacingYaw; }
	FVector GetFacingVector() const { return FRotator(0.0f, FacingYaw, 0.0f).Vector().GetSafeNormal2D(); }
	/** 敌军直插天空番号/部队 Logo 的世界空间锚点 */
	FVector GetSkyBannerWorldLocation() const { return GetActorLocation() + FVector(0.0f, 0.0f, EnemySkyBannerHeight); }

	/** 计算实际有效射程 (含高地对远程的延展) */
	float GetEffectiveEngageRange(const ALegionUnit* Target = nullptr) const;

	/** 结算伤害 (含正面格挡/侧背破防/长戟反制) */
	int32 ApplyDamage(float DamageAmount, ALegionUnit* InInstigator = nullptr, bool bRangedHit = false);

	/** 指挥官子弹精准命中单个士兵: 伤害仅结算到被命中的个体, 血量归零时只死该个体 */
	int32 ApplySoldierProjectileHit(const UPrimitiveComponent* HitComp, int32 HitItem,
		const FVector& HitLocation, const FVector& HitDirection, float DamageAmount);

	/** 实体弹道命中结算: 直接按索引对个体士兵扣血 (Geometric 命中, 不经碰撞组件) */
	int32 ApplyBulletHitToSoldier(int32 SoldierIndex, float DamageAmount, const FVector& HitDirection);

	/** 查询存活士兵脚底世界位置 (供弹道扫描); 士兵阵亡/索引越界返回 false */
	bool GetAliveSoldierFeet(int32 SoldierIndex, FVector& OutFeet) const;

	/** 随机取一名存活士兵索引 (弹道目标选择); 全灭返回 INDEX_NONE */
	int32 PickRandomAliveSoldier() const;

	/** 自检: 验证命中指定个体胶囊时只击杀该个体且立即关闭其碰撞 */
	bool RunExactHitSelfTest();

	/** 施加击退冲量 */
	void ApplyKnockback(const FVector& WorldImpulse);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLegionWiped, ALegionUnit*);
	FOnLegionWiped OnLegionWiped;

	virtual void Tick(float DeltaSeconds) override;

	// ---------------- 参数 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Formation")
	float SoldierSpacing = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Formation", meta = (ClampMin = "0.2", ClampMax = "8.0"))
	float FormationColumnBias = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Formation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FormationJitter = 0.36f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Formation")
	float SoldierYawJitterDeg = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Move")
	float MoveSpeed = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Move")
	float TurnSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Move")
	float FormationTurnSpeed = 210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Combat")
	float EngageRange = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Combat")
	float DamagePerSoldierPerSecond = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Combat")
	int32 SoldierHP = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual")
	float SoldierScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual")
	float MarchBobAmplitude = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual")
	float EngageLungeAmplitude = 55.0f;

	/** 敌军直插天空番号与部队 Logo 相对兵团地面的悬空高度 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual", meta = (ClampMin = "1200.0", ClampMax = "15000.0"))
	float EnemySkyBannerHeight = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Terrain", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float GroundConformInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Terrain", meta = (ClampMin = "1.0", ClampMax = "40.0"))
	float GroundConformSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Terrain")
	float GroundTraceHeight = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual")
	FLinearColor AllyColor = FLinearColor(0.05f, 0.56f, 0.94f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual")
	FLinearColor EnemyColor = FLinearColor(0.84f, 0.05f, 0.03f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Visual")
	FLinearColor SelectedColor = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f);

protected:
	virtual void BeginPlay() override;

private:
	/** HISM 表现层在一支兵团内保持不变的网格归一化与兵种装备局部变换 */
	struct FLegionInstancedTransformParams
	{
		FTransform BodyLocal = FTransform::Identity;
		FTransform GearLocal = FTransform::Identity;
		float ActorYaw = 0.0f;
	};

	void RebuildFormation();
	FLegionInstancedTransformParams BuildInstancedTransformParams() const;
	void ComputeSoldierTransforms(const FLegionSoldier& S, float TimeSeconds,
		const FLegionInstancedTransformParams& Params, FTransform& OutBody, FTransform& OutHead) const;
	void WriteAllSoldierTransforms(float TimeSeconds);
	void WriteSkeletalSoldierRepresentation(float DeltaSeconds);
	void UpdateFrameVisual();
	void UpdateOrderVisuals(float TimeSeconds);
	void ConformToTerrain(float DeltaSeconds);
	bool TraceGroundZ(const FVector& XY, float& OutZ) const;
	void StartDying(int32 SoldierIndex);
	void FireRangedVolley(ALegionUnit* Target);
	void ApplyPeerSeparation(float DeltaSeconds);

	void MoveToInternal(const FVector& Destination, float OptionalFacingYaw, ELegionDirective KeepDirective);
	FVector PickNextReconPoint(const ALegionGameMode* GM);
	void UpdateAlertness(float DeltaSeconds, ALegionGameMode* GM);
	void UpdateSoldiersMicroAI(float DeltaSeconds, float Now, ALegionGameMode* GM);
	void DriveSoldierAnimation(FLegionSoldier& S, int32 SoldierIndex, float DeltaSeconds);
	bool IsFriendlyBlockingFireLine(int32 SoldierIndex, const FVector2D& AimDir2D, float CheckDist, float& OutClearSideSign) const;
	bool PickConflictFreeStepOffset(int32 SoldierIndex, const FVector2D& HomeWorldXY, float MinStepDist, float MaxRadius,
		const FVector2D& PreferredDir, float DirSpreadRad, FVector2D& OutOffset);
	int32 PickSoldierCombatTarget(FLegionSoldier& S, int32 SoldierIndex, ALegionUnit* Foe, bool bForceReselect, FVector& OutTargetFeet);

	void SpawnSkeletalSoldier(FLegionSoldier& S);
	void ApplyAnimStateToSoldiers();
	UAnimationAsset* PickAnimForState(ELegionState InState, int32 SoldierIndex) const;
	void EnableSoldierRagdoll(USkeletalMeshComponent* C, int32 SoldierIndex);
	int32 PickCasualtyIndex(const FVector& FromPos) const;
	int32 FindSoldierIndexByHit(const UPrimitiveComponent* HitComp, int32 HitItem, const FVector& HitLocation) const;
	void CheckAndHandleWiped();

	struct FLegionBlendSpaceBinding
	{
		bool bValid = false;
		int32 SpeedAxis = 1; // 0=X, 1=Y
		int32 DirAxis = 0;   // 0=X, 1=Y
		float MinSpeed = 0.0f;
		float MaxSpeed = 500.0f;
		float MinDir = -180.0f;
		float MaxDir = 180.0f;
	};
	static FLegionBlendSpaceBinding InspectBlendSpace(const UBlendSpace* BS);

	// ---------------- 组件 ----------------
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> PickBox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SoldierHISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ExtraHISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrameTop;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrameBottom;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrameLeft;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrameRight;

	/** 正面朝向指示翼条 (+X 前锋高亮横杠, 方便判别举盾/拒马朝向) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrontShieldBar;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OrderLine;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MoveMarker;

	// ---------------- 状态 ----------------
	UPROPERTY(Transient)
	TArray<FLegionSoldier> Soldiers;

	ELegionTeam Team = ELegionTeam::Player;
	ELegionClass Archetype = ELegionClass::Shieldbearer;
	FLegionArchetypeStats ArchetypeStats;
	ELegionState State = ELegionState::Holding;

	// ---------------- 军团弱 AI 战术层 ----------------
	ELegionDirective Directive = ELegionDirective::Standby;
	ELegionAlertness Alertness = ELegionAlertness::Relaxed;
	float AlertLevel = 0.0f;
	float AlertDecayCooldown = 0.0f;
	float DirectiveDecisionTimer = 0.0f;
	float DirectiveHoldTimer = 0.0f;
	float DirectiveSpeedMul = 1.0f;

	FVector GuardAnchor = FVector::ZeroVector;
	float GuardBaseYaw = 90.0f;
	float GuardScanPhase = 0.0f;

	TArray<FVector> PatrolWaypoints;
	int32 PatrolCursor = 0;
	int32 PatrolDirection = 1;
	float PatrolDwellTimer = 0.0f;

	FVector ReconGoal = FVector::ZeroVector;
	float ReconDwellTimer = 0.0f;
	bool bHasReconGoal = false;

	FVector LastKnownThreatPos = FVector::ZeroVector;
	float ThreatMemoryTimer = 0.0f;
	float ContactBroadcastCooldown = 0.0f;
	float FlankCommitTimer = 0.0f;
	FVector FlankWaypoint = FVector::ZeroVector;

	FRandomStream UnitRng;

	int32 CampaignLegionId = INDEX_NONE;
	FString Callsign;
	int32 SyncLevel = 1;

	TArray<FVector> Waypoints;
	int32 WaypointIndex = 0;
	FVector MoveDestination = FVector::ZeroVector;
	bool bHasMoveDestination = false;
	bool bHasCustomFacing = false;
	float FacingYaw = 90.0f;
	float DesiredFacingYaw = 90.0f;

	TWeakObjectPtr<ALegionUnit> AttackTarget;
	float RepathTimer = 0.0f;
	float VolleyTimer = 0.0f;

	/** 真实弹道: 目标兵团扫描节流 (0.35s 轮询最近射程内敌团) */
	float FireScanTimer = 0.0f;
	/** 真实弹道: 当前自动开火目标团 (无需攻击命令, 驻守亦会自卫射击) */
	TWeakObjectPtr<ALegionUnit> CachedFireTarget;

	FVector KnockbackVelocity = FVector::ZeroVector;

	const FLegionTerrainGrid* TerrainGrid = nullptr;

	float LegionHP = 0.0f;
	int32 AliveCount = 0;
	int32 TargetSoldierCount = 0;
	float SoldierXYScale = 1.0f;

	// ---------------- 骨骼士兵模式 ----------------
	bool bSkeletalSoldiers = false;
	FLinearColor SkeletalTint = FLinearColor(-1.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> SkeletalBodyMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SoldierBodyMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> AnimIdle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> AnimRelaxedIdle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> AnimMove;
	UPROPERTY(Transient)
	TObjectPtr<UBlendSpace> RelaxedBlendSpace;
	UPROPERTY(Transient)
	TObjectPtr<UBlendSpace> CombatBlendSpace;
	FLegionBlendSpaceBinding RelaxedBSBinding;
	FLegionBlendSpaceBinding CombatBSBinding;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimationAsset>> AnimAttackPool;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimationAsset>> AnimDeathPool;

	ELegionState AppliedAnimState = ELegionState::Wiped;
	TWeakObjectPtr<ALegionUnit> LastAttacker;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsAsset> RagdollPhysicsAsset;

	bool bWiped = false;
	bool bSelected = false;
	bool bHovered = false;
	bool bSoldierTransformsDirty = true;
	/** 远距手枪隐藏状态 (带滞回, 仅表现层) */
	bool bGunsHiddenForDistance = false;
	float GroundConformTimer = 0.0f;
	int32 GroundTraceCursor = 0;
	FVector FormationHalfExtent = FVector(300.0f, 300.0f, 0.0f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrameMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrontBarMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OrderVisualMID;
};
