// Copyright Ebbing Rainbow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BoneReference.h"
#include "BoneContainer.h"
#include "TurretAnimInstance.generated.h"

class UTurretAnimInstance;

/**
 * 科幻四足炮台程序化动画代理:
 * 工作线程里只读写代理快照，避免在 Evaluate 阶段触碰 UObject 状态。
 */
struct FTurretAnimInstanceProxy : public FAnimInstanceProxy
{
public:
	FTurretAnimInstanceProxy() = default;
	explicit FTurretAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void CacheBones() override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	struct FLegBoneGroup
	{
		FBoneReference Begin;
		FBoneReference Upper;
		FBoneReference Knee;
		float PhaseOffset = 0.0f;
		/** 右侧(+Y)前摆绕 +Z 取负，左侧(-Y)前摆绕 +Z 取正 */
		float YawStepSign = 1.0f;
	};

	void InitBoneReferences(const FBoneContainer& InRequiredBones);

	FBoneReference BodyBone;
	FBoneReference HeadBone;
	FBoneReference BarrelBone;
	FLegBoneGroup Legs[4]; // RF, LF, RB, LB
	bool bBonesCached = false;

	// 由 PreUpdate 从 GameThread 同步过来的只读快照
	float TimeAcc = 0.0f;
	float WalkPhase = 0.0f;
	float WalkWeight = 0.0f;
	float AttackAlpha = 0.0f; // 0..1 归一化进度, <= 0 表示未播放
	float HitAlpha = 0.0f;    // 1 -> 0 衰减
	FVector HitFromDirCS = FVector(1.0f, 0.0f, 0.0f);
};

/**
 * 科幻四足炮台 (SKM_scifiturret) 纯 C++ 程序化骨骼动画实例:
 * - 待机: 炮塔顶部雷达往复扫描 + 机身微弱液压呼吸
 * - 移动: 四足对角步态 (RF+LB / LF+RB 交替迈步抬腿落足) + 机身步频起伏
 * - 攻击: 炮管后坐力回弹 + 炮头锁定正前方 + 机身卸力后仰
 * - 受击: 按受击方向向后仰倒震颤
 */
UCLASS(Transient)
class EBBINGRAINBOW_API UTurretAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 设置周期随机相位，避免多个同场炮台待机完全同步 */
	void SetSeedPhase(float InSeedPhase);

	/** 同步移动状态与速度 (cm/s) */
	void SetMoving(bool bInMoving, float InMoveSpeed);

	/** 触发一次开火后坐力动画 */
	void TriggerAttack(float Duration = 0.55f);

	/** 触发一次方向性受击顿挫 (组件空间来源方向) */
	void TriggerHit(const FVector& ComponentSpaceFromDir, float Duration = 0.38f);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override
	{
		return new FTurretAnimInstanceProxy(this);
	}

	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override
	{
		delete InProxy;
	}

private:
	friend struct FTurretAnimInstanceProxy;

	float SeedPhase = 0.0f;
	float TimeAcc = 0.0f;

	bool bIsMoving = false;
	float CurrentMoveSpeed = 0.0f;
	float WalkPhase = 0.0f;
	float WalkWeight = 0.0f;

	float AttackElapsed = 0.0f;
	float AttackDuration = 0.0f;

	float HitRemaining = 0.0f;
	float HitDuration = 0.38f;
	FVector HitFromDirCS = FVector(1.0f, 0.0f, 0.0f);
};
