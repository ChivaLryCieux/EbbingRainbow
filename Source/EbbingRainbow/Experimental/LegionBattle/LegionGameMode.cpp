// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Battle/Player/TacticalCameraPawn.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Core/ERAssetCatalog.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Animation/AnimationAsset.h"
#include "Camera/CameraActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ALegionGameMode::ALegionGameMode()
{
	DefaultPawnClass = ATacticalCameraPawn::StaticClass();
	PlayerControllerClass = ALegionPlayerController::StaticClass();
	HUDClass = ALegionHUD::StaticClass();
	LegionClass = ALegionUnit::StaticClass();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AllyBodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/AdvancedRealisticGlass/Models/SM_Quinn.SM_Quinn")));
	AllySkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	EnemyBodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/AdvancedRealisticGlass/Models/SM_Quinn.SM_Quinn")));
	AllyAnimIdle = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")));
	AllyAnimMove = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd")));
	AllyAnimAttacks = {
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03"))),
	};
	AllyAnimDeaths = {
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_02.MM_Death_Front_02"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Left_01.MM_Death_Left_01"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Right_01.MM_Death_Right_01"))),
	};
}

void ALegionGameMode::BeginPlay()
{
	Super::BeginPlay();

	int32 V = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-LegionLegions="), V))  { LegionsPerSide = FMath::Max(1, V); }
	if (FParse::Value(FCommandLine::Get(), TEXT("-LegionSoldiers="), V)) { SoldiersPerLegion = FMath::Max(1, V); }
	bAutoShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-LegionShot"));
}

void ALegionGameMode::LoadRuntimeAssets()
{
	UERAssetCatalog* Catalog = UERAssetCatalog::Get(this);
	UMaterialInterface* BasicMat = Catalog ? Catalog->LoadAsset<UMaterialInterface>(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")) : nullptr;

	if (UMaterialInterface* Base = AllyMaterial.IsNull() ? BasicMat : AllyMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.92f, 0.90f, 1.0f));
		CachedAllyMat = MID;
	}

	// 敌方统一使用高亮红曜石色 (如果战役合同指定敌方势力则叠势力色)
	FLinearColor EnemyTint(0.92f, 0.14f, 0.10f, 1.0f);
	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		if (Campaign->HasActiveContract())
		{
			EnemyTint = ERGalacticMeta::GetFactionColor(Campaign->GetActiveContract().EnemyFaction);
		}
	}
	EnemySkeletalTint = EnemyTint;
	if (UMaterialInterface* BaseE = EnemyMaterial.IsNull() ? BasicMat : EnemyMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseE, this);
		MID->SetVectorParameterValue(TEXT("Color"), EnemyTint);
		CachedEnemyMat = MID;
	}

	CachedAllyBody = AllyBodyMesh.IsNull() ? nullptr : AllyBodyMesh.LoadSynchronous();
	CachedEnemyBody = EnemyBodyMesh.IsNull() ? nullptr : EnemyBodyMesh.LoadSynchronous();
	CachedGearCube = Catalog ? Catalog->LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube")) : nullptr;

	CachedAllySkel = AllySkeletalMesh.IsNull() ? nullptr : AllySkeletalMesh.LoadSynchronous();
	CachedAnimIdle = AllyAnimIdle.IsNull() ? nullptr : AllyAnimIdle.LoadSynchronous();
	CachedAnimMove = AllyAnimMove.IsNull() ? nullptr : AllyAnimMove.LoadSynchronous();
	CachedAnimAttacks.Reset();
	for (const TSoftObjectPtr<UAnimationAsset>& P : AllyAnimAttacks)
	{
		if (UAnimationAsset* A = P.LoadSynchronous()) { CachedAnimAttacks.Add(A); }
	}
	CachedAnimDeaths.Reset();
	for (const TSoftObjectPtr<UAnimationAsset>& P : AllyAnimDeaths)
	{
		if (UAnimationAsset* A = P.LoadSynchronous()) { CachedAnimDeaths.Add(A); }
	}
}

void ALegionGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World) { return; }

	LoadRuntimeAssets();

	// 1. 探测地形包围盒并建立 2D 坡度导航网格
	DeployCenter = FVector::ZeroVector;
	{
		FBox TerrainBounds(ForceInit);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass() && It->GetClass()->GetName().Contains(TEXT("Landscape")))
			{
				FVector O, E;
				It->GetActorBounds(false, O, E);
				TerrainBounds += FBox::BuildAABB(O, E);
			}
		}
		if (TerrainBounds.IsValid)
		{
			const FVector C = TerrainBounds.GetCenter();
			DeployCenter = FVector(C.X, C.Y, 0.0f);
			const FVector Ex = TerrainBounds.GetExtent();
			EffectiveBattlefieldHalfExtent = FMath::Max(2500.0f, FMath::Min(Ex.X, Ex.Y) - 2500.0f);
		}
	}

	const float HalfExt = GetBattlefieldHalfExtent();
	TerrainGrid.Build(World, DeployCenter, HalfExt, 250.0f, 38.0f);
	float CenterZ = 0.0f, CenterSlope = 0.0f;
	if (TerrainGrid.SampleHeightAndSlope(DeployCenter, CenterZ, CenterSlope))
	{
		DeployCenter.Z = CenterZ;
	}

	// 生成左上角黑白等高线小地图纹理 (256x256)
	if (TerrainGrid.IsBuilt())
	{
		constexpr int32 MapPx = 256;
		TArray<FColor> Pixels;
		TerrainGrid.BuildMinimapPixels(MapPx, Pixels);
		if (Pixels.Num() == MapPx * MapPx)
		{
			MinimapTexture = UTexture2D::CreateTransient(MapPx, MapPx, PF_B8G8R8A8);
			if (MinimapTexture)
			{
				MinimapTexture->SRGB = true;
				MinimapTexture->Filter = TF_Bilinear;
				MinimapTexture->AddressX = TA_Clamp;
				MinimapTexture->AddressY = TA_Clamp;
				if (FTexturePlatformData* PD = MinimapTexture->GetPlatformData())
				{
					if (PD->Mips.Num() > 0)
					{
						void* Data = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
						FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
						PD->Mips[0].BulkData.Unlock();
						MinimapTexture->UpdateResource();
					}
				}
			}
		}
	}

	// 2. 从战役子系统读取委托编成, 或走独立演武沙盘预设
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	const bool bUseContract = Campaign && Campaign->HasActiveContract();

	PlayerLegions.Reset();
	EnemyLegions.Reset();
	PendingWaves.Reset();
	InitialPlayerCounts.Reset();
	InitialEnemySoldiersTotal = 0;

	if (bUseContract)
	{
		const FGalacticContract& Contract = Campaign->GetActiveContract();
		BattleTitleText = Contract.Title;
		BattleSubText = FString::Printf(TEXT("CONTRACT // %s VS %s · SECTOR %02d"),
			*ERGalacticMeta::GetFactionCode(Contract.Employer),
			*ERGalacticMeta::GetFactionCode(Contract.EnemyFaction),
			Contract.TargetSectorId + 1);

		// 计算我方存活兵团总数决定是否开启骨骼动画模式
		TArray<FManagedLegionData> AlivePlayerRoster;
		int32 TotalAllySoldiers = 0;
		for (const FManagedLegionData& M : Campaign->GetPlayerLegions())
		{
			if (M.AliveSoldiers > 0)
			{
				AlivePlayerRoster.Add(M);
				TotalAllySoldiers += M.AliveSoldiers;
			}
		}
		bUseAnimatedAlly = (CachedAllySkel != nullptr) && (TotalAllySoldiers <= MaxAnimatedSoldiersPerSide);

		const int32 NP = AlivePlayerRoster.Num();
		const float X0 = -0.5f * FMath::Max(0, NP - 1) * LegionSpacing;
		for (int32 i = 0; i < NP; ++i)
		{
			const FManagedLegionData& M = AlivePlayerRoster[i];
			const FVector RawPos(DeployCenter.X + X0 + i * LegionSpacing, DeployCenter.Y - LineDistance * 0.5f, DeployCenter.Z);
			const FVector SafePos = TerrainGrid.IsBuilt() ? TerrainGrid.ClampToWalkable(RawPos) : RawPos;
			if (ALegionUnit* L = SpawnLegion(ELegionTeam::Player, M.Archetype, M.AliveSoldiers,
				SafePos, 90.0f, M.LegionId, M.Callsign, M.SyncLevel))
			{
				PlayerLegions.Add(L);
				InitialPlayerCounts.Add(M.LegionId, M.AliveSoldiers);
			}
		}

		// 敌军波次: Delay<=0 立即布阵, Delay>0 存入 PendingWaves 倒计时增援
		int32 TotalEnemyContractSoldiers = 0;
		for (const FEnemyWaveLegionSpec& W : Contract.EnemyWaves)
		{
			TotalEnemyContractSoldiers += W.SoldierCount;
		}
		bUseAnimatedEnemy = (CachedAllySkel != nullptr) && (TotalEnemyContractSoldiers <= MaxAnimatedSoldiersPerSide);

		const float FrontSpan = FMath::Max(4500.0f, (float)NP * LegionSpacing * 0.8f);
		for (int32 w = 0; w < Contract.EnemyWaves.Num(); ++w)
		{
			const FEnemyWaveLegionSpec& W = Contract.EnemyWaves[w];
			InitialEnemySoldiersTotal += W.SoldierCount;

			const float WX = DeployCenter.X + W.LaneBias * FrontSpan * 0.5f;
			const float WY = DeployCenter.Y + LineDistance * (W.DelaySeconds > 0.0f ? 0.72f : 0.5f);
			const FVector DropPos = TerrainGrid.IsBuilt()
				? TerrainGrid.ClampToWalkable(FVector(WX, WY, DeployCenter.Z))
				: FVector(WX, WY, DeployCenter.Z);

			if (W.DelaySeconds <= 0.01f)
			{
				const FString Label = FString::Printf(TEXT("%s-%02d"),
					*ERGalacticMeta::GetClassCode(W.Archetype), EnemyLegions.Num() + 1);
				if (ALegionUnit* L = SpawnLegion(ELegionTeam::Enemy, W.Archetype, W.SoldierCount,
					DropPos, -90.0f, INDEX_NONE, Label, 1))
				{
					EnemyLegions.Add(L);
				}
			}
			else
			{
				FPendingEnemyWave PW;
				PW.Spec = W;
				PW.DropLocation = DropPos;
				PendingWaves.Add(PW);
			}
		}
	}
	else
	{
		// 独立沙盘演武模式: 四兵种齐上阵 (4v4 默认, 支持命令行 -LegionLegions / -LegionSoldiers 压测)
		BattleTitleText = TEXT("虹膜军团演武");
		BattleSubText = TEXT("IRIS FOUNDATION // MULTI-CORPS TACTICAL SANDBOX");

		const int32 N = FMath::Max(1, LegionsPerSide);
		bUseAnimatedAlly = (CachedAllySkel != nullptr) && (N * SoldiersPerLegion <= MaxAnimatedSoldiersPerSide);
		bUseAnimatedEnemy = bUseAnimatedAlly;
		static const ELegionClass Classes[4] = {
			ELegionClass::Shieldbearer, ELegionClass::Pikeman, ELegionClass::Vanguard, ELegionClass::Marksman
		};
		const float X0 = -0.5f * (N - 1) * LegionSpacing;
		for (int32 i = 0; i < N; ++i)
		{
			const ELegionClass C = Classes[i % 4];
			const float X = DeployCenter.X + X0 + i * LegionSpacing;

			const FVector PPos = TerrainGrid.IsBuilt()
				? TerrainGrid.ClampToWalkable(FVector(X, DeployCenter.Y - LineDistance * 0.5f, DeployCenter.Z))
				: FVector(X, DeployCenter.Y - LineDistance * 0.5f, DeployCenter.Z);
			if (ALegionUnit* L = SpawnLegion(ELegionTeam::Player, C, SoldiersPerLegion,
				PPos, 90.0f, i + 1,
				FString::Printf(TEXT("IRIS-%02d // %s"), i + 1, *ERGalacticMeta::GetClassName(C)), 1))
			{
				PlayerLegions.Add(L);
				InitialPlayerCounts.Add(i + 1, SoldiersPerLegion);
			}

			const FVector EPos = TerrainGrid.IsBuilt()
				? TerrainGrid.ClampToWalkable(FVector(X, DeployCenter.Y + LineDistance * 0.5f, DeployCenter.Z))
				: FVector(X, DeployCenter.Y + LineDistance * 0.5f, DeployCenter.Z);
			if (ALegionUnit* L = SpawnLegion(ELegionTeam::Enemy, C, SoldiersPerLegion,
				EPos, -90.0f, INDEX_NONE,
				FString::Printf(TEXT("HOSTILE-%02d // %s"), i + 1, *ERGalacticMeta::GetClassName(C)), 1))
			{
				EnemyLegions.Add(L);
				InitialEnemySoldiersTotal += SoldiersPerLegion;
			}
		}
	}

	// 3. 配置大地形贴地战术摄像机
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (ATacticalCameraPawn* Cam = Cast<ATacticalCameraPawn>(PC->GetPawn()))
		{
			Cam->bTerrainFollowingPivot = true;
			Cam->MinArmLength = 800.0f;
			Cam->MaxArmLength = MaxZoomArmLength;
			Cam->DefaultArmLength = InitialArmLength;
			Cam->DefaultYaw = InitialYawDeg;
			Cam->ClosePitch = -28.0f;
			Cam->FarPitch = -68.0f;
			Cam->EdgeScrollMargin = EdgeScrollMarginPx;
			Cam->PanSpeed = CameraPanSpeed;
			Cam->ZoomStep = 450.0f;
			Cam->SetMoveBounds(FBox(
				DeployCenter + FVector(-CameraBoundExtent, -CameraBoundExtent, -20000.0f),
				DeployCenter + FVector(CameraBoundExtent, CameraBoundExtent, 20000.0f)));
			Cam->SnapTo(DeployCenter + FVector(0.0f, -700.0f, 0.0f));
			Cam->SnapArmLength(InitialArmLength, InitialYawDeg);
		}
	}

	SetTacticalTimeMode(ELegionTimeMode::Normal);

	UE_LOG(LogERLegion, Log, TEXT("[Legion] Deployed %d ally legions vs %d enemy legions (+%d pending waves), animated=%d"),
		PlayerLegions.Num(), EnemyLegions.Num(), PendingWaves.Num(), (int32)bUseAnimatedAlly);
}

