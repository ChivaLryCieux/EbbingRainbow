// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionUnit.h"

#include "Core/ERAssetCatalog.h"
#include "Core/ERCollisionChannels.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Legion/Terrain/LegionTerrainGrid.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/ConstructorHelpers.h"
#include "Legion/Units/LegionUnitInternal.h"
#include "Legion/LegionPerf.h"

void ALegionUnit::SetAnimationSuites(UBlendSpace* InRelaxedBS, UBlendSpace* InCombatBS, UAnimationAsset* InRelaxedIdle)
{
	RelaxedBlendSpace = InRelaxedBS;
	CombatBlendSpace = InCombatBS;
	AnimRelaxedIdle = InRelaxedIdle;
	RelaxedBSBinding = InspectBlendSpace(InRelaxedBS);
	CombatBSBinding = InspectBlendSpace(InCombatBS);
}

ALegionUnit::FLegionBlendSpaceBinding ALegionUnit::InspectBlendSpace(const UBlendSpace* BS)
{
	FLegionBlendSpaceBinding Out;
	if (!BS) { return Out; }
	const FBlendParameter& P0 = BS->GetBlendParameter(0);
	const FBlendParameter& P1 = BS->GetBlendParameter(1);

	auto IsDirParam = [](const FBlendParameter& P)
	{
		return P.DisplayName.Contains(TEXT("Dir"), ESearchCase::IgnoreCase)
			|| P.DisplayName.Contains(TEXT("Angle"), ESearchCase::IgnoreCase)
			|| (P.Min < -10.0f && P.Max > 90.0f);
	};

	if (IsDirParam(P0) && !IsDirParam(P1))
	{
		Out.DirAxis = 0; Out.MinDir = P0.Min; Out.MaxDir = P0.Max;
		Out.SpeedAxis = 1; Out.MinSpeed = P1.Min; Out.MaxSpeed = P1.Max;
	}
	else if (IsDirParam(P1) && !IsDirParam(P0))
	{
		Out.DirAxis = 1; Out.MinDir = P1.Min; Out.MaxDir = P1.Max;
		Out.SpeedAxis = 0; Out.MinSpeed = P0.Min; Out.MaxSpeed = P0.Max;
	}
	else
	{
		// 默认约定 X=Direction, Y=Speed
		Out.DirAxis = 0; Out.MinDir = P0.Min; Out.MaxDir = P0.Max;
		Out.SpeedAxis = 1; Out.MinSpeed = P1.Min; Out.MaxSpeed = P1.Max;
	}
	Out.bValid = (Out.MaxSpeed > Out.MinSpeed);
	return Out;
}

// ---------------- 选中/悬停 ----------------

void ALegionUnit::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected) { return; }
	bSelected = bInSelected;
	UpdateFrameVisual();
}

void ALegionUnit::SetHovered(bool bInHovered)
{
	if (bHovered == bInHovered) { return; }
	bHovered = bInHovered;
	UpdateFrameVisual();
}

void ALegionUnit::UpdateFrameVisual()
{
	if (!FrameMID && FrameTop)
	{
		FrameMID = FrameTop->CreateAndSetMaterialInstanceDynamic(0);
		if (FrameBottom) { FrameBottom->SetMaterial(0, FrameMID); }
		if (FrameLeft)   { FrameLeft->SetMaterial(0, FrameMID); }
		if (FrameRight)  { FrameRight->SetMaterial(0, FrameMID); }
	}
	if (!FrontBarMID && FrontShieldBar)
	{
		FrontBarMID = FrontShieldBar->CreateAndSetMaterialInstanceDynamic(0);
	}

	const FLinearColor TeamCol = (Team == ELegionTeam::Player) ? AllyColor : EnemyColor;
	if (FrameMID)
	{
		FLinearColor C = TeamCol;
		float Gain = bHovered ? 1.45f : 0.70f;
		if (bSelected) { C = SelectedColor; Gain = 1.75f; }
		if (bWiped)    { Gain = 0.08f; }
		FrameMID->SetVectorParameterValue(TEXT("Color"), C * Gain);
	}
	if (FrontBarMID)
	{
		const float FGain = bWiped ? 0.08f : (bSelected ? 2.1f : 1.15f);
		FrontBarMID->SetVectorParameterValue(TEXT("Color"), (bSelected ? SelectedColor : TeamCol) * FGain);
	}
}

