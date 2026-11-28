// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Campaign/GalacticCampaignSubsystem.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/FPS/LegionCommanderCharacter.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"
#include "Legion/LegionPerf.h"

#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
	// 2026-12 真实弹道改造: 全员换装 FP 射击模板的手枪姿态/射击动画 + 2D 混合空间 (放松态垂枪漫步 / 警戒交战持枪八向走位)
	AllyAnimIdle = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS")));
	AllyAnimRelaxedIdle = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")));
	AllyAnimMove = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Fwd.MF_Pistol_Jog_Fwd")));
	AllyRelaxedBlendSpace = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run")));
	AllyCombatBlendSpace = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/BS_Pistol_Locomotion.BS_Pistol_Locomotion")));
	// 交战姿态 = ADS 瞄准站姿: MM_Pistol_Fire 是 additive 叠加层, 单节点实例无法播放 (会变 bind pose 僵尸直立);
	// 开火反馈由弹道曳光/击杀承担, 士兵保持瞄准姿态即正确交战视觉
	AllyAnimAttacks = {
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS"))),
	};
	AllyAnimDeaths = {
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_02.MM_Death_Front_02"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_03.MM_Death_Front_03"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Left_01.MM_Death_Left_01"))),
		TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Right_01.MM_Death_Right_01"))),
	};

	// Lvl_Legion (1008m x 1008m 首颗行星) 经离线扫描验证的 8 个空间分离候选战区 (>=160m 间距)
	auto AddZone = [this](const TCHAR* Name, float X, float Y, float Yaw, float Walk, float Span, float DeltaZ)
	{
		FLegionSpawnZone Z;
		Z.DisplayName = Name;
		Z.CenterXY = FVector2D(X, Y);
		Z.BattleAxisYawDeg = Yaw;
		Z.HalfExtentCm = 16000.0f;
		Z.WalkableRatio = Walk;
		Z.ReliefSpanCm = Span;
		Z.PlayerHighGroundDeltaCm = DeltaZ;
		CandidateZones.Add(MoveTemp(Z));
	};
	AddZone(TEXT("东北缓谷"),    14400.0f,  26400.0f, 225.0f, 0.896f,  9752.0f, -1299.0f);
	AddZone(TEXT("西翼盆地"),   -26400.0f,  -9600.0f,   0.0f, 0.878f,  9979.0f, -1145.0f);
	AddZone(TEXT("北境台地"),    -2400.0f,  26400.0f,  90.0f, 0.852f,  8352.0f,   239.0f);
	AddZone(TEXT("东南廊道"),    26400.0f, -14400.0f,  45.0f, 0.819f, 10881.0f,   195.0f);
	AddZone(TEXT("中央鞍部"),    -9600.0f,  -2400.0f,   0.0f, 0.783f, 10735.0f,    34.0f);
	AddZone(TEXT("东麓制高点"),  26400.0f,  14400.0f,  45.0f, 0.827f, 10246.0f,  2283.0f);
	AddZone(TEXT("西北仰攻坡"), -26400.0f,  19200.0f,  45.0f, 0.813f,  9920.0f, -3041.0f);
	AddZone(TEXT("西南深谷"),   -26400.0f, -26400.0f,  90.0f, 0.735f, 14098.0f,   411.0f);
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
	bAIShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-LegionAIShot"));
	bResultShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-ResultShot"));
	bPauseShotRequested = FParse::Param(FCommandLine::Get(), TEXT("-PauseShot"));
	bForceSkeletalSoldiers = FParse::Param(FCommandLine::Get(), TEXT("-LegionForceSkeletal"));
	bPerfTestRequested = FParse::Param(FCommandLine::Get(), TEXT("-LegionPerf"));
}

void ALegionGameMode::GetBattleBasis(FVector& OutForward, FVector& OutRight) const
{
	const float Rad = FMath::DegreesToRadians(BattleAxisYawDeg);
	OutForward = FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f);
	// 保持 Yaw=90° 时 Forward=(0,1,0), Right=(1,0,0), 与原始南北布阵完全一致
	OutRight = FVector(OutForward.Y, -OutForward.X, 0.0f);
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
	const float EnemyYaw = FRotator::NormalizeAxis(BattleAxisYawDeg + 180.0f);
	if (ALegionUnit* L = SpawnLegion(ELegionTeam::Enemy, Wave.Spec.Archetype, Wave.Spec.SoldierCount,
		Wave.DropLocation, EnemyYaw, INDEX_NONE, Label, 1))
	{
		EnemyLegions.Add(L);
		if (ALegionUnit* Nearest = FindNearestEnemy(L))
		{
			L->CommandAutonomousAssault(Nearest);
		}
		UE_LOG(LogERLegion, Log, TEXT("[Legion] Enemy reinforcement wave deployed: %s (%d soldiers)"),
			*Label, Wave.Spec.SoldierCount);
	}
}

