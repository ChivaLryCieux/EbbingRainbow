// Copyright Ebbing Rainbow. All Rights Reserved.

#include "Battle/Units/TurretAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"

namespace
{
	/**
	 * 围绕组件空间轴 AxisCS 旋转骨骼 BoneRef（正角按右手定则旋转）.
	 * 通过将 AxisCS 转换到父骨骼的参考系，可直接修改 Output.Pose 中的本地姿态，
	 * 让子骨骼链天然跟随且无需二次全链转回。
	 */
	void RotateBoneAroundCSAxis(
		FCompactPose& Pose,
		FCSPose<FCompactPose>& RefCSPose,
		const FBoneReference& BoneRef,
		const FVector& AxisCS,
		float AngleRad)
	{
		if (FMath::Abs(AngleRad) <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (!BoneRef.IsValidToEvaluate(Bones))
		{
			return;
		}
		const FCompactPoseBoneIndex Idx = BoneRef.GetCompactPoseIndex(Bones);
		const FCompactPoseBoneIndex ParentIdx = Pose.GetParentBoneIndex(Idx);

		const FQuat ParentCSRot = (ParentIdx != INDEX_NONE)
			? RefCSPose.GetComponentSpaceTransform(ParentIdx).GetRotation()
			: FQuat::Identity;

		const FVector AxisInParent = ParentCSRot.UnrotateVector(AxisCS).GetSafeNormal();
		if (AxisInParent.IsNearlyZero())
		{
			return;
		}

		FTransform& LocalT = Pose[Idx];
		const FQuat DeltaInParent(AxisInParent, AngleRad);
		LocalT.SetRotation((DeltaInParent * LocalT.GetRotation()).GetNormalized());
	}

	/** 沿组件空间偏移向量 DeltaCS 平移骨骼 BoneRef */
	void TranslateBoneInCS(
		FCompactPose& Pose,
		FCSPose<FCompactPose>& RefCSPose,
		const FBoneReference& BoneRef,
		const FVector& DeltaCS)
	{
		if (DeltaCS.IsNearlyZero(KINDA_SMALL_NUMBER))
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (!BoneRef.IsValidToEvaluate(Bones))
		{
			return;
		}
		const FCompactPoseBoneIndex Idx = BoneRef.GetCompactPoseIndex(Bones);
		const FCompactPoseBoneIndex ParentIdx = Pose.GetParentBoneIndex(Idx);

		const FQuat ParentCSRot = (ParentIdx != INDEX_NONE)
			? RefCSPose.GetComponentSpaceTransform(ParentIdx).GetRotation()
			: FQuat::Identity;

		Pose[Idx].AddToTranslation(ParentCSRot.UnrotateVector(DeltaCS));
	}
}

void FTurretAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	BodyBone = FBoneReference(TEXT("bn_body"));
	HeadBone = FBoneReference(TEXT("bn_head"));
	BarrelBone = FBoneReference(TEXT("bn_headbarrel"));

	// 对角步态: RF(0)+LB(3) 同相位 0, LF(1)+RB(2) 反相 PI
	Legs[0].Begin = FBoneReference(TEXT("bn_legbegin_RF"));
	Legs[0].Upper = FBoneReference(TEXT("bn_leg1_RF"));
	Legs[0].Knee = FBoneReference(TEXT("bn_leg2_RF"));
	Legs[0].PhaseOffset = 0.0f;
	Legs[0].YawStepSign = -1.0f;

	Legs[1].Begin = FBoneReference(TEXT("bn_legbegin_LF"));
	Legs[1].Upper = FBoneReference(TEXT("bn_leg1_LF"));
	Legs[1].Knee = FBoneReference(TEXT("bn_leg2_LF"));
	Legs[1].PhaseOffset = PI;
	Legs[1].YawStepSign = 1.0f;

	Legs[2].Begin = FBoneReference(TEXT("bn_legbegin_RB"));
	Legs[2].Upper = FBoneReference(TEXT("bn_leg1_RB"));
	Legs[2].Knee = FBoneReference(TEXT("bn_leg2_RB"));
	Legs[2].PhaseOffset = PI;
	Legs[2].YawStepSign = -1.0f;