void ALegionUnit::UpdateOrderVisuals(float TimeSeconds)
{
	// 战术路线、终点靶标与集火连线已统一交由 ALegionHUD::DrawOrderOverlays 绘制,
	// 这里保持 3D 占位立方体隐藏, 避免悬空木棍穿透骨骼士兵或遮挡视线。
	if (OrderLine)  { OrderLine->SetVisibility(false); }
	if (MoveMarker) { MoveMarker->SetVisibility(false); }
	(void)TimeSeconds;
}

void ALegionUnit::DriveSoldierAnimation(FLegionSoldier& S, int32 SoldierIndex, float DeltaSeconds)
{
	if (!bSkeletalSoldiers) { return; }
	USkeletalMeshComponent* C = S.SkelComp.Get();
	if (!C || !S.bAlive) { return; }
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_AnimationDrive);

	if (S.AnimSuiteHoldTimer > 0.0f) { S.AnimSuiteHoldTimer = FMath::Max(0.0f, S.AnimSuiteHoldTimer - DeltaSeconds); }

	const bool bWantCombatBS = (S.PerceivedAlertness != ELegionAlertness::Relaxed)
		|| (S.SoldierState == ELegionSoldierState::FormationMarch && Directive == ELegionDirective::Assault);

	UAnimSingleNodeInstance* Single = C->GetSingleNodeInstance();
	const bool bCurrentlyCombat = (Single && Single->GetCurrentAsset() == CombatBlendSpace.Get());
	bool bUseCombatBS = bCurrentlyCombat;
	if (bWantCombatBS != bCurrentlyCombat && S.AnimSuiteHoldTimer <= 0.0f)
	{
		bUseCombatBS = bWantCombatBS;
	}

	UBlendSpace* DesiredBS = bUseCombatBS ? CombatBlendSpace.Get() : RelaxedBlendSpace.Get();
	const FLegionBlendSpaceBinding& Binding = bUseCombatBS ? CombatBSBinding : RelaxedBSBinding;

	const float Speed2D = S.WorldVelXY.Size();
	// 速度死区滞回: 进走 > 28 cm/s, 停步 < 12 cm/s, 消除边界来回闪烁抽搐
	if (!S.bAnimWalking && Speed2D > 28.0f) { S.bAnimWalking = true; }
	else if (S.bAnimWalking && Speed2D < 12.0f) { S.bAnimWalking = false; }

	const float TargetAnimSpeed = S.bAnimWalking
		? FMath::Clamp(Speed2D, 0.0f, Binding.bValid ? Binding.MaxSpeed : 500.0f)
		: 0.0f;

	// 刹车锁方向: 仅在明确移动中更新方向, 停步瞬间保持上帧方向防止甩腿
	float TargetAnimDir = S.SmoothedAnimDir;
	if (S.bAnimWalking && Speed2D >= 28.0f)
	{
		const float VelYaw = FMath::RadiansToDegrees(FMath::Atan2(S.WorldVelXY.Y, S.WorldVelXY.X));
		float RawDir = FMath::FindDeltaAngleDegrees(S.CurYaw, VelYaw);
		// ±180 接缝保护: 限制在 [-162, +162] 且接近正后方时保持在当前同号半区, 绝不横穿 0 度甩腿
		if (FMath::Abs(RawDir) > 140.0f && FMath::Abs(S.SmoothedAnimDir) > 70.0f
			&& (RawDir * S.SmoothedAnimDir) < 0.0f)
		{
			RawDir = FMath::Sign(S.SmoothedAnimDir) * 162.0f;
		}
		else
		{
			RawDir = FMath::Clamp(RawDir, -162.0f, 162.0f);
		}
		if (Binding.bValid)
		{
			RawDir = FMath::Clamp(RawDir, Binding.MinDir, Binding.MaxDir);
		}
		TargetAnimDir = RawDir;
	}

	const float AnimDt = FMath::Min(DeltaSeconds, 0.033f);
	S.SmoothedAnimSpeed = FMath::FInterpTo(S.SmoothedAnimSpeed, TargetAnimSpeed, AnimDt, 7.5f);
	if (S.SmoothedAnimSpeed < 2.5f && TargetAnimSpeed <= 0.0f)
	{
		S.SmoothedAnimSpeed = 0.0f;
	}

	const float PrevDir = S.SmoothedAnimDir;
	if (S.SmoothedAnimSpeed < 10.0f && !S.bAnimWalking)
	{
		// 完全停稳归零时平滑向 0 度收拢
		S.SmoothedAnimDir = FRotator::NormalizeAxis(
			S.SmoothedAnimDir + FMath::FindDeltaAngleDegrees(S.SmoothedAnimDir, 0.0f) * FMath::Clamp(AnimDt * 4.0f, 0.0f, 1.0f));
	}
	else
	{
		// 单帧方向增量限幅 + 严格归一化, 彻底杜绝卡顿帧或越界引起的甩腿抽搐
		float DeltaDir = FMath::FindDeltaAngleDegrees(S.SmoothedAnimDir, TargetAnimDir) * FMath::Clamp(AnimDt * 7.5f, 0.0f, 1.0f);
		const float MaxStepDeg = FMath::Clamp(200.0f * AnimDt, 0.1f, 18.0f);
		DeltaDir = FMath::Clamp(DeltaDir, -MaxStepDeg, MaxStepDeg);
		S.SmoothedAnimDir = FRotator::NormalizeAxis(S.SmoothedAnimDir + DeltaDir);
	}
	const float AnimDirJump = FMath::Abs(FMath::FindDeltaAngleDegrees(PrevDir, S.SmoothedAnimDir));
	if (S.bTelPrimed)
	{
		S.MaxFrameJumpAnimDir = FMath::Max(S.MaxFrameJumpAnimDir, AnimDirJump);
	}

	if (DesiredBS && Binding.bValid && LegionUnitInternal::IsSafeSingleNodeAsset(DesiredBS))
	{
		if (Single && Single->GetCurrentAsset() != DesiredBS)
		{
			Single->SetAnimationAsset(DesiredBS, true, S.TraitPlayRate);
			Single->SetPosition(FMath::Frac(S.Phase * 0.37f + SoldierIndex * 0.07f), false);
			Single->SetPlaying(true);
			S.AnimSuiteHoldTimer = 1.2f;
		}
		else if (!Single)
		{
			C->PlayAnimation(DesiredBS, true);
			if (UAnimSingleNodeInstance* Created = C->GetSingleNodeInstance())
			{
				Created->SetPlayRate(S.TraitPlayRate);
				Created->SetPosition(FMath::Frac(S.Phase * 0.37f), false);
			}
			S.AnimSuiteHoldTimer = 1.2f;
		}

		if (UAnimSingleNodeInstance* Node = C->GetSingleNodeInstance())
		{
			FVector BlendInput = FVector::ZeroVector;
			BlendInput[Binding.DirAxis] = FMath::Clamp(S.SmoothedAnimDir, Binding.MinDir, Binding.MaxDir);
			BlendInput[Binding.SpeedAxis] = S.SmoothedAnimSpeed;
			Node->SetBlendSpacePosition(BlendInput);
		}
		return;
	}

	// 兜底: 若未提供 BlendSpace, 按单兵移动/警戒状态挑安全非叠加单段动画
	UAnimationAsset* Fallback = nullptr;
	if (S.bAnimWalking)
	{
		Fallback = AnimMove.Get();
	}
	else if (S.PerceivedAlertness == ELegionAlertness::Relaxed && AnimRelaxedIdle)
	{
		Fallback = AnimRelaxedIdle.Get();
	}
	else
	{
		Fallback = AnimIdle.Get();
	}
	if (LegionUnitInternal::IsSafeSingleNodeAsset(Fallback))
	{
		if (!Single || Single->GetCurrentAsset() != Fallback)
		{
			C->PlayAnimation(Fallback, true);
			C->SetPosition(FMath::Frac(S.Phase * 0.61f), false);
			S.AnimSuiteHoldTimer = 1.2f;
		}
	}
}