void ALegionGameMode::AssignInitialWeakAIDirectives()
{
	FRandomStream Rng(ActiveMatchSeed ^ 0x5A17C3);
	FVector BattleForward, BattleRight;
	GetBattleBasis(BattleForward, BattleRight);
	const float EnemyYaw = FRotator::NormalizeAxis(BattleAxisYawDeg + 180.0f);

	for (int32 i = 0; i < EnemyLegions.Num(); ++i)
	{
		ALegionUnit* E = EnemyLegions[i].Get();
		if (!E || E->IsWiped() || E->GetDirective() == ELegionDirective::PlayerOrder) { continue; }

		const FVector Home = E->GetActorLocation();
		const ELegionClass Cls = E->GetArchetype();

		// 按兵种与编队序号分配差异化弱 AI: 高地射手/重盾戒备固守, 长戟防区巡逻, 突击刃兵探索渗透
		if (Cls == ELegionClass::Marksman || (Cls == ELegionClass::Shieldbearer && i % 2 == 0))
		{
			// 在附近 18m 内找局部高地制高点作为戒备锚点
			FVector BestGuard = Home;
			float BestZ = Home.Z;
			if (TerrainGrid.IsBuilt())
			{
				for (int32 k = 0; k < 8; ++k)
				{
					const float Ang = ((float)k / 8.0f) * 2.0f * PI;
					FVector Cand = Home + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.0f) * 1400.0f;
					Cand = TerrainGrid.ClampToWalkable(Cand);
					float Z = 0.0f, Slope = 0.0f;
					if (TerrainGrid.SampleHeightAndSlope(Cand, Z, Slope) && Z > BestZ + 60.0f)
					{
						BestZ = Z;
						BestGuard = Cand;
					}
				}
			}
			E->CommandGuard(BestGuard, EnemyYaw);
		}
		else if (Cls == ELegionClass::Vanguard)
		{
			E->CommandRecon();
		}
		else
		{
			// 2~3 点往返巡逻航线 (沿战线左右或浅纵深来回巡弋)
			const float Lateral = 1750.0f * ((i % 2 == 0) ? 1.0f : -1.0f);
			const float Depth = Rng.FRandRange(900.0f, 1750.0f);
			TArray<FVector> Pts;
			Pts.Add(Home);
			Pts.Add(Home + BattleRight * Lateral - BattleForward * (Depth * 0.45f));
			Pts.Add(Home + BattleRight * (Lateral * 0.35f) - BattleForward * Depth);
			E->CommandPatrol(Pts);
		}
	}

	UE_LOG(LogERLegion, Log, TEXT("[LegionAI] Assigned initial weak-AI directives to %d enemy legions (seed=%d)"),
		EnemyLegions.Num(), ActiveMatchSeed);
}

void ALegionGameMode::BroadcastTeamContact(ELegionTeam ReportingTeam, ALegionUnit* SpottedEnemy, const FVector& ContactPos, bool bFromDirectFire)
{
	if (!SpottedEnemy || SpottedEnemy->IsWiped() || !bBattleStarted || bBattleOver) { return; }

	const TArray<TObjectPtr<ALegionUnit>>& Friends =
		(ReportingTeam == ELegionTeam::Player) ? PlayerLegions : EnemyLegions;

	constexpr float RadioRadiusSq = 6800.0f * 6800.0f;
	for (ALegionUnit* L : Friends)
	{
		if (!L || L->IsWiped()) { continue; }
		if (FVector::DistSquared2D(L->GetActorLocation(), ContactPos) <= RadioRadiusSq
			|| FVector::DistSquared2D(L->GetActorLocation(), SpottedEnemy->GetActorLocation()) <= RadioRadiusSq)
		{
			L->NotifyEnemyContact(SpottedEnemy, ContactPos, bFromDirectFire);
		}
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
	LegionPerf::FScope PerfScope(LegionPerf::Get().GameModeTickMs);
	LegionPerf::Get().Frames += 1;

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

	if (!bInitialWeakAIAssigned && Elapsed >= InitialWeakAIDelay)
	{
		bInitialWeakAIAssigned = true;
		AssignInitialWeakAIDirectives();
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

	// 有限视野接敌 (0.3s 轮询): 兼容旧版敌方自动突击 + 推进所有存活旅弱 AI 决策树
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
				if (L->GetDirective() == ELegionDirective::PlayerOrder) { continue; }
				ALegionUnit* Foe = FindNearestEnemy(L);
				if (Foe && FVector::DistSquared2D(L->GetActorLocation(), Foe->GetActorLocation()) <= R2)
				{
					L->NotifyEnemyContact(Foe, Foe->GetActorLocation(), false);
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
				L->NotifyEnemyContact(Foe, Foe->GetActorLocation(), false);
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

	FinishBattle(bPlayerAlive);
}

void ALegionGameMode::CommitOutcomeToCampaign(bool bVictory)
{
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
			Campaign->CommitBattleOutcome(BattleContractId, bVictory, SurvivingById, TotalCasualties, EnemyEliminated);
		}
	}
}

void ALegionGameMode::AbortBattleAsDefeat()
{
	if (bBattleOver) { return; }

	bBattleOver = true;
	bPlayerWon = false;

	CommitOutcomeToCampaign(false);

	UE_LOG(LogERLegion, Log, TEXT("[Legion] Battle aborted by player -> committed as DEFEAT, returning to Lvl_Hall."));

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")));
	}
}

