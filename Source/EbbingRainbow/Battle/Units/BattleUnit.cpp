// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Units/BattleUnit.h"
#include "Battle/Skills/BattleSkillData.h"
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

float ABattleUnit::GetCmPerAP() const
{
	const int32 EffectiveInitiative = FMath::Max(1, Stats.Initiative);
	return EffectiveInitiative * CmPerInitiativeUnit;
}

void ABattleUnit::AddBarrier(float Amount)
{
	if (Amount > 0.0f && IsAlive())
	{
		Barrier += Amount;
		RefreshLabel();
		const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, FString::Printf(TEXT("+%d 护盾"), FMath::RoundToInt(Amount)), FColor(80, 180, 255), 38.0f);
	}
}

void ABattleUnit::ApplyAttributeModifier(ETargetAttributeType AttrType, float Value, EAttributeModifierOp Op, ABattleUnit* InInstigator, EDamageType DamageType)
{
	if (!IsAlive())
	{
		return;
	}

	const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);

	switch (AttrType)
	{
	case ETargetAttributeType::Integrity:
		if (Value < 0.0f)
		{
			// 扣除完整度走完整的闪避与抗性减免流程
			TakeBattleDamage(-Value, InInstigator, DamageType);
		}
		else if (Value > 0.0f)
		{
			// 修复 / 恢复完整度
			const float HealAmount = FMath::Min(Value, Stats.MaxIntegrity - Integrity);
			Integrity += HealAmount;
			HP = FMath::RoundToInt(Integrity);
			RefreshLabel();
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, FString::Printf(TEXT("+%d 完整度"), FMath::RoundToInt(HealAmount)), FColor(80, 255, 120), 38.0f);
		}
		break;

	case ETargetAttributeType::Barrier:
		AddBarrier(Value);
		break;

	case ETargetAttributeType::AP:
		{
			const int32 DeltaAP = FMath::RoundToInt(Value);
			AP = FMath::Clamp(AP + DeltaAP, 0, Stats.MaxAP);
			RefreshLabel();
			const FString APStr = (DeltaAP >= 0) ? FString::Printf(TEXT("+%d AP"), DeltaAP) : FString::Printf(TEXT("%d AP"), DeltaAP);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, APStr, FColor(100, 200, 255), 38.0f);
		}
		break;

	case ETargetAttributeType::Initiative:
		{
			const int32 DeltaInit = FMath::RoundToInt(Value);
			Stats.Initiative = FMath::Max(1, Stats.Initiative + DeltaInit);
			RefreshLabel();
			const FString InitStr = (DeltaInit >= 0) ? FString::Printf(TEXT("先攻 +%d"), DeltaInit) : FString::Printf(TEXT("先攻 %d"), DeltaInit);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, InitStr, FColor(255, 210, 80), 38.0f);
		}
		break;

	case ETargetAttributeType::EvasionRate:
		{
			Stats.EvasionRate = FMath::Clamp(Stats.EvasionRate + Value, 0.0f, 1.0f);
			const FString EvaStr = FString::Printf(TEXT("闪避率 %s%.0f%%"), Value >= 0.0f ? TEXT("+") : TEXT(""), Value * 100.0f);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, EvaStr, FColor(180, 180, 255), 36.0f);
		}
		break;

	case ETargetAttributeType::KineticResistance:
		{
			Stats.KineticResistance = FMath::Clamp(Stats.KineticResistance + Value, -1.0f, 0.85f);
			const FString ResStr = FString::Printf(TEXT("力学抗性 %s%.0f%%"), Value >= 0.0f ? TEXT("+") : TEXT(""), Value * 100.0f);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Kinetic), 36.0f);
		}
		break;

	case ETargetAttributeType::ThermalResistance:
		{
			Stats.ThermalResistance = FMath::Clamp(Stats.ThermalResistance + Value, -1.0f, 0.85f);
			const FString ResStr = FString::Printf(TEXT("热学抗性 %s%.0f%%"), Value >= 0.0f ? TEXT("+") : TEXT(""), Value * 100.0f);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Thermal), 36.0f);
		}
		break;

	case ETargetAttributeType::ElectricResistance:
		{
			Stats.ElectricResistance = FMath::Clamp(Stats.ElectricResistance + Value, -1.0f, 0.85f);
			const FString ResStr = FString::Printf(TEXT("电学抗性 %s%.0f%%"), Value >= 0.0f ? TEXT("+") : TEXT(""), Value * 100.0f);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Electric), 36.0f);
		}
		break;

	case ETargetAttributeType::ChemicalResistance:
		{
			Stats.ChemicalResistance = FMath::Clamp(Stats.ChemicalResistance + Value, -1.0f, 0.85f);
			const FString ResStr = FString::Printf(TEXT("化学抗性 %s%.0f%%"), Value >= 0.0f ? TEXT("+") : TEXT(""), Value * 100.0f);
			ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Chemical), 36.0f);
		}
		break;
	}
}