void ALegionUnit::SpawnSkeletalSoldier(FLegionSoldier& S)
{
	USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
	C->SetupAttachment(PickBox);
	C->SetSkeletalMeshAsset(SkeletalBodyMesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetComponentTickEnabled(true);
	C->SetRelativeLocationAndRotation(S.LocalOffset,
		FRotator(0.0f, S.CurYaw - GetActorRotation().Yaw - 90.0f, 0.0f));
	const float OrganicScale = 0.93f + 0.14f * FMath::Frac(S.Phase * 2.13f);
	C->SetRelativeScale3D(FVector(OrganicScale));
	if (SoldierBodyMaterial)
	{
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			C->SetMaterial(i, SoldierBodyMaterial);
		}
	}
	else if (SkeletalTint.R >= 0.0f)
	{
		// 保留 SKM_Manny_Simple 原生材质槽, 经 MID 的 "Paint Tint" 参数统一染出敌我阵营色
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = C->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetVectorParameterValue(TEXT("Paint Tint"), SkeletalTint);
				MID->SetVectorParameterValue(TEXT("Tint"), SkeletalTint);
				MID->SetVectorParameterValue(TEXT("Color"), SkeletalTint);
			}
		}
	}
	// 动画更新率优化 (URO): 按屏幕占比自适应跳帧求值, 士兵在 RTS 视距下占比小自然降频,
	// 近景特写自动恢复全速率; 跳过帧由 TrailMode 累积时间补偿, 动作速度不变只降远处刷新率
	{
		static const bool bSoldierURO = !FParse::Param(FCommandLine::Get(), TEXT("LegionNoURO"));
		C->bEnableUpdateRateOptimizations = bSoldierURO;
	}
	C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (RelaxedBlendSpace && RelaxedBSBinding.bValid && LegionUnitInternal::IsSafeSingleNodeAsset(RelaxedBlendSpace))
	{
		C->SetAnimation(RelaxedBlendSpace);
		C->SetPosition(FMath::Frac(S.Phase * 0.37f), false);
		C->Play(true);
		if (UAnimSingleNodeInstance* Single = C->GetSingleNodeInstance())
		{
			Single->SetPlayRate(S.TraitPlayRate);
			Single->SetBlendSpacePosition(FVector::ZeroVector);
		}
	}
	else if (UAnimationAsset* StartAnim = LegionUnitInternal::IsSafeSingleNodeAsset(AnimRelaxedIdle) ? AnimRelaxedIdle.Get() : AnimIdle.Get())
	{
		C->SetAnimation(StartAnim);
		C->SetPosition(FMath::Frac(S.Phase * 0.61f), false);
		C->Play(true);
	}
	C->RegisterComponent();

	// 子弹命中代理: 胶囊跟随骨骼组件, 只挡 WorldDynamic (手枪弹);
	// 命中可视士兵才结算, 不再依赖 PickBox 隐形大盒
	// (-LegionNoHitProxy 仅供性能诊断: 跳过物理胶囊创建, 玩家射击将无命中, 兵团交战不受影响)
	static const bool bNoHitProxy = FParse::Param(FCommandLine::Get(), TEXT("LegionNoHitProxy"));
	UCapsuleComponent* Proxy = nullptr;
	if (!bNoHitProxy)
	{
		Proxy = NewObject<UCapsuleComponent>(this, NAME_None, RF_Transient);
		Proxy->SetupAttachment(C);
		Proxy->SetCapsuleSize(36.0f, 92.0f);
		Proxy->SetRelativeLocation(FVector(0.0f, 0.0f, 92.0f));
		Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);
		Proxy->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Proxy->SetCanEverAffectNavigation(false);
		// 命中胶囊无需自身 tick: 变换跟随父骨骼组件由挂接系统驱动, 物理查询与 tick 无关
		Proxy->SetComponentTickEnabled(false);
		Proxy->RegisterComponent();
	}

	// FP 模板枪械: 手枪静态网格附加到手部骨骼, 随射击动画一起运动
	{
		static TWeakObjectPtr<UStaticMesh> CachedPistolMesh;
		UStaticMesh* PistolMesh = CachedPistolMesh.Get();
		if (!PistolMesh)
		{
			if (UERAssetCatalog* Catalog = UERAssetCatalog::Get(this))
			{
				PistolMesh = Catalog->LoadAsset<UStaticMesh>(TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol"));
				CachedPistolMesh = PistolMesh;
			}
		}
		if (PistolMesh)
		{
			UStaticMeshComponent* Gun = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Gun->SetupAttachment(C, TEXT("hand_r"));
			Gun->SetStaticMesh(PistolMesh);
			Gun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Gun->SetCanEverAffectNavigation(false);
			// 手枪无需自身 tick: socket 挂接跟随在父骨骼刷新时统一求解
			Gun->SetComponentTickEnabled(false);
			Gun->RegisterComponent();
			S.GunComp = Gun;

			static bool bLoggedGunProbe = false;
			if (!bLoggedGunProbe)
			{
				bLoggedGunProbe = true;
				const int32 BoneIdx = C->GetBoneIndex(TEXT("hand_r"));
				UE_LOG(LogERLegion, Log, TEXT("[Legion][GunProbe] mesh=%s bone hand_r idx=%d gunpos=%s attachparent=%s"),
					PistolMesh ? TEXT("ok") : TEXT("null"), BoneIdx,
					*Gun->GetComponentLocation().ToCompactString(),
					Gun->GetAttachParent() ? *Gun->GetAttachParent()->GetName() : TEXT("null"));
			}
		}
		else
		{
			static bool bLoggedGunFail = false;
			if (!bLoggedGunFail)
			{
				bLoggedGunFail = true;
				UE_LOG(LogERLegion, Warning, TEXT("[Legion][GunProbe] pistol mesh load FAILED"));
			}
		}
	}

	S.SkelComp = C;
	S.HitProxy = Proxy;
}