	Legs[3].Begin = FBoneReference(TEXT("bn_legbegin_LB"));
	Legs[3].Upper = FBoneReference(TEXT("bn_leg1_LB"));
	Legs[3].Knee = FBoneReference(TEXT("bn_leg2_LB"));
	Legs[3].PhaseOffset = 0.0f;
	Legs[3].YawStepSign = 1.0f;

	bBonesCached = false;
}

void FTurretAnimInstanceProxy::InitBoneReferences(const FBoneContainer& InRequiredBones)
{
	if (!InRequiredBones.IsValid())
	{
		return;
	}

	BodyBone.Initialize(InRequiredBones);
	HeadBone.Initialize(InRequiredBones);
	BarrelBone.Initialize(InRequiredBones);

	for (FLegBoneGroup& Leg : Legs)
	{
		Leg.Begin.Initialize(InRequiredBones);
		Leg.Upper.Initialize(InRequiredBones);
		Leg.Knee.Initialize(InRequiredBones);
	}
	bBonesCached = true;
}

void FTurretAnimInstanceProxy::CacheBones()
{
	FAnimInstanceProxy::CacheBones();
	InitBoneReferences(GetRequiredBones());
}

void FTurretAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	if (!bBonesCached && GetRequiredBones().IsValid())
	{
		InitBoneReferences(GetRequiredBones());
	}

	if (const UTurretAnimInstance* TurretAnim = Cast<UTurretAnimInstance>(InAnimInstance))
	{
		TimeAcc = TurretAnim->TimeAcc + TurretAnim->SeedPhase;
		WalkPhase = TurretAnim->WalkPhase;
		WalkWeight = TurretAnim->WalkWeight;
		AttackAlpha = (TurretAnim->AttackDuration > KINDA_SMALL_NUMBER)
			? FMath::Clamp(TurretAnim->AttackElapsed / TurretAnim->AttackDuration, 0.0f, 1.0f)
			: 0.0f;
		HitAlpha = (TurretAnim->HitDuration > KINDA_SMALL_NUMBER)
			? FMath::Clamp(TurretAnim->HitRemaining / TurretAnim->HitDuration, 0.0f, 1.0f)
			: 0.0f;
		HitFromDirCS = TurretAnim->HitFromDirCS;
	}
}

