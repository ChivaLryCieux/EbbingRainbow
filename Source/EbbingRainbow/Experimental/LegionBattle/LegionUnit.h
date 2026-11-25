// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Experimental/LegionBattle/LegionTypes.h"
#include "LegionUnit.generated.h"

class FLegionTerrainGrid;
class UAnimationAsset;
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

	// ---------------- 命令接口 ----------------

	/** 移动到目标点 (可附带自定义驻守朝向角度; OptionalFacingYaw > 999 表示自动面向行军方向) */
	void CommandMoveTo(const FVector& Destination, float OptionalFacingYaw = 1000.0f);

	/** 进攻另一个兵团 */
	void CommandAttack(ALegionUnit* Target);

	/** 停驻当前位置, 清除目标 */
	void CommandHalt();

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
	ALegionUnit* GetAttackTarget() const { return AttackTarget.Get(); }

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
	void RebuildFormation();
	void ComputeSoldierTransforms(const FLegionSoldier& S, float TimeSeconds,
		FTransform& OutBody, FTransform& OutHead) const;
	void WriteAllSoldierTransforms(float TimeSeconds);
	void UpdateFrameVisual();
	void UpdateOrderVisuals(float TimeSeconds);
	void ConformToTerrain(float DeltaSeconds);
	bool TraceGroundZ(const FVector& XY, float& OutZ) const;
	void StartDying(int32 SoldierIndex);
	void FireRangedVolley(ALegionUnit* Target);
	void ApplyPeerSeparation(float DeltaSeconds);

	void SpawnSkeletalSoldier(FLegionSoldier& S);
	void ApplyAnimStateToSoldiers();
	UAnimationAsset* PickAnimForState(ELegionState InState, int32 SoldierIndex) const;
	void EnableSoldierRagdoll(USkeletalMeshComponent* C, int32 SoldierIndex);
	int32 PickCasualtyIndex(const FVector& FromPos) const;
	int32 FindSoldierIndexByHit(const UPrimitiveComponent* HitComp, int32 HitItem, const FVector& HitLocation) const;
	void CheckAndHandleWiped();

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
	TObjectPtr<UAnimationAsset> AnimMove;
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
	float GroundConformTimer = 0.0f;
	FVector FormationHalfExtent = FVector(300.0f, 300.0f, 0.0f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrameMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrontBarMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OrderVisualMID;
};