void ALegionUnit::ApplyAnimStateToSoldiers()
{
	if (!bSkeletalSoldiers) { return; }
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		USkeletalMeshComponent* C = S.SkelComp.Get();
		if (!C || !S.bAlive) { continue; }
		if (UAnimationAsset* A = PickAnimForState(State, i))
		{
			if (LegionUnitInternal::IsSafeSingleNodeAsset(A))
			{
				C->PlayAnimation(A, true);
				C->SetPosition(S.Phase * 1.37f, false);
			}
		}
	}
}

UAnimationAsset* ALegionUnit::PickAnimForState(ELegionState InState, int32 SoldierIndex) const
{
	switch (InState)
	{
	case ELegionState::Moving:
		return AnimMove ? AnimMove.Get() : AnimIdle.Get();
	case ELegionState::Engaged:
		if (AnimAttackPool.Num() > 0)
		{
			return AnimAttackPool[SoldierIndex % AnimAttackPool.Num()].Get();
		}
		return AnimIdle ? AnimIdle.Get() : AnimMove.Get();
	case ELegionState::Holding:
	default:
		return AnimIdle ? AnimIdle.Get() : AnimMove.Get();
	}
}

void ALegionUnit::EnableSoldierRagdoll(USkeletalMeshComponent* C, int32 SoldierIndex)
{
	if (!C || C->IsSimulatingPhysics()) { return; }

	if (USkeletalMesh* Mesh = C->GetSkeletalMeshAsset())
	{
		if (!Mesh->GetPhysicsAsset())
		{
			if (!RagdollPhysicsAsset)
			{
				if (UERAssetCatalog* Catalog = UERAssetCatalog::Get(this))
				{
					RagdollPhysicsAsset = Catalog->LoadAsset<UPhysicsAsset>(
						TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
				}
			}
			if (RagdollPhysicsAsset)
			{
				C->SetPhysicsAsset(RagdollPhysicsAsset);
			}
		}
	}

	C->bBlendPhysics = true;
	C->SetCollisionProfileName(TEXT("Ragdoll"));
	C->SetSimulatePhysics(true);

	FVector Launch(1.0f, 0.0f, 0.0f);
	if (Soldiers.IsValidIndex(SoldierIndex) && !Soldiers[SoldierIndex].DeathImpulseDir.IsNearlyZero())
	{
		Launch = Soldiers[SoldierIndex].DeathImpulseDir.GetSafeNormal2D();
	}
	else if (const ALegionUnit* Attacker = LastAttacker.Get())
	{
		Launch = (C->GetComponentLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
		if (Launch.IsNearlyZero()) { Launch = FVector(1.0f, 0.0f, 0.0f); }
	}
	const float Phase = Soldiers.IsValidIndex(SoldierIndex) ? Soldiers[SoldierIndex].Phase : 0.0f;
	const float Side = FMath::Sin(Phase * 3.1f) * 0.45f;
	Launch = (Launch + FVector(0.0f, Side, 0.35f)).GetSafeNormal();
	C->AddImpulse(Launch * 560.0f, NAME_None, true);
}

void ALegionUnit::WriteSkeletalSoldierRepresentation(float DeltaSeconds)
{
	if (!bSkeletalSoldiers) { return; }
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_SkeletalRepresentation);
	LegionPerf::FScope PerfScope(LegionPerf::Get().SkeletalRepMs);

	// 远距手枪隐藏: RTS 俯瞰视距下手枪只剩几个像素, 隐藏可减半士兵 draw call;
	// 带滞回阈值避免边界闪烁, -LegionGunCullDist=0 关闭 (cm, 默认 4000)
	static float GunCullDist = -1.0f;
	if (GunCullDist < 0.0f)
	{
		GunCullDist = 4000.0f;
		float CmdDist = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("LegionGunCullDist="), CmdDist)) { GunCullDist = CmdDist; }
	}
	if (GunCullDist > 0.0f)
	{
		FVector CamLoc = GetActorLocation();
		bool bHaveCam = false;
		if (const UWorld* W = GetWorld())
		{
			if (const APlayerController* PC = W->GetFirstPlayerController())
			{
				if (const APlayerCameraManager* CM = PC->PlayerCameraManager)
				{
					CamLoc = CM->GetCameraLocation();
					bHaveCam = true;
				}
			}
		}
		if (bHaveCam)
		{
			const float DistSq = FVector::DistSquared(CamLoc, GetActorLocation());
			const float HideAt = GunCullDist + 400.0f;
			const float ShowAt = GunCullDist - 400.0f;
			bool bWantHide = bGunsHiddenForDistance;
			if (DistSq > HideAt * HideAt) { bWantHide = true; }
			else if (DistSq < ShowAt * ShowAt) { bWantHide = false; }
			if (bWantHide != bGunsHiddenForDistance)
			{
				bGunsHiddenForDistance = bWantHide;
				for (FLegionSoldier& S : Soldiers)
				{
					if (UStaticMeshComponent* Gun = S.GunComp.Get())
					{
						Gun->SetVisibility(!bWantHide);
					}
				}
			}
		}
	}

	const float ActorYaw = GetActorRotation().Yaw;
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		if (!S.bAlive) { continue; }

		if (USkeletalMeshComponent* C = S.SkelComp.Get())
		{
			// 单次写入位置+旋转, 避免两次 UpdateComponentToWorld 级联 (命中胶囊/手枪挂在下面);
			// 驻立士兵变换不变时整帧跳过, 省去物理体更新/渲染变换提交/socket 重解的全部下游成本
			const float NewYaw = S.CurYaw - ActorYaw - 90.0f;
			const bool bUnchanged = S.LastRepLoc.Equals(S.LocalOffset, 0.2f)
				&& FMath::Abs(FMath::FindDeltaAngleDegrees(S.LastRepYaw, NewYaw)) < 0.05f;
			if (!bUnchanged)
			{
				C->SetRelativeLocationAndRotation(S.LocalOffset, FRotator(0.0f, NewYaw, 0.0f));
				S.LastRepLoc = S.LocalOffset;
				S.LastRepYaw = NewYaw;
			}
		}

		// 仅对右手手枪模型施加平滑微后坐力, 不摇晃士兵全身骨骼; 远距离隐藏时跳过后坐写入
		if (!bGunsHiddenForDistance)
		{
			if (UStaticMeshComponent* Gun = S.GunComp.Get())
			{
				const float RecoilCurve = (S.FireRecoilAlpha > 0.01f) ? FMath::Sin(S.FireRecoilAlpha * PI) : 0.0f;
				Gun->SetRelativeLocationAndRotation(FVector(-2.0f * RecoilCurve, 0.0f, 0.5f * RecoilCurve),
					FRotator(6.0f * RecoilCurve, 0.0f, 0.0f));
			}
		}

		DriveSoldierAnimation(S, i, DeltaSeconds);
	}
}

