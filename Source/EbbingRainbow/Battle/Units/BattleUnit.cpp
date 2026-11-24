// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Units/BattleUnit.h"
#include "Battle/Units/TurretAnimInstance.h"
#include "Core/ERLog.h"
#include "Battle/Core/UBattleContext.h"
#include "Battle/Core/BattleManager.h"
#include "Core/ERAssetCatalog.h"
#include "Core/ERCollisionChannels.h"
#include "Battle/Skills/BattleSkillData.h"
#include "Battle/Skills/BattleSkillComponent.h"
#include "Battle/Skills/BattleEventHub.h"
#include "Battle/Skills/BattleResolver.h"
#include "Battle/Skills/BattleSkillLibrary.h"
#include "Battle/Terrain/BattleTerrainQuery.h"
#include "Battle/FX/BattleFloatingText.h"
#include "UI/ERUnitLabelWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
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

const TArray<FBattleSkillSpec> ABattleUnit::EmptySkillSpecs;

ABattleUnit::ABattleUnit()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;

	SkillComp = CreateDefaultSubobject<UBattleSkillComponent>(TEXT("SkillComp"));

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(40.0f, 88.0f);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);       // 供鼠标拾取
	Capsule->SetCollisionResponseToChannel(ER_TRACE_BATTLELOS, ECR_Ignore); // 战斗视线穿透
	GetMesh()->SetCollisionResponseToChannel(ER_TRACE_BATTLELOS, ECR_Ignore);

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

	LabelWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("LabelWidgetComp"));
	LabelWidgetComp->SetupAttachment(GetRootComponent());
	LabelWidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f + 56.0f));
	LabelWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	LabelWidgetComp->SetDrawAtDesiredSize(true);
	LabelWidgetComp->SetPivot(FVector2D(0.5f, 1.0f));
	LabelWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelWidgetComp->SetCanEverAffectNavigation(false);
	LabelWidgetComp->SetWidgetClass(UERUnitLabelWidget::StaticClass());
}

void ABattleUnit::EnsurePresentationAssets()
{
	if (bPresentationReady && PresentationTeam == Team)
	{
		return;
	}
	bPresentationReady = true;
	PresentationTeam = Team;

	UERAssetCatalog* Catalog = UERAssetCatalog::Get(this);
	if (!Catalog)
	{
		return; // 无世界 (CDO/编辑器预览): 保持白模圆柱
	}

	AttackAnimSequence = nullptr;
	MoveAnimSequence = nullptr;
	MoveMontage = nullptr;
	AttackAnimations.Reset();
	HitReactionFrontAnimations.Reset();
	HitReactionBackAnimations.Reset();
	DeathAnimations.Reset();
	SkelMeshMIDs.Reset();

	if (Team == EBattleTeam::Enemy)
	{
		// 默认敌方棋子: 科幻四足炮台 + 纯 C++ 程序化骨骼动画 + PA_scifiturret 布娃娃
		if (USkeletalMesh* TurretMesh = Catalog->LoadAsset<USkeletalMesh>(
			TEXT("/Game/SciFiturretasset/Meshes/SKM_scifiturret.SKM_scifiturret")))
		{
			GetMesh()->SetSkeletalMeshAsset(TurretMesh);
			GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
			GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
			GetMesh()->SetRelativeScale3D(FVector(0.78f));
			if (BodyMesh)
			{
				BodyMesh->SetVisibility(false);
			}
		}

		GetMesh()->SetAnimInstanceClass(UTurretAnimInstance::StaticClass());
		if (UTurretAnimInstance* TurretAnim = Cast<UTurretAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			TurretAnim->SetSeedPhase((float)(GetUniqueID() % 97) * 0.73f);
		}
		if (LabelWidgetComp)
		{
			LabelWidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f + 72.0f));
		}
		return;
	}

	// 我方默认棋子: 接入 UE Manny 人形骨骼网格体与动画资产。
	// 原先在构造函数里 ConstructorHelpers 硬引用 14 个资产 —— 每张图加载 CDO 都拖一遍
	// (大厅也不放过); 挪到运行时 BeginPlay/InitUnit, 目录缓存后同阵营共享
	if (USkeletalMesh* Manny = Catalog->LoadAsset<USkeletalMesh>(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		GetMesh()->SetSkeletalMeshAsset(Manny);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		GetMesh()->SetRelativeScale3D(FVector(1.0f));
		if (BodyMesh)
		{
			BodyMesh->SetVisibility(false);
		}
	}

	if (LabelWidgetComp)
	{
		LabelWidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f + 56.0f));
	}

	if (UClass* ABP = Catalog->LoadAsset<UClass>(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")))
	{
		GetMesh()->SetAnimInstanceClass(ABP);
	}

	AttackAnimSequence = Catalog->LoadAsset<UAnimSequence>(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));

	// 移动循环动画: TP 模板原地慢跑 (MF_* 是 in-place 版, 无根位移)
	MoveAnimSequence = Catalog->LoadAsset<UAnimSequence>(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));

	// 一次性动画资产池 (硬引用由 UPROPERTY 数组持有, 不会被 GC 回收)
	auto LoadAnims = [Catalog](const TCHAR* const* Paths, int32 Count, TArray<TObjectPtr<UAnimSequence>>& OutPool)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			if (UAnimSequence* Seq = Catalog->LoadAsset<UAnimSequence>(Paths[i]))
			{
				OutPool.Add(Seq);
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
}

