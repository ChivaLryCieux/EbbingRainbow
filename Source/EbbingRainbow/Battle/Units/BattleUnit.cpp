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
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
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

	// 一次性动画资产池 (硬引用由 UPROPERTY 数组持有, 不会被 GC 回收)
	auto LoadAnims = [](const TCHAR* const* Paths, int32 Count, TArray<TObjectPtr<UAnimSequence>>& OutPool)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			// 注意: 不能用 static, 否则循环里只会加载第一条路径
			ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(Paths[i]);
			if (Finder.Succeeded())
			{
				OutPool.Add(Finder.Object);
			}
		}
	};

	const TCHAR* const AttackPaths[] = {
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03")
	};
	LoadAnims(AttackPaths, (int32)UE_ARRAY_COUNT(AttackPaths), AttackAnimations);

	const TCHAR* const HitFrontPaths[] = {
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_02.MM_HitReact_Front_Lgt_02"),
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_03.MM_HitReact_Front_Lgt_03"),
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01.MM_HitReact_Front_Med_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01.MM_HitReact_Front_Hvy_01")
	};
	LoadAnims(HitFrontPaths, (int32)UE_ARRAY_COUNT(HitFrontPaths), HitReactionFrontAnimations);

	const TCHAR* const HitBackPaths[] = {
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Back_Med_01.MM_HitReact_Back_Med_01")
	};
	LoadAnims(HitBackPaths, (int32)UE_ARRAY_COUNT(HitBackPaths), HitReactionBackAnimations);

	const TCHAR* const DeathPaths[] = {
		TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Left_01.MM_Death_Left_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Right_01.MM_Death_Right_01")
	};
	LoadAnims(DeathPaths, (int32)UE_ARRAY_COUNT(DeathPaths), DeathAnimations);

	// 脚底队伍光环底盘: 零资产依赖, 用 MID 调色表达敌我/选中/悬停/当前回合
	TeamDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeamDisc"));
	TeamDisc->SetupAttachment(GetRootComponent());
	TeamDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TeamDisc->SetCanEverAffectNavigation(false);
	TeamDisc->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f + 1.2f));
	TeamDisc->SetRelativeScale3D(FVector(0.92f, 0.92f, 0.02f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiscFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DiscFinder.Succeeded())
	{
		TeamDisc->SetStaticMesh(DiscFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DiscMatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DiscMatFinder.Succeeded())
	{
		TeamDisc->SetMaterial(0, DiscMatFinder.Object);
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

	if (TeamDisc && !DiscMID)
	{
		DiscMID = TeamDisc->CreateAndSetMaterialInstanceDynamic(0);
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

void ABattleUnit::ApplyModifierBundle(const FAttributeModifierBundle& Bundle, ABattleUnit* InInstigator, EDamageType DamageType)
{
	if (!IsAlive())
	{
		return;
	}

	const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);

	// 1. 完整度修改 (负数为扣减/受击伤害，正数为修复完整度)
	if (Bundle.IntegrityDelta < 0.0f)
	{
		TakeBattleDamage(-Bundle.IntegrityDelta, InInstigator, DamageType);
	}
	else if (Bundle.IntegrityDelta > 0.0f)
	{
		const float HealAmount = FMath::Min(Bundle.IntegrityDelta, Stats.MaxIntegrity - Integrity);
		Integrity += HealAmount;
		HP = FMath::RoundToInt(Integrity);
		RefreshLabel();
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, FString::Printf(TEXT("+%d 完整度"), FMath::RoundToInt(HealAmount)), FColor(80, 255, 120), 38.0f);
	}

	// 若在受到伤害后已阵亡，无需继续结算后续附加状态
	if (!IsAlive())
	{
		return;
	}

	// 2. 护盾修改
	if (!FMath::IsNearlyZero(Bundle.BarrierDelta))
	{
		AddBarrier(Bundle.BarrierDelta);
	}

	// 3. 行动点 AP 修改
	if (Bundle.APDelta != 0)
	{
		AP = FMath::Clamp(AP + Bundle.APDelta, 0, Stats.MaxAP);
		RefreshLabel();
		const FString APStr = (Bundle.APDelta > 0) ? FString::Printf(TEXT("+%d AP"), Bundle.APDelta) : FString::Printf(TEXT("%d AP"), Bundle.APDelta);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, APStr, FColor(100, 200, 255), 38.0f);
	}

	// 4. 先攻值修改
	if (Bundle.InitiativeDelta != 0)
	{
		Stats.Initiative = FMath::Max(1, Stats.Initiative + Bundle.InitiativeDelta);
		RefreshLabel();
		const FString InitStr = (Bundle.InitiativeDelta > 0) ? FString::Printf(TEXT("先攻 +%d"), Bundle.InitiativeDelta) : FString::Printf(TEXT("先攻 %d"), Bundle.InitiativeDelta);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, InitStr, FColor(255, 210, 80), 38.0f);
	}

	// 5. 闪避率修改
	if (!FMath::IsNearlyZero(Bundle.EvasionDelta))
	{
		Stats.EvasionRate = FMath::Clamp(Stats.EvasionRate + Bundle.EvasionDelta, 0.0f, 1.0f);
		const FString EvaStr = FString::Printf(TEXT("闪避率 %s%.0f%%"), Bundle.EvasionDelta > 0.0f ? TEXT("+") : TEXT(""), Bundle.EvasionDelta * 100.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, EvaStr, FColor(180, 180, 255), 36.0f);
	}

	// 6. 力学抗性
	if (!FMath::IsNearlyZero(Bundle.KineticResistanceDelta))
	{
		Stats.KineticResistance = FMath::Clamp(Stats.KineticResistance + Bundle.KineticResistanceDelta, -1.0f, 0.85f);
		const FString ResStr = FString::Printf(TEXT("力学抗性 %s%.0f%%"), Bundle.KineticResistanceDelta > 0.0f ? TEXT("+") : TEXT(""), Bundle.KineticResistanceDelta * 100.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Kinetic), 36.0f);
	}

	// 7. 热学抗性
	if (!FMath::IsNearlyZero(Bundle.ThermalResistanceDelta))
	{
		Stats.ThermalResistance = FMath::Clamp(Stats.ThermalResistance + Bundle.ThermalResistanceDelta, -1.0f, 0.85f);
		const FString ResStr = FString::Printf(TEXT("热学抗性 %s%.0f%%"), Bundle.ThermalResistanceDelta > 0.0f ? TEXT("+") : TEXT(""), Bundle.ThermalResistanceDelta * 100.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Thermal), 36.0f);
	}

	// 8. 电学抗性
	if (!FMath::IsNearlyZero(Bundle.ElectricResistanceDelta))
	{
		Stats.ElectricResistance = FMath::Clamp(Stats.ElectricResistance + Bundle.ElectricResistanceDelta, -1.0f, 0.85f);
		const FString ResStr = FString::Printf(TEXT("电学抗性 %s%.0f%%"), Bundle.ElectricResistanceDelta > 0.0f ? TEXT("+") : TEXT(""), Bundle.ElectricResistanceDelta * 100.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Electric), 36.0f);
	}

	// 9. 化学抗性
	if (!FMath::IsNearlyZero(Bundle.ChemicalResistanceDelta))
	{
		Stats.ChemicalResistance = FMath::Clamp(Stats.ChemicalResistance + Bundle.ChemicalResistanceDelta, -1.0f, 0.85f);
		const FString ResStr = FString::Printf(TEXT("化学抗性 %s%.0f%%"), Bundle.ChemicalResistanceDelta > 0.0f ? TEXT("+") : TEXT(""), Bundle.ChemicalResistanceDelta * 100.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, ResStr, GetDamageTypeColor(EDamageType::Chemical), 36.0f);
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

	// 跑动速度随先攻值浮动: 高先攻单位不仅先手, 跑得也更快
	MoveSpeed = FMath::Clamp(480.0f + (Stats.Initiative - 8) * 22.0f, 400.0f, 850.0f);

	UpdateVisualColor();
	UpdateDiscVisual(0.0f);
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

FLinearColor ABattleUnit::GetTeamBaseColor() const
{
	if (!IsAlive())
	{
		return FLinearColor(0.10f, 0.10f, 0.12f);
	}
	if (Team == EBattleTeam::Player)
	{
		return FLinearColor(0.10f, 0.45f, 1.0f);   // 我方: 冰蓝
	}
	return FLinearColor(1.0f, 0.20f, 0.12f);       // 敌方: 赤红
}

void ABattleUnit::TriggerDiscFlash()
{
	DiscFlashTimer = DiscFlashDuration;
}

void ABattleUnit::UpdateDiscVisual(float DeltaSeconds)
{
	if (!TeamDisc)
	{
		return;
	}

	// 阵亡即收起光环, 避免尸体上还飘着一圈颜色
	if (!IsAlive())
	{
		TeamDisc->SetVisibility(false);
		return;
	}
	TeamDisc->SetVisibility(true);

	FLinearColor Target = GetTeamBaseColor();
	float Scale = 1.0f;

	if (bSelected)
	{
		// 当前行动单位: 烫金 + 呼吸脉冲
		DiscPulseTime += DeltaSeconds;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(DiscPulseTime * 6.0f);
		Target = FMath::Lerp(Target, FLinearColor(1.0f, 0.85f, 0.15f), 0.72f);
		Scale = 1.14f + 0.06f * Pulse;
		Target *= 0.85f + 0.35f * Pulse;
	}
	else if (bHovered)
	{
		Target = FMath::Lerp(Target, FLinearColor::White, 0.35f);
		Scale = 1.06f;
	}

	// 受击闪白: 给骨骼网格体一个廉价的命中反馈 (材质无 Tint 参数可用)
	if (DiscFlashTimer > 0.0f)
	{
		DiscFlashTimer = FMath::Max(0.0f, DiscFlashTimer - DeltaSeconds);
		const float Flash = FMath::Clamp(DiscFlashTimer / DiscFlashDuration, 0.0f, 1.0f);
		Target = FMath::Lerp(Target, FLinearColor::White, Flash);
		Scale += 0.10f * Flash;
	}

	if (DiscMID)
	{
		DiscMID->SetVectorParameterValue(TEXT("Color"), Target);
	}
	TeamDisc->SetRelativeScale3D(FVector(0.92f * Scale, 0.92f * Scale, 0.02f));
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

	// 从 TargetModifiers 中提取预估伤害数值供卡牌角标展示
	if (InSkillData->TargetModifiers.IntegrityDelta < 0.0f)
	{
		SkillDef.BaseDamage = FMath::RoundToInt(-InSkillData->TargetModifiers.IntegrityDelta * InSkillData->HitCount);
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

	// 攻击动作 -> 在动画命中时刻才结算伤害 (近战手感的关键)
	const float HitDelay = PlayAttackAnimation();
	if (HitDelay > KINDA_SMALL_NUMBER)
	{
		PendingHitTarget = Target;
		PendingHitSkillIndex = SkillIndex;
		GetWorldTimerManager().ClearTimer(PendingHitTimerHandle);
		GetWorldTimerManager().SetTimer(PendingHitTimerHandle, this, &ABattleUnit::ApplyPendingHit, HitDelay, false);
	}
	else
	{
		// 无动画资产: 立即结算, 保证逻辑不卡
		ResolveSkillImpact(Skill, Target);
	}

	return true;
}

void ABattleUnit::ResolveSkillImpact(const FBattleSkillDef& Skill, ABattleUnit* Target)
{
	if (!Target)
	{
		return;
	}

	// 目标受击反应由 TakeBattleDamage 内部按来袭方向播放, 这里不重复触发

	// 表现: 绘制技能击中射线与落点火花
	if (UWorld* World = GetWorld())
	{
		const FVector Muzzle = GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		const FVector HitLoc = Target->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		DrawDebugLine(World, Muzzle, HitLoc, Skill.Color.ToFColor(true), false, 1.2f, 0, 5.0f);
		DrawDebugSphere(World, HitLoc, 40.0f, 12, Skill.Color.ToFColor(true), false, 1.0f);
	}

	Target->TakeBattleDamage(Skill.BaseDamage, this, Skill.DamageType);
}

void ABattleUnit::ApplyPendingHit()
{
	ABattleUnit* Target = PendingHitTarget.Get();
	const int32 SkillIndex = PendingHitSkillIndex;
	PendingHitTarget.Reset();
	PendingHitSkillIndex = INDEX_NONE;

	// 施法者被打断 / 目标已倒: 本次伤害作废
	if (!IsAlive() || !Target || !Target->IsAlive() || !Stats.Skills.IsValidIndex(SkillIndex))
	{
		return;
	}

	ResolveSkillImpact(Stats.Skills[SkillIndex], Target);
}

void ABattleUnit::CancelPendingHit()
{
	PendingHitTarget.Reset();
	PendingHitSkillIndex = INDEX_NONE;
	GetWorldTimerManager().ClearTimer(PendingHitTimerHandle);
}

UAnimMontage* ABattleUnit::GetOrCreateMontage(UAnimSequence* Animation)
{
	if (!Animation)
	{
		return nullptr;
	}

	if (TObjectPtr<UAnimMontage> Existing = MontageCache.FindRef(Animation))
	{
		return Existing;
	}

	// 0.10s 淡入/淡出: 起手和收招都不硬切
	UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		Animation, FName(TEXT("DefaultSlot")), 0.10f, 0.10f, 1.0f, 1, -1.0f, 0.0f);
	if (Montage)
	{
		MontageCache.Add(Animation, Montage);
	}
	return Montage;
}

void ABattleUnit::PlayOneShotAnimation(UAnimSequence* Animation)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!Animation || !MeshComp)
	{
		return;
	}

	// 走 ABP 的 DefaultSlot: 叠加动画 (受击/死亡) 在 AnimSingleNodeInstance 下不生效,
	// 打包构建里会静默失效, 因此统一用动态蒙太奇播放
	UAnimMontage* Montage = GetOrCreateMontage(Animation);
	if (Montage)
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			AnimInst->Montage_Play(Montage);
		}
	}
	else
	{
		MeshComp->PlayAnimation(Animation, false);
	}

	const float Length = Animation->GetPlayLength();
	GetWorldTimerManager().ClearTimer(OneShotAnimTimerHandle);
	if (Length > KINDA_SMALL_NUMBER)
	{
		// +0.12s 留给淡出, 避免恢复逻辑早于动画结束
		GetWorldTimerManager().SetTimer(OneShotAnimTimerHandle, this, &ABattleUnit::RestoreAnimBlueprint, Length + 0.12f, false);
	}
	else
	{
		RestoreAnimBlueprint();
	}
}