ALegionUnit* ALegionGameMode::SpawnLegion(ELegionTeam Team, ELegionClass Archetype, int32 SoldierCount,
	const FVector& Location, float YawDeg, int32 CampaignId, const FString& Callsign, int32 SyncLevel)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 兵团 Actor 本身保持无旋转, 仅把初始朝向传递给各个士兵自身转动
	ALegionUnit* L = GetWorld()->SpawnActor<ALegionUnit>(LegionClass, Location, FRotator::ZeroRotator, P);
	if (L)
	{
		L->SetTerrainGrid(&TerrainGrid);
		if (Team == ELegionTeam::Player)
		{
			L->InitLegion(Team, Archetype, SoldierCount,
				CachedAllyBody, nullptr, bUseAnimatedAlly ? nullptr : CachedAllyMat,
				bUseAnimatedAlly ? CachedAllySkel : nullptr,
				CachedAnimIdle, CachedAnimMove, CachedAnimAttacks, CachedAnimDeaths,
				CampaignId, Callsign, SyncLevel, YawDeg,
				FLinearColor(0.84f, 0.90f, 0.98f, 1.0f));
		}
		else
		{
			// 敌我统一采用 Manny 骨骼模型与动作, 通过 "Paint Tint" 染为敌方阵营色区分
			L->InitLegion(Team, Archetype, SoldierCount,
				CachedEnemyBody, nullptr, bUseAnimatedEnemy ? nullptr : CachedEnemyMat,
				bUseAnimatedEnemy ? CachedAllySkel : nullptr,
				CachedAnimIdle, CachedAnimMove, CachedAnimAttacks, CachedAnimDeaths,
				CampaignId, Callsign, SyncLevel, YawDeg,
				EnemySkeletalTint);
		}
	}
	return L;
}