void ABattleUnit::BeginPlay()
{
	Super::BeginPlay();
	EnsurePresentationAssets();

	if (UBattleContext* Ctx = UBattleContext::Get(GetWorld()))
	{
		Ctx->RegisterUnit(this);
	}

	if (BodyMesh && !BodyMID)
	{
		BodyMID = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (TeamDisc && !DiscMID)
	{
		DiscMID = TeamDisc->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (GetMesh() && Team != EBattleTeam::Enemy)
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
	// 有效先攻 (状态可改先攻) 决定移动转化率
	const float EffectiveInitiative = SkillComp ? SkillComp->GetEffectiveInitiative() : (float)Stats.Initiative;
	return FMath::Max(1.0f, EffectiveInitiative) * CmPerInitiativeUnit;
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

void ABattleUnit::DrainBarrier(float Amount)
{
	if (Amount > 0.0f && Barrier > 0.0f)
	{
		const float Drained = FMath::Min(Barrier, Amount);
		Barrier -= Drained;
		RefreshLabel();
		const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);
		ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc, FString::Printf(TEXT("-%d 护盾"), FMath::RoundToInt(Drained)), FColor(120, 160, 220), 36.0f);
	}
}

void ABattleUnit::ApplyHeal(float Amount, ABattleUnit* Source)
{
	if (!IsAlive() || Amount <= 0.0f)
	{
		return;
	}

	const float MaxIntegrity = SkillComp ? SkillComp->GetEffectiveMaxIntegrity() : Stats.MaxIntegrity;
	const float HealAmount = FMath::Min(Amount, MaxIntegrity - Integrity);
	if (HealAmount <= 0.0f)
	{
		return;
	}

	Integrity += HealAmount;
	RefreshLabel();
	const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);
	ABattleFloatingText::SpawnFloatingText(GetWorld(), FloatingLoc,
		FString::Printf(TEXT("+%d 完整度"), FMath::RoundToInt(HealAmount)), FColor(80, 255, 120), 38.0f);
}