void ALegionGameMode::FinishBattle(bool bVictory)
{
	if (bBattleOver) { return; }

	bBattleOver = true;
	bPlayerWon = bVictory;

	// 结算阶段立即暂停游戏世界, 冻结所有兵团、弹道与角色行为
	SetTacticalTimeMode(ELegionTimeMode::Paused);

	CommitOutcomeToCampaign(bPlayerWon);

	// 立刻切断第一人称与战场操控、唤出鼠标并切换为纯 UI 模式
	if (UWorld* World = GetWorld())
	{
		if (ALegionPlayerController* LPC = Cast<ALegionPlayerController>(World->GetFirstPlayerController()))
		{
			LPC->EnterBattleOverMode();
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
	LegionPerf::FScope PerfScope(LegionPerf::Get().BulletsMs);

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
			const FVector SegMid = (B.PrevPos + SegEnd) * 0.5f;

			BulletHitCandidates.Reset();
			for (ALegionUnit* Foe : Foes)
			{
				if (!Foe || Foe->IsWiped()) { continue; }

				// 粗剔除: 线段中点距团中心超过编队半径+射程余量则整团跳过
				const float RoughR = Foe->GetFormationHalfExtent().GetMax() + SegLen + 200.0f;
				if (FVector::DistSquared2D(SegMid, Foe->GetActorLocation()) > RoughR * RoughR) { continue; }

				const int32 Count = Foe->GetSoldierCount();
				for (int32 s = 0; s < Count; ++s)
				{
					FVector Feet;
					if (!Foe->GetAliveSoldierFeet(s, Feet)) { continue; }
					FLegionBulletHitCandidate& Candidate = BulletHitCandidates.AddDefaulted_GetRef();
					Candidate.Unit = Foe;
					Candidate.SoldierIndex = s;
					Candidate.Feet = Feet;
				}
			}

			// 时间优先扫描: 第一个命中的采样点就是子弹终点, 无需把每个士兵都扫完整段
			ALegionUnit* HitUnit = nullptr;
			int32 HitSoldier = INDEX_NONE;
			bool bFoundHit = false;
			const bool bHasHitCandidates = (BulletHitCandidates.Num() > 0);
			for (int32 k = 0; bHasHitCandidates && k <= Steps && !bFoundHit; ++k)
			{
				const float T = (float)k / (float)Steps;
				const FVector P = FMath::Lerp(B.PrevPos, SegEnd, T);
				for (const FLegionBulletHitCandidate& Candidate : BulletHitCandidates)
				{
					ALegionUnit* CandidateUnit = Candidate.Unit.Get();
					if (!CandidateUnit) { continue; }

					// 垂直胶囊点测: 水平偏移 + 轴向夹取
					const FVector& Feet = Candidate.Feet;
					const float Dz = (P.Z < Feet.Z + AxisLo) ? (Feet.Z + AxisLo - P.Z)
						: (P.Z > Feet.Z + AxisHi) ? (P.Z - Feet.Z - AxisHi) : 0.0f;
					const float Dx = P.X - Feet.X;
					const float Dy = P.Y - Feet.Y;
					if (Dx * Dx + Dy * Dy + Dz * Dz <= CapsuleRadius * CapsuleRadius)
					{
						HitUnit = CandidateUnit;
						HitSoldier = Candidate.SoldierIndex;
						bFoundHit = true;
						break;
					}
				}
			}

			if (HitUnit && HitSoldier != INDEX_NONE)
			{
				const FVector HitDir = B.Vel.GetSafeNormal();
				HitUnit->ApplyBulletHitToSoldier(HitSoldier, BulletDamage, HitDir);
				bKill = true;
			}
		}

		if (bKill)
		{
			Bullets.RemoveAtSwap(i);
		}
	}
}

