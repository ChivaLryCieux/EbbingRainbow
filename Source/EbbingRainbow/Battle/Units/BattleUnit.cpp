// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Units/BattleUnit.h"
#include "Battle/UI/BattleFloatingText.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

ABattleUnit::ABattleUnit()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(40.0f, 88.0f);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // 供鼠标拾取

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	Move->MaxWalkSpeed = 650.0f;
	Move->bUseRVOAvoidance = false;

	// 白模外观: 引擎自带圆柱体 (若加载了骨骼模型则隐藏)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	BodyMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.76f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, MatFinder.Object);
	}

	// 接入 UE Manny 人形骨骼网格体与动画资产
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(MannyFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		BodyMesh->SetVisibility(false);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	if (AnimFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimFinder.Class);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	if (AttackFinder.Succeeded())
	{
		AttackAnimSequence = AttackFinder.Object;
	}

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f + 70.0f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(26.0f);
	Label->SetCanEverAffectNavigation(false);
}

void ABattleUnit::BeginPlay()
{
	Super::BeginPlay();
	if (BodyMesh && !BodyMID)
	{
		BodyMID = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (GetMesh())
	{
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
			{
				SkelMeshMIDs.Add(MID);
			}
		}
	}
}

float ABattleUnit::GetCapsuleHalfHeight() const
{
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void ABattleUnit::InitUnit(const FBattleUnitStats& InStats, EBattleTeam InTeam)
{
	Stats = InStats;
	Team = InTeam;
	HP = Stats.MaxHP;
	AP = Stats.MaxAP;

	// 默认技能配置 (如果没有定制技能)
	if (Stats.Skills.Num() == 0)
	{
		Stats.Skills.Add(FBattleSkillDef(TEXT("Slash"), TEXT("Slash (近战斩击)"), 4.0f, 250.0f, 12, TEXT("近身重击目标敌军"), FLinearColor(1.0f, 0.3f, 0.1f)));
		Stats.Skills.Add(FBattleSkillDef(TEXT("Shoot"), TEXT("Shoot (远程射击)"), 5.0f, 1200.0f, 8, TEXT("远程射击敌军目标"), FLinearColor(0.2f, 0.9f, 0.4f)));
	}

	if (!BodyMID && BodyMesh)
	{
		BodyMID = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	UpdateVisualColor();
	RefreshLabel();
}

void ABattleUnit::ResetAP()
{
	AP = Stats.MaxAP;
	RefreshLabel();
}

void ABattleUnit::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	UpdateVisualColor();
}

void ABattleUnit::SetHovered(bool bInHovered)
{
	bHovered = bInHovered;
	UpdateVisualColor();
}

void ABattleUnit::UpdateVisualColor()
{
	FLinearColor BaseColor;
	if (!IsAlive())
	{
		BaseColor = FLinearColor(0.12f, 0.12f, 0.14f);
	}
	else if (Team == EBattleTeam::Player)
	{
		BaseColor = FLinearColor(0.1f, 0.45f, 1.0f);
	}
	else
	{
		BaseColor = FLinearColor(1.0f, 0.2f, 0.12f);
	}

	if (bSelected)
	{
		BaseColor = FMath::Lerp(BaseColor, FLinearColor(1.0f, 0.9f, 0.2f), 0.5f);
	}
	else if (bHovered)
	{
		BaseColor = FMath::Lerp(BaseColor, FLinearColor::White, 0.35f);
	}

	if (BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), BaseColor);
	}

	for (UMaterialInstanceDynamic* MID : SkelMeshMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Tint"), BaseColor);
			MID->SetVectorParameterValue(TEXT("Color"), BaseColor);
			MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
		}
	}

	if (Label)
	{
		Label->SetTextRenderColor(BaseColor.ToFColor(true));
	}
}

void ABattleUnit::RefreshLabel()
{
	if (Label)
	{
		if (!IsAlive())
		{
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n[DEAD]"), *Stats.DisplayName)));
			Label->SetTextRenderColor(FColor(100, 100, 100));
		}
		else
		{
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s\nHP %d/%d  AP %.1f/%.0f  SPD %d"),
				*Stats.DisplayName, HP, Stats.MaxHP, AP, Stats.MaxAP, Stats.Speed)));
		}
	}
}