float ABattleUnit::ApplyDamageAmount(float DamageAmount, ABattleUnit* Attacker, EDamageType DamageType,
	bool bCrit, float* OutBarrierAbsorbed, float* OutIntegrityDamage)
{
	if (OutBarrierAbsorbed)
	{
		*OutBarrierAbsorbed = 0.0f;
	}
	if (OutIntegrityDamage)
	{
		*OutIntegrityDamage = 0.0f;
	}

	if (!IsAlive() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const FVector FloatingLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f);

	// 1. 护盾吸收 (Barrier)
	float BarrierAbsorbed = 0.0f;
	if (Barrier > 0.0f)
	{
		BarrierAbsorbed = FMath::Min(Barrier, DamageAmount);
		Barrier -= BarrierAbsorbed;
		DamageAmount -= BarrierAbsorbed;
	}

	// 2. 扣除完整度 (Integrity)
	const float ActualIntegrityDamage = FMath::Min(Integrity, DamageAmount);
	Integrity -= ActualIntegrityDamage;

	if (OutBarrierAbsorbed)
	{
		*OutBarrierAbsorbed = BarrierAbsorbed;
	}
	if (OutIntegrityDamage)
	{
		*OutIntegrityDamage = ActualIntegrityDamage;
	}

	// 3. 飘字与反馈
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
		DamageText = FString::Printf(TEXT("-%d"), FMath::RoundToInt(ActualIntegrityDamage));
	}

	// 伤害越大字越大: 重击与弱点一眼可辨 (暴击再放大)
	const float TotalDealt = ActualIntegrityDamage + BarrierAbsorbed;
	const float DamageTextSize = FMath::Clamp(36.0f + TotalDealt * 0.55f + (bCrit ? 8.0f : 0.0f), 36.0f, 64.0f);
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

	// 4. 阵亡结算: 倒地动画 -> 布娃娃 -> 沿受击方向击飞
	if (!IsAlive())
	{
		Integrity = 0.0f;
		if (GetCapsuleComponent())
		{
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		UpdateVisualColor();
		RefreshLabel();
		StartDeathRagdoll(Attacker, TotalDealt);
		OnUnitDied.Broadcast(this);
	}

	return ActualIntegrityDamage + BarrierAbsorbed;
}

float ABattleUnit::TakeBattleDamage(float DamageAmount, ABattleUnit* Attacker, EDamageType DamageType)
{
	// 兼容入口: 无命中判定的直接伤害, 仍走抗性/护盾管线
	return UBattleResolver::ApplyDirectDamage(Attacker, this, DamageType, DamageAmount);
}