bool ALegionGameMode::ScanProjectileSoldierHit(const FVector& Start, const FVector& End, ELegionTeam ShooterTeam,
	ALegionUnit*& OutUnit, int32& OutSoldierIndex, FVector& OutHitPos)
{
	OutUnit = nullptr;
	OutSoldierIndex = INDEX_NONE;
	UWorld* World = GetWorld();
	if (!World) { return false; }

	// 与士兵子弹同一套胶囊近似与步进参数
	constexpr float CapsuleRadius = 40.0f;
	constexpr float AxisLo = 15.0f;
	constexpr float AxisHi = 165.0f;
	constexpr float StepCm = 15.0f;

	// 地形遮挡截断: 士兵躲在坡后不应被穿透命中 (投射物物理体随后会自然撞上地形)
	FVector SegEnd = End;
	FHitResult TerrainHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LegionBulletTerrain), false);
	if (World->LineTraceSingleByChannel(TerrainHit, Start, End, ECC_WorldStatic, Params))
	{
		SegEnd = FMath::Lerp(Start, End, FMath::Clamp(TerrainHit.Time, 0.0f, 1.0f));
	}

	const TArray<TObjectPtr<ALegionUnit>>& Foes =
		(ShooterTeam == ELegionTeam::Player) ? EnemyLegions : PlayerLegions;

	const float SegLen = FVector::Dist(Start, SegEnd);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(SegLen / StepCm));
	const FVector SegMid = (Start + SegEnd) * 0.5f;

	BulletHitCandidates.Reset();
	for (ALegionUnit* Foe : Foes)
	{
		if (!Foe || Foe->IsWiped()) { continue; }
		const float RoughR = Foe->GetFormationHalfExtent().GetMax() + SegLen + 200.0f;
		if (FVector::DistSquared2D(SegMid, Foe->GetActorLocation()) > RoughR * RoughR) { continue; }

		const int32 Count = Foe->GetSoldierCount();
		for (int32 s = 0; s < Count; ++s)
		{
			FVector Feet;
			if (!Foe->GetAliveSoldierFeet(s, Feet)) { continue; }
			FLegionBulletHitCandidate& Candidate = BulletHitCandidates.AddDefaulted_GetRef();
			Candidate.Unit = Foe;
			Candidate.SoldierIndex = s;
			Candidate.Feet = Feet;
		}
	}

	for (int32 k = 0; k <= Steps; ++k)
	{
		const float T = (float)k / (float)Steps;
		const FVector P = FMath::Lerp(Start, SegEnd, T);
		for (const FLegionBulletHitCandidate& Candidate : BulletHitCandidates)
		{
			if (!Candidate.Unit.IsValid()) { continue; }
			const FVector& Feet = Candidate.Feet;
			const float Dz = (P.Z < Feet.Z + AxisLo) ? (Feet.Z + AxisLo - P.Z)
				: (P.Z > Feet.Z + AxisHi) ? (P.Z - Feet.Z - AxisHi) : 0.0f;
			const float Dx = P.X - Feet.X;
			const float Dy = P.Y - Feet.Y;
			if (Dx * Dx + Dy * Dy + Dz * Dz <= CapsuleRadius * CapsuleRadius)
			{
				OutUnit = Candidate.Unit.Get();
				OutSoldierIndex = Candidate.SoldierIndex;
				OutHitPos = P;
				return true;
			}
		}
	}
	return false;
}

int32 ALegionGameMode::ApplyExplosionToEnemySoldiers(const FVector& Center, float Radius, float Damage, ELegionTeam ShooterTeam)
{
	// 爆炸对敌兵的数学结算: 取代物理 overlap 命中代理胶囊, 与单发扫描共用位置源
	const TArray<TObjectPtr<ALegionUnit>>& Foes =
		(ShooterTeam == ELegionTeam::Player) ? EnemyLegions : PlayerLegions;
	const float RadiusSq = Radius * Radius;
	int32 HitCount = 0;
	for (ALegionUnit* Foe : Foes)
	{
		if (!Foe || Foe->IsWiped()) { continue; }
		const float RoughR = Foe->GetFormationHalfExtent().GetMax() + Radius;
		if (FVector::DistSquared2D(Center, Foe->GetActorLocation()) > RoughR * RoughR) { continue; }

		const int32 Count = Foe->GetSoldierCount();
		for (int32 s = 0; s < Count; ++s)
		{
			FVector Feet;
			if (!Foe->GetAliveSoldierFeet(s, Feet)) { continue; }
			// 胶囊躯干中心近似 (脚底 +90cm)
			const FVector Chest = Feet + FVector(0.0f, 0.0f, 90.0f);
			if (FVector::DistSquared(Center, Chest) <= RadiusSq)
			{
				const FVector Dir = (Chest - Center).GetSafeNormal();
				Foe->ApplyBulletHitToSoldier(s, Damage, Dir);
				++HitCount;
			}
		}
	}
	return HitCount;
}