bool ALegionGameMode::WorldToMinimapUV(const FVector& WorldPos, FVector2D& OutUV) const
{
	if (!TerrainGrid.IsBuilt()) { return false; }
	const float Span = TerrainGrid.GetWorldSpan();
	if (Span <= 1.0f) { return false; }
	const FVector Center = TerrainGrid.GetGridCenter();
	const float Half = Span * 0.5f;
	// 主相机 Yaw=90° 面朝 +Y: 屏幕左=+X, 右=-X, 上=+Y, 下=-Y
	OutUV.X = FMath::Clamp(1.0f - (WorldPos.X - (Center.X - Half)) / Span, 0.0f, 1.0f);
	OutUV.Y = FMath::Clamp(1.0f - (WorldPos.Y - (Center.Y - Half)) / Span, 0.0f, 1.0f);
	return true;
}

FVector ALegionGameMode::MinimapUVToWorld(const FVector2D& UV) const
{
	if (!TerrainGrid.IsBuilt()) { return DeployCenter; }
	const float Span = TerrainGrid.GetWorldSpan();
	const FVector Center = TerrainGrid.GetGridCenter();
	const float Half = Span * 0.5f;
	const float WX = (Center.X - Half) + (1.0f - FMath::Clamp(UV.X, 0.0f, 1.0f)) * Span;
	const float WY = (Center.Y - Half) + (1.0f - FMath::Clamp(UV.Y, 0.0f, 1.0f)) * Span;
	return FVector(WX, WY, DeployCenter.Z);
}