void ABattleUnit::InitUnit(const FBattleUnitStats& InStats, EBattleTeam InTeam)
{
	Stats = InStats;
	Team = InTeam;
	Integrity = Stats.MaxIntegrity;
	HP = FMath::RoundToInt(Integrity);
	AP = Stats.MaxAP;
	Barrier = 0.0f;

	// 默认技能配置 (涵盖力学、热学、电学以供基础演示)
	if (Stats.Skills.Num() == 0)
	{
		Stats.Skills.Add(FBattleSkillDef(TEXT("Slash"), TEXT("Slash (近战斩击)"), 1, 250.0f, 15, EDamageType::Kinetic, TEXT("近身重击目标敌军 (力学伤害)"), FLinearColor(1.0f, 0.4f, 0.1f)));
		Stats.Skills.Add(FBattleSkillDef(TEXT("Shoot"), TEXT("Shoot (远程穿透)"), 2, 1200.0f, 12, EDamageType::Kinetic, TEXT("远程狙击敌军目标 (力学伤害)"), FLinearColor(0.2f, 0.9f, 0.4f)));
		Stats.Skills.Add(FBattleSkillDef(TEXT("Overheat"), TEXT("Overheat (热能过载)"), 2, 650.0f, 18, EDamageType::Thermal, TEXT("向目标倾泻高热粒子 (热学伤害)"), FLinearColor(1.0f, 0.15f, 0.1f)));
		Stats.Skills.Add(FBattleSkillDef(TEXT("Shock"), TEXT("Shock (电弧激荡)"), 1, 800.0f, 10, EDamageType::Electric, TEXT("发射高频电弧脉冲 (电学伤害)"), FLinearColor(0.2f, 0.7f, 1.0f)));
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
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n[已损毁]"), *Stats.DisplayName)));
			Label->SetTextRenderColor(FColor(100, 100, 100));
		}
		else
		{
			FString StatusStr;
			if (Barrier > 0.0f)
			{
				StatusStr = FString::Printf(TEXT("%s\n完整度 %.0f/%.0f (+%.0f护盾)  AP %d/%d  先攻 %d"),
					*Stats.DisplayName, Integrity, Stats.MaxIntegrity, Barrier, AP, Stats.MaxAP, Stats.Initiative);
			}
			else
			{
				StatusStr = FString::Printf(TEXT("%s\n完整度 %.0f/%.0f  AP %d/%d  先攻 %d"),
					*Stats.DisplayName, Integrity, Stats.MaxIntegrity, AP, Stats.MaxAP, Stats.Initiative);
			}
			Label->SetText(FText::FromString(StatusStr));
		}
	}
}

bool ABattleUnit::PlanMove(const FVector& Destination, TArray<FVector>& OutPath, float& OutCostAP, bool& bOutTruncated) const
{
	int32 CostInt = 0;
	const bool bRes = PlanMove(Destination, OutPath, CostInt, bOutTruncated);
	OutCostAP = (float)CostInt;
	return bRes;
}