void ALegionUnit::WriteAllSoldierTransforms(float TimeSeconds)
{
	if (bSkeletalSoldiers) { return; }
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_InstancedTransforms);
	const FLegionInstancedTransformParams InstancedParams = BuildInstancedTransformParams();
	bool bWroteBody = false;
	bool bWroteHead = false;
	for (int32 i = 0; i < Soldiers.Num(); ++i)
	{
		FLegionSoldier& S = Soldiers[i];
		FTransform BodyXf, HeadXf;
		ComputeSoldierTransforms(S, TimeSeconds, InstancedParams, BodyXf, HeadXf);
		if (S.InstanceIndex != INDEX_NONE)
		{
			SoldierHISM->UpdateInstanceTransform(S.InstanceIndex, BodyXf, false, false, true);
			bWroteBody = true;
		}
		if (S.ExtraIndex != INDEX_NONE)
		{
			ExtraHISM->UpdateInstanceTransform(S.ExtraIndex, HeadXf, false, false, true);
			bWroteHead = true;
		}
	}
	if (bWroteBody) { SoldierHISM->MarkRenderStateDirty(); }
	if (bWroteHead) { ExtraHISM->MarkRenderStateDirty(); }
}

ALegionUnit::FLegionInstancedTransformParams ALegionUnit::BuildInstancedTransformParams() const
{
	FLegionInstancedTransformParams Params;

	const float SoldierHeight = 165.0f * SoldierScale;
	float LiftB = 0.0f;
	const float SB = LegionUnitInternal::NormalizeMesh(SoldierHISM->GetStaticMesh(), SoldierHeight, LiftB);
	Params.BodyLocal = FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, LiftB),
		FVector(SB * SoldierXYScale, SB * SoldierXYScale, SB));

	// 附属层按兵种塑造明显剪影差异 (盾墙 / 长戟 / 刀锋 / 狙击枪)
	FVector GearOffset(48.0f, 0.0f, 95.0f);
	FVector GearScale(0.18f, 0.65f, 1.0f);
	FRotator GearRot = FRotator::ZeroRotator;
	if (Archetype == ELegionClass::Shieldbearer)
	{
		GearOffset = FVector(52.0f, 10.0f, 88.0f);
		GearScale = FVector(0.14f, 0.85f, 1.25f); // 宽大防暴盾牌
	}
	else if (Archetype == ELegionClass::Pikeman)
	{
		GearOffset = FVector(68.0f, 22.0f, 120.0f);
		GearScale = FVector(1.85f, 0.08f, 0.08f); // 前伸长戟
		GearRot = FRotator(-18.0f, 0.0f, 0.0f);
	}
	else if (Archetype == ELegionClass::Vanguard)
	{
		GearOffset = FVector(45.0f, -20.0f, 102.0f);
		GearScale = FVector(0.75f, 0.10f, 0.14f); // 突击战刃
		GearRot = FRotator(12.0f, -15.0f, 0.0f);
	}
	else if (Archetype == ELegionClass::Marksman)
	{
		GearOffset = FVector(56.0f, 8.0f, 112.0f);
		GearScale = FVector(1.05f, 0.10f, 0.12f); // 长管步枪
	}
	Params.GearLocal = FTransform(GearRot, GearOffset * SoldierScale, GearScale * SoldierScale);
	Params.ActorYaw = GetActorRotation().Yaw;
	return Params;
}