void ABattleUnit::RestoreAnimBlueprint()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		}
	}
}

float ABattleUnit::PlayAttackAnimation()
{
	if (AttackAnimations.Num() == 0)
	{
		return 0.0f;
	}

	const int32 Index = FMath::RandRange(0, AttackAnimations.Num() - 1);
	UAnimSequence* Anim = AttackAnimations[Index];
	if (!Anim)
	{
		return 0.0f;
	}

	PlayOneShotAnimation(Anim);

	const float Length = Anim->GetPlayLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// 命中时刻: 不早于 0.12s (给起手一个读帧), 不晚于 0.75s (避免等太久)
	const float HitDelay = FMath::Clamp(Length * AttackHitFraction, 0.12f, 0.75f);
	return HitDelay;
}

void ABattleUnit::PlayHitReaction(const FVector& FromWorldLocation)
{
	// 阵亡走布娃娃, 不再播受击
	if (bDying || !IsAlive())
	{
		return;
	}

	FVector ToSource = FromWorldLocation - GetActorLocation();
	ToSource.Z = 0.0f;
	if (ToSource.IsNearlyZero())
	{
		return;
	}
	ToSource.Normalize();

	const float Dot = FVector::DotProduct(GetActorForwardVector(), ToSource);
	const bool bFromFront = Dot >= 0.0f;

	const TArray<TObjectPtr<UAnimSequence>>& FrontPool = HitReactionFrontAnimations;
	const TArray<TObjectPtr<UAnimSequence>>& BackPool = HitReactionBackAnimations;
	const TArray<TObjectPtr<UAnimSequence>>& Pool = bFromFront
		? (FrontPool.Num() > 0 ? FrontPool : BackPool)
		: (BackPool.Num() > 0 ? BackPool : FrontPool);

	if (Pool.Num() == 0)
	{
		return;
	}

	const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
	if (UAnimSequence* Anim = Pool[Index])
	{
		PlayOneShotAnimation(Anim);
	}
}