void ABattleUnit::InitUnit(const FBattleUnitStats& InStats, EBattleTeam InTeam)
{
	Stats = InStats;
	Team = InTeam;
	Integrity = Stats.MaxIntegrity;
	AP = Stats.MaxAP;
	Barrier = 0.0f;

	// SpawnActor->BeginPlay 时 Team 尚为默认 Player；此处确定敌我阵营后再切换表现资产
	EnsurePresentationAssets();

	// 技能系统 2.0: 技能由 Id 经内置目录装配 (肉鸽构筑持久化同款路径)
	if (SkillComp)
	{
		SkillComp->ClearSkills();
		if (Stats.SkillIds.Num() > 0)
		{
			SkillComp->GrantSkillsByIds(Stats.SkillIds);
		}
		else
		{
			// 默认演示技能组 (覆盖力学/热学/电学/化学四学派)
			SkillComp->GrantSkillsByIds({ TEXT("Slash"), TEXT("Shoot"), TEXT("Overheat"), TEXT("Shock") });
		}
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

void ABattleUnit::ApplyPersistentIntegrity(float InIntegrity)
{
	Integrity = FMath::Clamp(InIntegrity, 0.0f, Stats.MaxIntegrity);
	Barrier = 0.0f;

	UpdateVisualColor();
	UpdateDiscVisual(0.0f);
	RefreshLabel();
}

void ABattleUnit::ResetAP()
{
	AP = SkillComp ? SkillComp->GetEffectiveMaxAP() : Stats.MaxAP;
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

	if (LabelWidgetComp)
	{
		if (!LabelWidgetComp->GetUserWidgetObject() && GetWorld() && GetWorld()->IsGameWorld())
		{
			LabelWidgetComp->InitWidget();
		}
		if (UERUnitLabelWidget* LW = Cast<UERUnitLabelWidget>(LabelWidgetComp->GetUserWidgetObject()))
		{
			LW->SetLabelColor(BaseColor);
		}
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
	if (!LabelWidgetComp)
	{
		return;
	}
	if (!LabelWidgetComp->GetUserWidgetObject() && GetWorld() && GetWorld()->IsGameWorld())
	{
		LabelWidgetComp->InitWidget();
	}
	UERUnitLabelWidget* LW = Cast<UERUnitLabelWidget>(LabelWidgetComp->GetUserWidgetObject());
	if (!LW)
	{
		return;
	}

	if (!IsAlive())
	{
		LW->SetLabelText(FText::FromString(FString::Printf(TEXT("%s\n[已损毁]"), *Stats.DisplayName)));
		LW->SetLabelColor(FLinearColor(0.45f, 0.45f, 0.48f, 1.0f));
		return;
	}

	const float MaxIntegrity = SkillComp ? SkillComp->GetEffectiveMaxIntegrity() : Stats.MaxIntegrity;
	const int32 MaxAP = SkillComp ? SkillComp->GetEffectiveMaxAP() : Stats.MaxAP;
	const int32 EffectiveInit = FMath::RoundToInt(SkillComp ? SkillComp->GetEffectiveInitiative() : (float)Stats.Initiative);

	FString StatusStr;
	if (Barrier > 0.0f)
	{
		StatusStr = FString::Printf(TEXT("%s\n完整度 %.0f/%.0f (+%.0f护盾)  AP %d/%d  先攻 %d"),
			*Stats.DisplayName, Integrity, MaxIntegrity, Barrier, AP, MaxAP, EffectiveInit);
	}
	else
	{
		StatusStr = FString::Printf(TEXT("%s\n完整度 %.0f/%.0f  AP %d/%d  先攻 %d"),
			*Stats.DisplayName, Integrity, MaxIntegrity, AP, MaxAP, EffectiveInit);
	}

	// 状态摘要 (燃烧 x2 (1) · 导电 ...)
	if (SkillComp)
	{
		const FString StatusSummary = SkillComp->GetStatusSummary();
		if (!StatusSummary.IsEmpty())
		{
			StatusStr += FString::Printf(TEXT("\n%s"), *StatusSummary);
		}
	}

	LW->SetLabelText(FText::FromString(StatusStr));
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

	// 诊断节流: PlanMove 在移动预览悬停时每帧都会被调, 同一原因每秒只报一条
	auto LogPlanFail = [this, &Destination](const TCHAR* Reason)
	{
		static TMap<FString, double> LastLogByReason;
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		double& Last = LastLogByReason.FindOrAdd(FString(Reason), -10.0);
		if (Now - Last > 1.0)
		{
			Last = Now;
			UE_LOG(LogERBattle, Log, TEXT("[BattleInput] PlanMove 失败: %s (%s AP=%d dest=%s)"),
				Reason, *GetName(), AP, *Destination.ToCompactString());
		}
	};

	if (!IsAlive() || AP <= 0)
	{
		LogPlanFail(TEXT("单位死亡或 AP 耗尽"));
		return false;
	}

	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		LogPlanFail(TEXT("无导航系统"));
		return false;
	}

	// 投影起点与目标点至导航网格
	FNavLocation StartNav;
	if (!Nav->ProjectPointToNavigation(GetActorLocation(), StartNav, FVector(250.0f, 250.0f, 500.0f)))
	{
		LogPlanFail(TEXT("起点投影不到导航 (单位脚下无 nav)"));
		return false;
	}

	FNavLocation Projected;
	if (!Nav->ProjectPointToNavigation(Destination, Projected, FVector(350.0f, 350.0f, 600.0f)))
	{
		LogPlanFail(TEXT("目标点投影不到导航 (点击处无 nav)"));
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
		LogPlanFail(TEXT("寻路无有效路径"));
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
		UE_LOG(LogERBattle, Log, TEXT("[BattleInput] MoveTo 拒绝: %s bMoving=%d alive=%d AP=%d"),
			*GetName(), bMoving, IsAlive(), AP);
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
	PlayMoveAnimation();

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
	StopMoveAnimation();
	OnMoveFinished.Broadcast(this);
	TryAutoEndTurn();
}

void ABattleUnit::TryAutoEndTurn()
{
	// 仅我方: 敌方回合的结束时机归 AIDirector, 两边都发会重复推进回合
	if (AP > 0 || !IsAlive() || Team != EBattleTeam::Player)
	{
		return;
	}
	UBattleContext* Ctx = UBattleContext::Get(GetWorld());
	UBattleManager* Mgr = Ctx ? Ctx->GetManager() : nullptr;
	if (!Mgr || Mgr->GetActiveUnit() != this || Mgr->GetBattleState() != EBattleState::TurnActive)
	{
		return;
	}
	// 留一个节拍让命中结算/飘字落地, 触发时再复核一遍 (期间玩家可能已手动结束回合)
	const TWeakObjectPtr<ABattleUnit> Self(this);
	const TWeakObjectPtr<UBattleManager> MgrWeak(Mgr);
	GetWorldTimerManager().SetTimer(AutoEndTurnTimer,
		[Self, MgrWeak]()
		{
			UBattleManager* M = MgrWeak.Get();
			if (Self.IsValid() && M && Self->AP <= 0
				&& M->GetActiveUnit() == Self.Get()
				&& M->GetBattleState() == EBattleState::TurnActive)
			{
				M->EndCurrentTurn();
			}
		},
		0.45f, false);
}

bool ABattleUnit::CanUseSkill(int32 SkillIndex, const ABattleUnit* Target, FString& OutReason) const
{
	if (!IsAlive())
	{
		OutReason = TEXT("单位已损毁");
		return false;
	}

	if (!SkillComp || !SkillComp->HasSkillIndex(SkillIndex))
	{
		OutReason = TEXT("无效的技能索引");
		return false;
	}

	const FBattleSkillSpec* Spec = SkillComp->GetSpec(SkillIndex);
	UBattleSkillData* Skill = Spec ? Spec->SkillData.Get() : nullptr;
	if (!Skill)
	{
		OutReason = TEXT("无效的技能索引");
		return false;
	}

	// 冷却判定 (设计文档 §4 语义)
	if (Spec->IsOnCooldown())
	{
		OutReason = FString::Printf(TEXT("技能冷却中 (还需 %d 回合)"), FMath::Max(1, Spec->CooldownRemaining));
		return false;
	}

	FBattleSkillExecutionContext Context;
	Context.Caster = const_cast<ABattleUnit*>(this);
	Context.Target = const_cast<ABattleUnit*>(Target);
	Context.TargetLocation = Target ? Target->GetActorLocation() : FVector::ZeroVector;

	if (!Skill->CanActivate(this, Context, OutReason))
	{
		return false;
	}

	// 视线判定 (地形耦合: 与 WFC 几何碰撞, 单位不挡视线)
	if (Skill->bRequiresLineOfSight && Target && Target != this)
	{
		if (UBattleTerrainQuery* Terrain = UBattleTerrainQuery::Get(GetWorld()))
		{
			if (!Terrain->HasLineOfSight(const_cast<ABattleUnit*>(this), const_cast<ABattleUnit*>(Target)))
			{
				OutReason = TEXT("目标被地形遮挡 (无视线)");
				return false;
			}
		}
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

	if (!SkillComp)
	{
		return false;
	}

	FBattleSkillSpec* Spec = SkillComp->GetSpec(SkillIndex);
	UBattleSkillData* Skill = Spec ? Spec->SkillData.Get() : nullptr;
	if (!Skill)
	{
		return false;
	}

	// 1. 扣 AP + 写冷却
	AP = FMath::Max(0, AP - Skill->APCost);
	SkillComp->NotifySkillActivated(SkillIndex);
	RefreshLabel();

	// 2. 面向目标
	if (Target && Target != this)
	{
		const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	// 3. 延迟命中: 施法动画命中时刻才结算 (近战手感的关键)
	float HitDelay = 0.0f;
	if (Skill->CastAnimation)
	{
		PlayOneShotAnimation(Skill->CastAnimation);
		const float Length = Skill->CastAnimation->GetPlayLength();
		if (Length > KINDA_SMALL_NUMBER)
		{
			HitDelay = FMath::Clamp(Length * AttackHitFraction, 0.12f, 0.9f);
		}
	}
	else
	{
		HitDelay = PlayAttackAnimation();
	}

	PendingHitTarget = Target;
	PendingSkillData = Skill;
	PendingSkillLevel = Spec->Level;
	PendingTargetLocation = Target ? Target->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 250.0f;

	if (UBattleEventHub* Hub = UBattleEventHub::Get(GetWorld()))
	{
		Hub->OnSkillCastStarted.Broadcast(this, Skill, Target, PendingTargetLocation, HitDelay);
	}

	if (HitDelay > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().ClearTimer(PendingHitTimerHandle);
		GetWorldTimerManager().SetTimer(PendingHitTimerHandle, this, &ABattleUnit::ApplyPendingHit, HitDelay, false);
	}
	else
	{
		// 无动画资产: 立即结算, 保证逻辑不卡
		ApplyPendingHit();
	}

	return true;
}

void ABattleUnit::ApplyPendingHit()
{
	ABattleUnit* Target = PendingHitTarget.Get();
	UBattleSkillData* Skill = PendingSkillData;
	const int32 Level = PendingSkillLevel;
	const FVector TargetLocation = PendingTargetLocation;

	PendingHitTarget.Reset();
	PendingSkillData = nullptr;
	PendingSkillLevel = 1;
	PendingTargetLocation = FVector::ZeroVector;

	// 施法者被打断: 本次技能作废
	if (!IsAlive() || !Skill)
	{
		TryAutoEndTurn();
		return;
	}

	// 单体目标已倒: 目标类技能作废 (AoE 落点类仍结算)
	if ((!Target || !Target->IsAlive()) && !Skill->AoE.IsArea())
	{
		TryAutoEndTurn();
		return;
	}

	FBattleSkillExecutionContext Context;
	Context.Caster = this;
	Context.Target = Target;
	Context.TargetLocation = Target ? Target->GetActorLocation() : TargetLocation;

	// 全部数值结算交给 Resolver 管线 (命中/暴击/地形/反应/状态/事件)
	UBattleResolver::ResolveSkill(this, Target, Context.TargetLocation, Skill, Level, Context);
	TryAutoEndTurn();
}

void ABattleUnit::CancelPendingHit()
{
	PendingHitTarget.Reset();
	PendingSkillData = nullptr;
	PendingSkillLevel = 1;
	PendingTargetLocation = FVector::ZeroVector;
	GetWorldTimerManager().ClearTimer(PendingHitTimerHandle);
}

void ABattleUnit::PlayMoveAnimation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (UTurretAnimInstance* TurretAnim = Cast<UTurretAnimInstance>(AnimInst))
	{
		TurretAnim->SetMoving(true, MoveSpeed);
		return;
	}
	if (!MoveAnimSequence || !AnimInst)
	{
		return;
	}
	if (!MoveMontage)
	{
		// 循环次数给足, 实际停止由 StopMoveAnimation 负责 (0.12s 淡入/淡出)
		MoveMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			MoveAnimSequence, FName(TEXT("DefaultSlot")), 0.12f, 0.12f, 1.0f, 10000);
	}
	if (MoveMontage)
	{
		// 步速对齐: 基准动画约 420cm/s, 钳制播放速率减少滑步与鬼畜
		const float PlayRate = FMath::Clamp(MoveSpeed / 420.0f, 0.8f, 1.8f);
		AnimInst->Montage_Play(MoveMontage, PlayRate);
	}
}

void ABattleUnit::StopMoveAnimation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (UTurretAnimInstance* TurretAnim = Cast<UTurretAnimInstance>(AnimInst))
	{
		TurretAnim->SetMoving(false, 0.0f);
		return;
	}
	if (!MoveMontage || !AnimInst)
	{
		return;
	}
	if (AnimInst->Montage_IsPlaying(MoveMontage))
	{
		AnimInst->Montage_Stop(0.15f, MoveMontage);
	}
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
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UTurretAnimInstance* TurretAnim = Cast<UTurretAnimInstance>(MeshComp->GetAnimInstance()))
		{
			TurretAnim->TriggerAttack(0.55f);
			return 0.16f;
		}
	}

	if (AttackAnimations.Num() == 0)
	{
		return 0.0f;
	}

	const int32 Index = UBattleResolver::GetRng(GetWorld())->RandRange(0, AttackAnimations.Num() - 1);
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
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UTurretAnimInstance* TurretAnim = Cast<UTurretAnimInstance>(MeshComp->GetAnimInstance()))
			{
				const FVector LocalFrom = MeshComp->GetComponentTransform().InverseTransformVectorNoScale(ToSource);
				TurretAnim->TriggerHit(LocalFrom, 0.38f);
			}
		}
		return;
	}

	const int32 Index = UBattleResolver::GetRng(GetWorld())->RandRange(0, Pool.Num() - 1);
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
	StopMoveAnimation();
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
		const int32 Index = UBattleResolver::GetRng(GetWorld())->RandRange(0, DeathAnimations.Num() - 1);
		DeathAnim = DeathAnimations[Index];
	}
	if (DeathAnim)
	{
		PlayOneShotAnimation(DeathAnim);
	}

	// 先倒地再切物理, 过渡比瞬间布娃娃自然；无死亡动作资产 (如四足炮台) 则直接切物理塌陷
	if (!DeathAnim || RagdollBlendDelay <= KINDA_SMALL_NUMBER)
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

	// 兜底: 骨骼网格体没带物理资产时, 手动挂上 Mannequin 的 PA。
	// 走资产目录而不是 ConstructorHelpers (后者只允许在构造函数中调用)
	if (USkeletalMesh* SkelMesh = MeshComp->GetSkeletalMeshAsset())
	{
		if (!SkelMesh->GetPhysicsAsset())
		{
			if (UERAssetCatalog* Catalog = UERAssetCatalog::Get(this))
			{
				if (UPhysicsAsset* PA = Catalog->LoadAsset<UPhysicsAsset>(
					TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin")))
				{
					MeshComp->SetPhysicsAsset(PA);
				}
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

void ABattleUnit::DebugKill()
{
	if (!IsAlive())
	{
		return;
	}

	// 作弊路径: 不走闪避/抗性/护盾, 直接清零, 但后续表现与结算完全复用正常战死流程
	Integrity = 0.0f;
	Barrier = 0.0f;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UpdateVisualColor();
	RefreshLabel();

	ABattleFloatingText::SpawnFloatingText(GetWorld(),
		GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleHalfHeight() + 35.0f),
		TEXT("处决 (DEBUG)"), FColor(255, 120, 90), 44.0f);

	StartDeathRagdoll(nullptr, 60.0f);
	OnUnitDied.Broadcast(this);
}

void ABattleUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBattleContext* Ctx = UBattleContext::Get(GetWorld()))
	{
		Ctx->UnregisterUnit(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