bool ABattleUnit::PlanMove(const FVector& Destination, TArray<FVector>& OutPath, float& OutCostAP, bool& bOutTruncated) const
{
	OutPath.Reset();
	OutCostAP = 0.0f;
	bOutTruncated = false;

	if (!IsAlive() || AP <= 0.05f)
	{
		return false;
	}

	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		return false;
	}

	// 投影起点与目标点至导航网格
	FNavLocation StartNav;
	if (!Nav->ProjectPointToNavigation(GetActorLocation(), StartNav, FVector(250.0f, 250.0f, 500.0f)))
	{
		return false;
	}

	FNavLocation Projected;
	if (!Nav->ProjectPointToNavigation(Destination, Projected, FVector(350.0f, 350.0f, 600.0f)))
	{
		return false;
	}

	// 若目标与起点极其接近, 无需移动
	if (FVector::Dist2D(StartNav.Location, Projected.Location) < 30.0f)
	{
		return false;
	}

	UNavigationPath* NavPath = Nav->FindPathToLocationSynchronously(GetWorld(), StartNav.Location, Projected.Location);
	if (!NavPath || !NavPath->IsValid() || NavPath->PathPoints.Num() < 2)
	{
		return false;
	}

	// 按 AP 预算截断
	const float BudgetCm = AP * CmPerAP;
	float UsedCm = 0.0f;
	OutPath.Add(NavPath->PathPoints[0]);
	for (int32 i = 1; i < NavPath->PathPoints.Num(); ++i)
	{
		const FVector& A = NavPath->PathPoints[i - 1];
		const FVector& B = NavPath->PathPoints[i];
		const float SegLen = FVector::Dist(A, B);
		if (UsedCm + SegLen <= BudgetCm)
		{
			UsedCm += SegLen;
			OutPath.Add(B);
		}
		else
		{
			const float Remain = BudgetCm - UsedCm;
			if (Remain > 1.0f && SegLen > KINDA_SMALL_NUMBER)
			{
				OutPath.Add(A + (B - A) * (Remain / SegLen));
				UsedCm += Remain;
			}
			bOutTruncated = true;
			break;
		}
	}

	OutCostAP = UsedCm / CmPerAP;
	return OutPath.Num() >= 2 && OutCostAP > 0.05f;
}

bool ABattleUnit::MoveTo(const FVector& Destination)
{
	if (bMoving || !IsAlive())
	{
		return false;
	}

	TArray<FVector> Path;
	float Cost = 0.0f;
	bool bTruncated = false;
	if (!PlanMove(Destination, Path, Cost, bTruncated))
	{
		return false;
	}

	AP = FMath::Max(0.0f, AP - Cost);
	MovePath = MoveTemp(Path);
	MovePathIndex = 1;
	bMoving = true;
	RefreshLabel();

	// 启动安全看门狗: 最多允许位移 6 秒, 避免极端阻挡时无法结算
	GetWorldTimerManager().SetTimer(MoveWatchdogTimer, this, &ABattleUnit::StopMovement, 6.0f, false);
	return true;
}

void ABattleUnit::StopMovement()
{
	if (bMoving)
	{
		FinishMove();
	}
}

void ABattleUnit::FinishMove()
{
	GetWorldTimerManager().ClearTimer(MoveWatchdogTimer);
	bMoving = false;
	MovePath.Reset();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	OnMoveFinished.Broadcast(this);
}

bool ABattleUnit::CanUseSkill(int32 SkillIndex, const ABattleUnit* Target, FString& OutReason) const
{
	if (!IsAlive())
	{
		OutReason = TEXT("单位已死亡");
		return false;
	}
	if (!Stats.Skills.IsValidIndex(SkillIndex))
	{
		OutReason = TEXT("无效的技能索引");
		return false;
	}
	const FBattleSkillDef& Skill = Stats.Skills[SkillIndex];
	if (AP < Skill.APCost)
	{
		OutReason = FString::Printf(TEXT("行动值不足 (需要 %.0f, 剩余 %.1f)"), Skill.APCost, AP);
		return false;
	}
	if (!Target || !Target->IsAlive())
	{
		OutReason = TEXT("目标不存在或已死亡");
		return false;
	}
	if (Target->Team == Team)
	{
		OutReason = TEXT("不能以友军为目标");
		return false;
	}

	const float Dist = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	if (Dist > Skill.Range)
	{
		OutReason = FString::Printf(TEXT("目标超出射程 (当前 %.0fcm, 最大 %.0fcm)"), Dist, Skill.Range);
		return false;
	}

	return true;
}