void ABattleUnit::StartDeathRagdoll(AActor* DamageCauser, float DamageAmount)
{
	if (bDying)
	{
		return;
	}
	bDying = true;

	// 清理移动与尚未落地的延迟伤害
	CancelPendingHit();
	GetWorldTimerManager().ClearTimer(OneShotAnimTimerHandle);
	bMoving = false;
	MovePath.Reset();
	GetWorldTimerManager().ClearTimer(MoveWatchdogTimer);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	// 击飞方向: 由伤害来源指向单位, 带上抛量让尸体翻出弧线
	FVector LaunchDir(1.0f, 0.0f, 0.0f);
	if (DamageCauser)
	{
		FVector Away = GetActorLocation() - DamageCauser->GetActorLocation();
		Away.Z = 0.0f;
		if (!Away.Normalize())
		{
			Away = GetActorForwardVector();
		}
		LaunchDir = Away;
	}

	const float Strength = FMath::Clamp(DamageAmount * RagdollImpulsePerDamage, 280.0f, 1500.0f);
	PendingRagdollImpulse = (LaunchDir + FVector(0.0f, 0.0f, 0.28f)).GetSafeNormal() * Strength;

	// 先倒地动作再切物理
	UAnimSequence* DeathAnim = nullptr;
	if (DeathAnimations.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, DeathAnimations.Num() - 1);
		DeathAnim = DeathAnimations[Index];
	}
	if (DeathAnim)
	{
		PlayOneShotAnimation(DeathAnim);
	}

	// 先倒地再切物理, 过渡比瞬间布娃娃自然
	if (RagdollBlendDelay <= KINDA_SMALL_NUMBER)
	{
		EnableRagdoll();
	}
	else
	{
		GetWorldTimerManager().SetTimer(RagdollTimerHandle, this, &ABattleUnit::EnableRagdoll, RagdollBlendDelay, false);
	}
}