bool ABattleUnit::PlanMove(const FVector& Destination, TArray<FVector>& OutPath, int32& OutCostAP, bool& bOutTruncated) const
{
	OutPath.Reset();
	OutCostAP = 0;
	bOutTruncated = false;

	if (!IsAlive() || AP <= 0)
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

	// 转化率: 由先攻值决定 1 点 AP 可移动的最大距离 (cm)
	const float CmPerAPVal = GetCmPerAP();
	const float BudgetCm = (float)AP * CmPerAPVal;
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

	if (UsedCm <= 10.0f)
	{
		return false;
	}

	// 向上取整计算 AP 消耗: 例如移动 260cm, 每点 AP 能走 250cm, 则需消耗 2 点 AP
	OutCostAP = FMath::Clamp(FMath::CeilToInt(UsedCm / CmPerAPVal), 1, AP);
	return OutPath.Num() >= 2;
}

bool ABattleUnit::MoveTo(const FVector& Destination)
{
	if (bMoving || !IsAlive() || AP <= 0)
	{
		return false;
	}

	TArray<FVector> Path;
	int32 Cost = 0;
	bool bTruncated = false;
	if (!PlanMove(Destination, Path, Cost, bTruncated) || Cost <= 0)
	{
		return false;
	}

	AP = FMath::Max(0, AP - Cost);
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

void ABattleUnit::EquipSkillData(UBattleSkillData* InSkillData)
{
	if (!InSkillData)
	{
		return;
	}

	EquippedSkills.Add(InSkillData);

	// 同步生成一份 FBattleSkillDef 供 UI 与控制器无缝展现
	FBattleSkillDef SkillDef;
	SkillDef.SkillId = InSkillData->SkillId;
	SkillDef.DisplayName = InSkillData->DisplayName.IsEmpty() ? InSkillData->GetName() : InSkillData->DisplayName.ToString();
	SkillDef.APCost = InSkillData->APCost;
	SkillDef.Range = InSkillData->Range;
	SkillDef.DamageType = InSkillData->GetDisciplineDamageType();
	SkillDef.Description = InSkillData->Description.ToString();
	SkillDef.Color = InSkillData->ThemeColor;

	// 从 TargetEffects 中提取预估伤害数值供卡牌角标展示
	for (const FAttributeEffectDef& Eff : InSkillData->TargetEffects)
	{
		if (Eff.TargetAttribute == ETargetAttributeType::Integrity && Eff.Value < 0.0f)
		{
			SkillDef.BaseDamage = FMath::RoundToInt(-Eff.Value * InSkillData->HitCount);
			break;
		}
	}

	Stats.Skills.Add(SkillDef);
}

bool ABattleUnit::CanUseSkill(int32 SkillIndex, const ABattleUnit* Target, FString& OutReason) const
{
	if (!IsAlive())
	{
		OutReason = TEXT("单位已损毁");
		return false;
	}

	// 优先走技能资产验证
	if (EquippedSkills.IsValidIndex(SkillIndex) && EquippedSkills[SkillIndex])
	{
		FBattleSkillExecutionContext Context;
		Context.Caster = const_cast<ABattleUnit*>(this);
		Context.Target = const_cast<ABattleUnit*>(Target);
		Context.TargetLocation = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		return EquippedSkills[SkillIndex]->CanActivate(this, Context, OutReason);
	}

	if (!Stats.Skills.IsValidIndex(SkillIndex))
	{
		OutReason = TEXT("无效的技能索引");
		return false;
	}
	const FBattleSkillDef& Skill = Stats.Skills[SkillIndex];
	if (AP < Skill.APCost)
	{
		OutReason = FString::Printf(TEXT("行动点不足 (需要 %d, 剩余 %d)"), Skill.APCost, AP);
		return false;
	}
	if (!Target || !Target->IsAlive())
	{
		OutReason = TEXT("目标不存在或已损毁");
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

	// 优先执行 UBattleSkillData 资产逻辑
	if (EquippedSkills.IsValidIndex(SkillIndex) && EquippedSkills[SkillIndex])
	{
		UBattleSkillData* SkillAsset = EquippedSkills[SkillIndex];
		AP = FMath::Max(0, AP - SkillAsset->APCost);
		RefreshLabel();

		FBattleSkillExecutionContext Context;
		Context.Caster = this;
		Context.Target = Target;
		Context.TargetLocation = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		return SkillAsset->Execute(Context);
	}

	const FBattleSkillDef& Skill = Stats.Skills[SkillIndex];
	AP = FMath::Max(0, AP - Skill.APCost);
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

	Target->TakeBattleDamage(Skill.BaseDamage, this, Skill.DamageType);
	return true;
}

float ABattleUnit::TakeBattleDamage(float DamageAmount, ABattleUnit* Attacker, EDamageType DamageType)
{
	if (!IsAlive() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);

	// 1. 闪避判定 (EvasionRate)
	if (Stats.EvasionRate > 0.0f && FMath::FRand() < Stats.EvasionRate)
	{
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, TEXT("闪避 (EVADED)"), FColor(120, 200, 255), 36.0f);
		return 0.0f;
	}

	// 2. 抗性减免计算
	float Resistance = 0.0f;
	switch (DamageType)
	{
	case EDamageType::Kinetic:    Resistance = Stats.KineticResistance; break;
	case EDamageType::Thermal:    Resistance = Stats.ThermalResistance; break;
	case EDamageType::Electric:   Resistance = Stats.ElectricResistance; break;
	case EDamageType::Chemical:   Resistance = Stats.ChemicalResistance; break;
	case EDamageType::TrueDamage: Resistance = 0.0f; break;
	}

	// 抗性减免 [-1.0f, 0.85f]
	Resistance = FMath::Clamp(Resistance, -1.0f, 0.85f);
	float MitigatedDamage = DamageAmount * (1.0f - Resistance);
	MitigatedDamage = FMath::Max(1.0f, MitigatedDamage);

	// 3. 护盾吸收 (Barrier)
	float BarrierAbsorbed = 0.0f;
	if (Barrier > 0.0f)
	{
		BarrierAbsorbed = FMath::Min(Barrier, MitigatedDamage);
		Barrier -= BarrierAbsorbed;
		MitigatedDamage -= BarrierAbsorbed;
	}

	// 4. 扣除完整度 (Integrity)
	const float ActualIntegrityDamage = FMath::Min(Integrity, MitigatedDamage);
	Integrity -= ActualIntegrityDamage;
	HP = FMath::RoundToInt(Integrity);

	// 5. 飘字与反馈
	const FColor DamageColor = GetDamageTypeColor(DamageType);
	FString DamageText;
	if (BarrierAbsorbed > 0.0f && ActualIntegrityDamage > 0.0f)
	{
		DamageText = FString::Printf(TEXT("-%d (护盾 -%d)"), FMath::RoundToInt(ActualIntegrityDamage), FMath::RoundToInt(BarrierAbsorbed));
	}
	else if (BarrierAbsorbed > 0.0f)
	{
		DamageText = FString::Printf(TEXT("[护盾吸收 %d]"), FMath::RoundToInt(BarrierAbsorbed));
	}
	else
	{
		if (Resistance <= -0.15f)
		{
			DamageText = FString::Printf(TEXT("-%d [弱点!]"), FMath::RoundToInt(ActualIntegrityDamage));
		}
		else if (Resistance >= 0.25f)
		{
			DamageText = FString::Printf(TEXT("-%d [抵抗]"), FMath::RoundToInt(ActualIntegrityDamage));
		}
		else
		{
			DamageText = FString::Printf(TEXT("-%d"), FMath::RoundToInt(ActualIntegrityDamage));
		}
	}

	ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, DamageText, DamageColor, 40.0f);

	// 受击闪烁
	if (BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, [this]() {
			UpdateVisualColor();
		}, 0.15f, false);
	}

	OnUnitDamaged.Broadcast(this, FMath::RoundToInt(ActualIntegrityDamage));
	RefreshLabel();

	// 6. 阵亡结算
	if (!IsAlive())
	{
		Integrity = 0.0f;
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

	return ActualIntegrityDamage + BarrierAbsorbed;
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