void ALegionGameMode::SpawnWaveLegion(const FPendingEnemyWave& Wave)
{
	const FString Label = FString::Printf(TEXT("REINF-%s-%02d"),
		*ERGalacticMeta::GetClassCode(Wave.Spec.Archetype), EnemyLegions.Num() + 1);
	if (ALegionUnit* L = SpawnLegion(ELegionTeam::Enemy, Wave.Spec.Archetype, Wave.Spec.SoldierCount,
		Wave.DropLocation, -90.0f, INDEX_NONE, Label, 1))
	{
		EnemyLegions.Add(L);
		if (ALegionUnit* Nearest = FindNearestEnemy(L))
		{
			L->CommandAttack(Nearest);
		}
		UE_LOG(LogERLegion, Log, TEXT("[Legion] Enemy reinforcement wave deployed: %s (%d soldiers)"),
			*Label, Wave.Spec.SoldierCount);
	}
}

void ALegionGameMode::AddTracerBeam(const FVector& Start, const FVector& End, const FLinearColor& Color, float Duration)
{
	FLegionTracerBeam B;
	B.Start = Start;
	B.End = End;
	B.Color = Color;
	B.Duration = FMath::Max(0.05f, Duration);
	B.Remaining = B.Duration;
	TracerBeams.Add(B);
}

void ALegionGameMode::SetTacticalTimeMode(ELegionTimeMode NewMode)
{
	TimeMode = NewMode;
	UWorld* World = GetWorld();
	if (!World) { return; }

	switch (TimeMode)
	{
	case ELegionTimeMode::Paused:
		UGameplayStatics::SetGamePaused(this, true);
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		break;
	case ELegionTimeMode::SlowMo:
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::SetGlobalTimeDilation(this, 0.20f);
		break;
	case ELegionTimeMode::Normal:
	default:
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		break;
	}
}

void ALegionGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;

	// 更新与清理弹道视觉轨迹
	for (int32 i = TracerBeams.Num() - 1; i >= 0; --i)
	{
		TracerBeams[i].Remaining -= DeltaSeconds;
		if (TracerBeams[i].Remaining <= 0.0f)
		{
			TracerBeams.RemoveAtSwap(i);
		}
	}

	// 检查延迟增援波次
	for (FPendingEnemyWave& PW : PendingWaves)
	{
		if (!PW.bSpawned && Elapsed >= PW.Spec.DelaySeconds)
		{
			PW.bSpawned = true;
			SpawnWaveLegion(PW);
		}
	}

	// 有限视野接敌 (0.3s 轮询): 敌方只对进入 EnemyAggroRange 的我方兵团发起进攻;
	// 我方仅在驻守且敌方贴近至 PlayerAutoDefenseRange 内时就近自卫
	AggroScanTimer -= DeltaSeconds;
	if (!bBattleOver && AggroScanTimer <= 0.0f)
	{
		AggroScanTimer = 0.3f;
		if (bEnemiesAutoCharge)
		{
			const float R2 = EnemyAggroRange * EnemyAggroRange;
			for (ALegionUnit* L : EnemyLegions)
			{
				if (!L || L->IsWiped() || L->GetAttackTarget()) { continue; }
				if (L->GetLegionState() == ELegionState::Moving) { continue; }
				ALegionUnit* Foe = FindNearestEnemy(L);
				if (Foe && FVector::DistSquared2D(L->GetActorLocation(), Foe->GetActorLocation()) <= R2)
				{
					L->CommandAttack(Foe);
				}
			}
		}

		const float PR2 = PlayerAutoDefenseRange * PlayerAutoDefenseRange;
		for (ALegionUnit* L : PlayerLegions)
		{
			if (!L || L->IsWiped() || L->GetAttackTarget()) { continue; }
			if (L->GetLegionState() != ELegionState::Holding) { continue; }
			ALegionUnit* Foe = FindNearestEnemy(L);
			if (Foe && FVector::DistSquared2D(L->GetActorLocation(), Foe->GetActorLocation()) <= PR2)
			{
				L->CommandAttack(Foe);
			}
		}
	}

	// 胜负判定 (0.5s 轮询)
	EndCheckTimer -= DeltaSeconds;
	if (EndCheckTimer <= 0.0f)
	{
		EndCheckTimer = 0.5f;
		CheckBattleEnd();
	}

	HandleAutoShot();
}

ALegionUnit* ALegionGameMode::FindNearestEnemy(const ALegionUnit* From) const
{
	if (!From) { return nullptr; }
	const TArray<TObjectPtr<ALegionUnit>>& Foes =
		(From->GetTeam() == ELegionTeam::Player) ? EnemyLegions : PlayerLegions;

	ALegionUnit* Best = nullptr;
	float BestD = MAX_FLT;
	for (ALegionUnit* F : Foes)
	{
		if (!F || F->IsWiped()) { continue; }
		const float D = FVector::DistSquared(From->GetActorLocation(), F->GetActorLocation());
		if (D < BestD) { BestD = D; Best = F; }
	}
	return Best;
}

int32 ALegionGameMode::CountPendingEnemyWaves() const
{
	int32 C = 0;
	for (const FPendingEnemyWave& PW : PendingWaves)
	{
		if (!PW.bSpawned) { ++C; }
	}
	return C;
}