void ABattleUnit::EnableRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || bRagdollActive)
	{
		return;
	}

	// 兜底: 骨骼网格体没带物理资产时, 手动挂上 Mannequin 的 PA
	if (USkeletalMesh* SkelMesh = MeshComp->GetSkeletalMeshAsset())
	{
		if (!SkelMesh->GetPhysicsAsset())
		{
			static ConstructorHelpers::FObjectFinder<UPhysicsAsset> PAFinder(TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
			if (PAFinder.Succeeded())
			{
				MeshComp->SetPhysicsAsset(PAFinder.Object);
			}
		}
	}

	// 先回到 ABP 再开物理, 布娃娃才能从当前姿态平滑混合
	MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	MeshComp->bBlendPhysics = true;
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComp->SetSimulatePhysics(true);
	// 停掉仍在播放的蒙太奇, 否则动画会持续和物理争夺姿态
	if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
	{
		AnimInst->StopAllMontages(0.05f);
	}

	if (!PendingRagdollImpulse.IsNearlyZero())
	{
		// bVelChange=true: 直接作为瞬时速度, 得到干脆的击飞
		MeshComp->AddImpulse(PendingRagdollImpulse, NAME_None, true);
	}
	PendingRagdollImpulse = FVector::ZeroVector;
	bRagdollActive = true;
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

	// 伤害越大字越大: 重击与弱点一眼可辨
	const float TotalDealt = ActualIntegrityDamage + BarrierAbsorbed;
	const float DamageTextSize = FMath::Clamp(36.0f + TotalDealt * 0.55f, 36.0f, 64.0f);
	ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, DamageText, DamageColor, DamageTextSize);

	// 受击反馈: 光环闪白 + 按来袭方向播受击动作
	TriggerDiscFlash();
	PlayHitReaction(Attacker ? Attacker->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 100.0f);

	// 白模兜底路径的受击闪烁 (骨骼路径没有可用的 Tint 参数)
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

	// 6. 阵亡结算: 倒地动画 -> 布娃娃 -> 沿受击方向击飞
	if (!IsAlive())
	{
		Integrity = 0.0f;
		HP = 0;
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UpdateVisualColor();
		RefreshLabel();
		StartDeathRagdoll(Attacker, DamageAmount);
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

	// 光环状态 (选中脉冲/受击闪白)
	UpdateDiscVisual(DeltaSeconds);

	// 沿路径平滑移动
	if (bMoving && MovePath.IsValidIndex(MovePathIndex))
	{
		const FVector CurrentLoc = GetActorLocation();
		FVector Waypoint = MovePath[MovePathIndex];
		Waypoint.Z = Waypoint.Z + GetCapsuleHalfHeight() + 2.0f;

		const FVector Diff = Waypoint - CurrentLoc;
		const float Dist2D = Diff.Size2D();

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