bool FTurretAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!Bones.IsValid() || !bBonesCached)
	{
		return true;
	}

	FCSPose<FCompactPose> RefCSPose;
	RefCSPose.InitPose(Output.Pose);

	// 1. 开火后坐力包络: 前 14% 急速后坐到峰值 1.0, 随后阻尼回弹至 0
	float Recoil = 0.0f;
	float AimFocus = 0.0f;
	if (AttackAlpha > 0.0f && AttackAlpha < 1.0f)
	{
		constexpr float PeakT = 0.14f;
		if (AttackAlpha <= PeakT)
		{
			Recoil = AttackAlpha / PeakT;
		}
		else
		{
			const float T = (AttackAlpha - PeakT) / (1.0f - PeakT);
			Recoil = FMath::Exp(-5.2f * T) * (1.0f - T);
		}
		// 攻击期间炮头收拢雷达扫视，稳稳瞄向前方
		AimFocus = FMath::Sin(AttackAlpha * PI);
	}

	// 2. 受击顿挫包络
	float HitTiltAngle = 0.0f;
	FVector HitAxisCS = FVector(0.0f, 1.0f, 0.0f);
	if (HitAlpha > 0.001f)
	{
		const float StaggerEnv = FMath::Pow(HitAlpha, 0.7f);
		const float Rattle = 1.0f + 0.18f * FMath::Sin(HitAlpha * 24.0f);
		HitTiltAngle = FMath::DegreesToRadians(14.0f) * StaggerEnv * Rattle;
		FVector FlatFrom = FVector(HitFromDirCS.X, HitFromDirCS.Y, 0.0f).GetSafeNormal();
		if (FlatFrom.IsNearlyZero())
		{
			FlatFrom = FVector(1.0f, 0.0f, 0.0f);
		}
		// From x Up 的右手旋转将顶部(+Z)推向 -From 方向，即朝来袭反方向后仰
		HitAxisCS = FVector::CrossProduct(FlatFrom, FVector::UpVector).GetSafeNormal();
		if (HitAxisCS.IsNearlyZero())
		{
			HitAxisCS = FVector(0.0f, 1.0f, 0.0f);
		}
	}

	// 3. 机身待机呼吸 + 步态颠簸 + 射击卸力 + 受击倾倒
	const float IdleBobZ = 1.8f * FMath::Sin(TimeAcc * 2.1f);
	const float WalkBobZ = -3.2f * FMath::Abs(FMath::Sin(WalkPhase)) * WalkWeight;
	const FVector BodyOffsetCS(
		-6.5f * Recoil,
		0.0f,
		IdleBobZ + WalkBobZ - 1.5f * Recoil);
	TranslateBoneInCS(Output.Pose, RefCSPose, BodyBone, BodyOffsetCS);

	// 开火时机身略微后仰 (绕 +Y 逆向，即 -Pitch around Right=+Y: 抬头需要 -Angle around +Y)
	const float BodyAttackPitch = -FMath::DegreesToRadians(4.5f) * Recoil;
	const float IdleRoll = FMath::DegreesToRadians(1.1f) * FMath::Sin(TimeAcc * 1.15f);
	RotateBoneAroundCSAxis(Output.Pose, RefCSPose, BodyBone, FVector::RightVector, BodyAttackPitch);
	RotateBoneAroundCSAxis(Output.Pose, RefCSPose, BodyBone, FVector::ForwardVector, IdleRoll);
	if (HitTiltAngle > KINDA_SMALL_NUMBER)
	{
		RotateBoneAroundCSAxis(Output.Pose, RefCSPose, BodyBone, HitAxisCS, HitTiltAngle);
	}

	// 4. 炮塔头部: 待机雷达往复扫描 (开火时锁定正前方 + 轻微上抬)
	const float ScanScale = 1.0f - 0.88f * AimFocus;
	const float HeadYaw = (FMath::DegreesToRadians(19.0f) * FMath::Sin(TimeAcc * 0.92f)
		+ FMath::DegreesToRadians(6.0f) * FMath::Sin(TimeAcc * 2.25f)) * ScanScale;
	const float HeadPitch = FMath::DegreesToRadians(3.0f) * FMath::Sin(TimeAcc * 1.4f) * ScanScale
		- FMath::DegreesToRadians(3.5f) * Recoil;
	RotateBoneAroundCSAxis(Output.Pose, RefCSPose, HeadBone, FVector::UpVector, HeadYaw);
	RotateBoneAroundCSAxis(Output.Pose, RefCSPose, HeadBone, FVector::RightVector, HeadPitch);

	// 5. 炮管后坐力: 沿正前方后退 28cm 并伴随枪口上跳
	if (Recoil > 0.001f)
	{
		TranslateBoneInCS(Output.Pose, RefCSPose, BarrelBone, FVector(-28.0f * Recoil, 0.0f, 0.0f));
		RotateBoneAroundCSAxis(Output.Pose, RefCSPose, BarrelBone, FVector::RightVector,
			-FMath::DegreesToRadians(5.5f) * Recoil);
	}

	// 6. 四足对角步态 (移动时平滑混合)
	if (WalkWeight > 0.005f)
	{
		constexpr float MaxSwingRad = FMath::DegreesToRadians(22.0f);
		constexpr float MaxLiftRad = FMath::DegreesToRadians(24.0f);

		for (const FLegBoneGroup& Leg : Legs)
		{
			if (!Leg.Begin.IsValidToEvaluate(Bones))
			{
				continue;
			}

			const float Phase = WalkPhase + Leg.PhaseOffset;
			// Sin(Phase): 导数为 Cos(Phase), 故 Cos > 0 时脚正在向前摆动 (离地抬脚), Cos < 0 时向后蹬地 (贴地)
			const float SwingRad = MaxSwingRad * FMath::Sin(Phase) * WalkWeight * Leg.YawStepSign;
			const float SwingUp = FMath::Max(0.0f, FMath::Cos(Phase));
			const float LiftRad = MaxLiftRad * FMath::Pow(SwingUp, 0.72f) * WalkWeight;

			RotateBoneAroundCSAxis(Output.Pose, RefCSPose, Leg.Begin, FVector::UpVector, SwingRad);

			if (LiftRad > 0.002f && Leg.Upper.IsValidToEvaluate(Bones))
			{
				const FCompactPoseBoneIndex BeginIdx = Leg.Begin.GetCompactPoseIndex(Bones);
				const FCompactPoseBoneIndex RefTargetIdx = Leg.Knee.IsValidToEvaluate(Bones)
					? Leg.Knee.GetCompactPoseIndex(Bones)
					: Leg.Upper.GetCompactPoseIndex(Bones);

				FVector OutXY = RefCSPose.GetComponentSpaceTransform(RefTargetIdx).GetLocation()
					- RefCSPose.GetComponentSpaceTransform(BeginIdx).GetLocation();
				OutXY.Z = 0.0f;
				OutXY.Normalize();

				const FVector LiftAxisCS = FVector::CrossProduct(OutXY, FVector::UpVector).GetSafeNormal();
				if (!LiftAxisCS.IsNearlyZero())
				{
					// 大腿抬起, 小腿顺势下折保持爪尖接近水平离地
					RotateBoneAroundCSAxis(Output.Pose, RefCSPose, Leg.Upper, LiftAxisCS, LiftRad);
					if (Leg.Knee.IsValidToEvaluate(Bones))
					{
						RotateBoneAroundCSAxis(Output.Pose, RefCSPose, Leg.Knee, LiftAxisCS, -LiftRad * 1.15f);
					}
				}
			}
		}
	}

	return true;
}

void UTurretAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UTurretAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	TimeAcc += DeltaSeconds;

	const float TargetWeight = bIsMoving ? 1.0f : 0.0f;
	WalkWeight = FMath::FInterpTo(WalkWeight, TargetWeight, DeltaSeconds, bIsMoving ? 14.0f : 10.0f);
	if (WalkWeight > 0.005f)
	{
		// 基准 420cm/s 对应每秒约 1.55 个完整步态周期 (每只脚约 10rad/s)
		const float SpeedRatio = bIsMoving ? FMath::Clamp(CurrentMoveSpeed / 420.0f, 0.65f, 1.8f) : 1.0f;
		WalkPhase += DeltaSeconds * 9.5f * SpeedRatio;
	}

	if (AttackDuration > KINDA_SMALL_NUMBER)
	{
		AttackElapsed += DeltaSeconds;
		if (AttackElapsed >= AttackDuration)
		{
			AttackElapsed = 0.0f;
			AttackDuration = 0.0f;
		}
	}

	if (HitRemaining > 0.0f)
	{
		HitRemaining = FMath::Max(0.0f, HitRemaining - DeltaSeconds);
	}
}

void UTurretAnimInstance::SetSeedPhase(float InSeedPhase)
{
	SeedPhase = InSeedPhase;
}

void UTurretAnimInstance::SetMoving(bool bInMoving, float InMoveSpeed)
{
	bIsMoving = bInMoving;
	CurrentMoveSpeed = InMoveSpeed;
}

void UTurretAnimInstance::TriggerAttack(float Duration)
{
	AttackDuration = FMath::Max(Duration, 0.15f);
	AttackElapsed = KINDA_SMALL_NUMBER;
}

void UTurretAnimInstance::TriggerHit(const FVector& ComponentSpaceFromDir, float Duration)
{
	HitDuration = FMath::Max(Duration, 0.12f);
	HitRemaining = HitDuration;
	HitFromDirCS = ComponentSpaceFromDir.GetSafeNormal();
	if (HitFromDirCS.IsNearlyZero())
	{
		HitFromDirCS = FVector(1.0f, 0.0f, 0.0f);
	}
}
