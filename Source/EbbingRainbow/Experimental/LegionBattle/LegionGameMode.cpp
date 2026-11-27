// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Campaign/GalacticCampaignSubsystem.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/FPS/LegionCommanderCharacter.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Animation/AnimationAsset.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/ConstructorHelpers.h"

ALegionGameMode::ALegionGameMode()
{
	// 指挥官化身 = FP 模板射击栈蓝图 (缺失时退回 C++ 类, 无武器/无手臂的降级模式)
	static ConstructorHelpers::FClassFinder<APawn> CommanderBP(
		TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter.BP_ShooterCharacter_C"));
	if (CommanderBP.Succeeded()) { DefaultPawnClass = CommanderBP.Class; }
	else { DefaultPawnClass = ALegionCommanderCharacter::StaticClass(); }
	PlayerControllerClass = ALegionPlayerController::StaticClass();
	HUDClass = ALegionHUD::StaticClass();
	LegionClass = ALegionUnit::StaticClass();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AllyBodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/AdvancedRealisticGlass/Models/SM_Quinn.SM_Quinn")));
	AllySkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	EnemyBodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/AdvancedRealisticGlass/Models/SM_Quinn.SM_Quinn")));
	// 2026-12 真实弹道改造: 全员换装 FP 射击模板的手枪姿态/射击动画
	AllyAnimIdle = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS")));
	AllyAnimMove = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Fwd.MF_Pistol_Jog_Fwd")));
	// 交战姿态 = ADS 瞄准站姿: MM_Pistol_Fire 是 additive 叠加层, 单节点实例无法播放 (会变 bind pose 僵尸直立);
	// 开火反馈由弹道曳光/击杀承担, 士兵保持瞄准姿态即正确交战视觉
	AllyAnimAttacks = {
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS"))),
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
	bSkyShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-SkyShot"));
	bMapShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-MapShot"));
	bDeployShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-DeployShot"));
	bCloseupShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-LegionCloseup"));
}

void ALegionGameMode::BeginBattle()
{
	if (bBattleStarted) { return; }
	bBattleStarted = true;
	UE_LOG(LogERLegion, Log, TEXT("[Legion] Deployment link confirmed -> battle begins."));
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
	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	const bool bHasContract = Campaign && Campaign->HasActiveContract();
	const FString Label = bHasContract
		? ERGalacticMeta::GetEnemyBrigadeName(EnemyLegions.Num() + 1, true, Campaign->GetActiveContract().EnemyFaction)
		: ERGalacticMeta::GetEnemyBrigadeName(EnemyLegions.Num() + 1);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_GameModeTick);

	// 更新与清理弹道视觉轨迹 (接驳屏驻留期不会产生轨迹, 但清理本身无害)
	for (int32 i = TracerBeams.Num() - 1; i >= 0; --i)
	{
		TracerBeams[i].Remaining -= DeltaSeconds;
		if (TracerBeams[i].Remaining <= 0.0f)
		{
			TracerBeams.RemoveAtSwap(i);
		}
	}

	// 接驳加载屏未确认前: 战斗时钟/增援波次/索敌接敌/胜负判定全部冻结,
	// 世界照常流转 (草皮流送继续构筑), 双方兵团原地待命
	if (bBattleStarted)
	{
		Elapsed += DeltaSeconds;

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

	// 真实弹道推进: 实体子弹逐帧积分+命中扫描
	SimulateBullets(DeltaSeconds);

	// 胜负判定 (0.5s 轮询)
	EndCheckTimer -= DeltaSeconds;
	if (EndCheckTimer <= 0.0f)
	{
		EndCheckTimer = 0.5f;
		CheckBattleEnd();
	}
	} // end if (bBattleStarted)

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

	if (BattleContractId != INDEX_NONE)
	{
		if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
		{
			Campaign->CommitBattleOutcome(BattleContractId, bPlayerWon, SurvivingById, TotalCasualties, EnemyEliminated);
		}
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

void ALegionGameMode::SpawnBullet(const FVector& MuzzlePos, const FVector& Vel, ALegionUnit* Source)
{
	if (!Source || !bBattleStarted || bBattleOver) { return; }
	FLegionBullet B;
	B.Pos = MuzzlePos;
	B.PrevPos = MuzzlePos;
	B.Vel = Vel;
	B.Life = 1.6f; // ~8300cm 极限行程, 覆盖 2600 射程绰绰有余
	B.Team = Source->GetTeam();
	B.Source = Source;
	Bullets.Add(B);
}

void ALegionGameMode::SimulateBullets(float DeltaSeconds)
{
	if (Bullets.Num() == 0) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }
	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_Bullets);

	// 单发命中伤害: 士兵统一 40HP, 两枪倒
	constexpr float BulletDamage = 25.0f;
	// 士兵胶囊近似: 半径 40cm, 轴段覆盖脚底 +15..+165
	constexpr float CapsuleRadius = 40.0f;
	constexpr float AxisLo = 15.0f;
	constexpr float AxisHi = 165.0f;
	// 沿飞行线段步进采样间距 (远小于胶囊半径, 保证不穿透漏检)
	constexpr float StepCm = 15.0f;

	for (int32 i = Bullets.Num() - 1; i >= 0; --i)
	{
		FLegionBullet& B = Bullets[i];
		B.PrevPos = B.Pos;
		B.Pos += B.Vel * DeltaSeconds;
		B.Life -= DeltaSeconds;

		bool bKill = (B.Life <= 0.0f);
		FVector SegEnd = B.Pos;

		// 1. 地形/岩石/树干等静态物遮挡: 线段trace, 命中则截断飞行线段 (掩体自然生效)
		float TerrainT = 1.0f;
		if (!bKill)
		{
			FHitResult TerrainHit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(LegionBulletTerrain), false);
			if (World->LineTraceSingleByChannel(TerrainHit, B.PrevPos, B.Pos, ECC_WorldStatic, Params))
			{
				TerrainT = FMath::Clamp(TerrainHit.Time, 0.0f, 1.0f);
				SegEnd = FMath::Lerp(B.PrevPos, B.Pos, TerrainT);
				bKill = true; // 无论是否先打中士兵, 子弹最多飞到遮挡点
			}
		}

		// 2. 敌兵胶囊扫描 (仅敌方阵营: 弹道不允许误伤友军, 穿透己方队列是设计选择)
		if (!bKill || TerrainT < 1.0f)
		{
			const TArray<TObjectPtr<ALegionUnit>>& Foes =
				(B.Team == ELegionTeam::Player) ? EnemyLegions : PlayerLegions;

			const float SegLen = FVector::Dist(B.PrevPos, SegEnd);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(SegLen / StepCm));

			ALegionUnit* BestUnit = nullptr;
			int32 BestSoldier = INDEX_NONE;
			float BestT = 2.0f;
			FVector BestHitPos = FVector::ZeroVector;

			for (ALegionUnit* Foe : Foes)
			{
				if (!Foe || Foe->IsWiped()) { continue; }

				// 粗剔除: 线段中点距团中心超过编队半径+射程余量则整团跳过
				const FVector SegMid = (B.PrevPos + SegEnd) * 0.5f;
				const float RoughR = Foe->GetFormationHalfExtent().GetMax() + SegLen + 200.0f;
				if (FVector::DistSquared2D(SegMid, Foe->GetActorLocation()) > RoughR * RoughR) { continue; }

				const int32 Count = Foe->GetSoldierCount();
				for (int32 s = 0; s < Count; ++s)
				{
					FVector Feet;
					if (!Foe->GetAliveSoldierFeet(s, Feet)) { continue; }

					for (int32 k = 0; k <= Steps; ++k)
					{
						const float T = (float)k / (float)Steps;
						if (T >= BestT) { break; }
						const FVector P = FMath::Lerp(B.PrevPos, SegEnd, T);

						// 垂直胶囊点测: 水平偏移 + 轴向夹取
						const float Dz = (P.Z < Feet.Z + AxisLo) ? (Feet.Z + AxisLo - P.Z)
							: (P.Z > Feet.Z + AxisHi) ? (P.Z - Feet.Z - AxisHi) : 0.0f;
						const float Dxy = FVector::Dist2D(P, Feet);
						if (Dxy * Dxy + Dz * Dz <= CapsuleRadius * CapsuleRadius)
						{
							BestT = T;
							BestUnit = Foe;
							BestSoldier = s;
							BestHitPos = P;
							break;
						}
					}
				}
			}

			if (BestUnit && BestSoldier != INDEX_NONE)
			{
				const FVector HitDir = B.Vel.GetSafeNormal();
				BestUnit->ApplyBulletHitToSoldier(BestSoldier, BulletDamage, HitDir);
				bKill = true;
			}
		}

		if (bKill)
		{
			Bullets.RemoveAtSwap(i);
		}
	}
}