void ALegionUnit::ComputeSoldierTransforms(const FLegionSoldier& S, float TimeSeconds,
	const FLegionInstancedTransformParams& Params, FTransform& OutBody, FTransform& OutHead) const
{
	const bool bUseHomeOffset = S.LocalOffset.IsNearlyZero(0.5f);
	FVector Offset = (!bUseHomeOffset ? S.LocalOffset : S.HomeOffset) + FVector(0.0f, 0.0f, S.DieT >= 0.0f ? S.GroundZ : 0.0f);
	if (bUseHomeOffset) { Offset.Z = S.GroundZ; }
	FVector Shrink = FVector(1.0f);
	const float BaseYaw = S.CurYaw - Params.ActorYaw + S.JitterYaw;
	FRotator Rot = FRotator(0.0f, BaseYaw, 0.0f);
	const float Speed2DSq = S.WorldVelXY.SizeSquared();

	if (S.DieT >= 0.0f)
	{
		const float T = S.DieT;
		const float K = FMath::Max(0.001f, (1.0f - T) * (1.0f - T));
		Rot = FRotator(85.0f * T, BaseYaw, 0.0f);
		Offset.Z -= 46.0f * T;
		Shrink = FVector(K);
	}
	else if (Speed2DSq > 400.0f)
	{
		const float Speed2D = FMath::Sqrt(Speed2DSq);
		const float Freq = FMath::Clamp(Speed2D / 72.0f, 3.2f, 8.5f);
		const float W = TimeSeconds * Freq + S.Phase;
		const float AmpMul = FMath::Clamp(Speed2D / 360.0f, 0.35f, 1.0f);
		const float SinW = FMath::Sin(W);
		Offset.Z += FMath::Abs(SinW) * MarchBobAmplitude * AmpMul;
		Rot = FRotator(SinW * 5.0f * AmpMul, BaseYaw, 0.0f);
	}
	else if (S.SoldierState == ELegionSoldierState::CombatAimFire || State == ELegionState::Engaged)
	{
		const float W = TimeSeconds * 3.8f + S.Phase;
		Offset.Z += FMath::Sin(W) * 1.8f;
		Rot = FRotator(-S.FireRecoilAlpha * 6.0f, BaseYaw, 0.0f);
	}
	else
	{
		// 放松呼吸微摆
		const float W = TimeSeconds * 1.9f + S.Phase;
		Offset.Z += FMath::Sin(W) * 1.6f;
	}

	const FTransform SoldierFrame(Rot, Offset, Shrink);
	OutBody = SoldierFrame * Params.BodyLocal;
	OutHead = SoldierFrame * Params.GearLocal;
}