bool ABattleUnit::ExecuteSkill(int32 SkillIndex, ABattleUnit* Target)
{
	FString FailReason;
	if (!CanUseSkill(SkillIndex, Target, FailReason))
	{
		return false;
	}

	const FBattleSkillDef& Skill = Stats.Skills[SkillIndex];
	AP = FMath::Max(0.0f, AP - Skill.APCost);
	RefreshLabel();

	// 面向目标
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));

	// 播放攻击动作
	if (AttackAnimSequence && GetMesh())
	{
		GetMesh()->PlayAnimation(AttackAnimSequence, false);
		GetWorldTimerManager().ClearTimer(AttackAnimTimer);
		GetWorldTimerManager().SetTimer(AttackAnimTimer, [this]() {
			if (GetMesh() && IsAlive())
			{
				GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			}
		}, 0.75f, false);
	}

	// 表现: 绘制技能击中射线
	if (UWorld* World = GetWorld())
	{
		const FVector Muzzle = GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		const FVector HitLoc = Target->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		DrawDebugLine(World, Muzzle, HitLoc, Skill.Color.ToFColor(true), false, 1.2f, 0, 5.0f);
		DrawDebugSphere(World, HitLoc, 40.0f, 12, Skill.Color.ToFColor(true), false, 1.0f);
	}

	Target->TakeBattleDamage(Skill.BaseDamage, this);
	return true;
}

int32 ABattleUnit::TakeBattleDamage(int32 DamageAmount, ABattleUnit* Attacker)
{
	if (!IsAlive())
	{
		return 0;
	}

	const int32 ActualDamage = FMath::Min(HP, DamageAmount);
	HP -= ActualDamage;

	// 生成 3D 战场浮动伤害跳字
	const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);
	const FString DamageStr = FString::Printf(TEXT("-%d"), ActualDamage);
	const FColor DamageColor = (Team == EBattleTeam::Player) ? FColor(255, 65, 65) : FColor(255, 215, 30);
	ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, DamageStr, DamageColor, 42.0f);

	// 简单受击闪烁
	if (BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, [this]() {
			UpdateVisualColor();
		}, 0.15f, false);
	}

	OnUnitDamaged.Broadcast(this, ActualDamage);
	RefreshLabel();

	if (HP <= 0)
	{
		HP = 0;
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (GetMesh())
		{
			GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
			GetMesh()->SetSimulatePhysics(true);
		}
		UpdateVisualColor();
		RefreshLabel();
		OnUnitDied.Broadcast(this);
	}

	return ActualDamage;
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 头顶文字朝向摄像机
	if (Label)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				const FVector ToCam = PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
				Label->SetWorldRotation(ToCam.Rotation());
			}
		}
	}

	// 沿路径平滑移动
	if (bMoving && MovePath.IsValidIndex(MovePathIndex))
	{
		const FVector CurrentLoc = GetActorLocation();
		FVector Waypoint = MovePath[MovePathIndex];
		Waypoint.Z = Waypoint.Z + GetCapsuleHalfHeight() + 2.0f;

		const FVector Diff = Waypoint - CurrentLoc;
		const float Dist2D = Diff.Size2D();

		const float MoveSpeed = 650.0f; // 移动速度 (cm/s)
		const float StepDist = MoveSpeed * DeltaSeconds;

		// 转向移动方向
		if (Dist2D > 1.0f)
		{
			const FRotator TargetRot = FRotator(0.0f, Diff.Rotation().Yaw, 0.0f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaSeconds, 15.0f));
		}

		// 赋予移动组件速度, 驱动 ABP 姿态混合
		GetCharacterMovement()->Velocity = Diff.GetSafeNormal() * MoveSpeed;

		if (Dist2D <= StepDist || Dist2D < 25.0f)
		{
			// 到达当前路点, 切换至下一路点
			SetActorLocation(Waypoint, false);
			++MovePathIndex;
			if (!MovePath.IsValidIndex(MovePathIndex))
			{
				FinishMove();
			}
		}
		else
		{
			// 平滑位移
			const FVector NextLoc = CurrentLoc + Diff.GetSafeNormal() * StepDist;
			SetActorLocation(NextLoc, false);
		}
	}
}