void ALegionGameMode::CheckBattleEnd()
{
	if (bBattleOver) { return; }
	const bool bPlayerAlive = CountAliveLegions(ELegionTeam::Player) > 0;
	const bool bEnemyAlive = (CountAliveLegions(ELegionTeam::Enemy) > 0) || (CountPendingEnemyWaves() > 0);
	if (bPlayerAlive && bEnemyAlive) { return; }

	bBattleOver = true;
	bPlayerWon = bPlayerAlive;
	SetTacticalTimeMode(ELegionTimeMode::Normal);

	// 统计存活与伤亡并提交给银河战役子系统
	TMap<int32, int32> SurvivingById;
	int32 TotalCasualties = 0;
	for (ALegionUnit* L : PlayerLegions)
	{
		if (!L) { continue; }
		const int32 Cid = L->GetCampaignLegionId();
		const int32 Surv = L->GetAliveCount();
		if (Cid != INDEX_NONE)
		{
			SurvivingById.Add(Cid, Surv);
			if (const int32* InitCount = InitialPlayerCounts.Find(Cid))
			{
				TotalCasualties += FMath::Max(0, *InitCount - Surv);
			}
		}
	}
	const int32 EnemyEliminated = FMath::Max(0, InitialEnemySoldiersTotal - CountAliveSoldiers(ELegionTeam::Enemy));

	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		Campaign->CommitBattleOutcome(bPlayerWon, SurvivingById, TotalCasualties, EnemyEliminated);
	}

	UE_LOG(LogERLegion, Log, TEXT("[Legion] Battle over: %s (Player %d legions / %d soldiers)"),
		bPlayerWon ? TEXT("VICTORY") : TEXT("DEFEAT"),
		CountAliveLegions(ELegionTeam::Player), CountAliveSoldiers(ELegionTeam::Player));
	OnLegionBattleEnd.Broadcast(bPlayerWon);
}

int32 ALegionGameMode::CountAliveLegions(ELegionTeam Team) const
{
	const TArray<TObjectPtr<ALegionUnit>>& Ls = (Team == ELegionTeam::Player) ? PlayerLegions : EnemyLegions;
	int32 N = 0;
	for (ALegionUnit* L : Ls) { if (L && !L->IsWiped()) { ++N; } }
	return N;
}

int32 ALegionGameMode::CountAliveSoldiers(ELegionTeam Team) const
{
	const TArray<TObjectPtr<ALegionUnit>>& Ls = (Team == ELegionTeam::Player) ? PlayerLegions : EnemyLegions;
	int32 N = 0;
	for (ALegionUnit* L : Ls) { if (L) { N += L->GetAliveCount(); } }
	return N;
}

void ALegionGameMode::AllOutAssault(ELegionTeam Team)
{
	TArray<TObjectPtr<ALegionUnit>>& Ls = (Team == ELegionTeam::Player) ? PlayerLegions : EnemyLegions;
	for (ALegionUnit* L : Ls)
	{
		if (L && !L->IsWiped())
		{
			L->CommandAttack(FindNearestEnemy(L));
		}
	}
}

void ALegionGameMode::HandleAutoShot()
{
	if (!bAutoShotRequested) { return; }
	AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) { return; }

	if (!bOpeningShotDone && AutoShotTimer > 0.8f)
	{
		bOpeningShotDone = true;
		FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionStart"), true, true);
	}

	if (AutoShotStage == 0 && AutoShotTimer > 2.0f)
	{
		AllOutAssault(ELegionTeam::Player);
		AutoShotStage = 1;
	}
	else if (AutoShotStage == 1 && AutoShotTimer > 12.0f)
	{
		const FVector CamPos = DeployCenter + FVector(-1400.0f, -2600.0f, 3400.0f);
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(CamPos, FRotator(-36.0f, 68.0f, 0.0f));
		if (Cam)
		{
			PC->SetViewTargetWithBlend(Cam, 0.4f, VTBlend_Linear);
			AutoShotStage = 2;
		}
	}
	else if (AutoShotStage == 2 && AutoShotTimer > 14.5f)
	{
		const FString Path = FPaths::ScreenShotDir() / TEXT("LegionShot");
		FScreenshotRequest::RequestScreenshot(Path, true, true);
		AutoShotStage = 3;
		AutoShotTimer = 0.0f;
	}
	else if (AutoShotStage == 3 && AutoShotTimer > 1.0f)
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), 0.4f, VTBlend_Linear);
		AutoShotStage = 4;
	}
}
