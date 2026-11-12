// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Core/BattleGameMode.h"
#include "Core/ERDebug.h"
#include "Core/ERLog.h"
#include "Battle/Arena/BattleArena.h"
#include "WFC/WFC3DStyleTable.h"
#include "Battle/Player/BattlePlayerController.h"
#include "Battle/Player/TacticalCameraPawn.h"
#include "Battle/Units/BattleUnit.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Skills/BattleRandomStream.h"
#include "Battle/UI/BattleHUD.h"
#include "Hall/HallCameraPawn.h"
#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"
#include "Run/RunSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"

/** 调试：>0 时战斗开始 4 秒后自动判胜，便于验证"战斗→航线图"回环。 */
static TAutoConsoleVariable<int32> CVarAutoWin(
	TEXT("ER.AutoWin"), 0, TEXT("调试：>0 时战斗开始 4 秒后自动判定胜利。"), ECVF_Cheat);

/** 调试：>0 时竞技场就绪后自动拉远镜头截一张图，便于离线检查生成结果。 */
static TAutoConsoleVariable<int32> CVarAutoShot(
	TEXT("ER.AutoShot"), 0, TEXT("调试：>0 时竞技场就绪后自动截图到 Saved/Screenshots。"), ECVF_Cheat);

ABattleGameMode::ABattleGameMode()
{
	DefaultPawnClass = ATacticalCameraPawn::StaticClass();
	PlayerControllerClass = ABattlePlayerController::StaticClass();
	HUDClass = ABattleHUD::StaticClass();
	ArenaClass = ABattleArena::StaticClass();
	UnitClass = ABattleUnit::StaticClass();

	// 默认名单 (名称, 完整度, AP, 先攻值, 闪避率, 力学抗性, 热学抗性, 电学抗性, 化学抗性)
	// 技能 Id 由 UBattleSkillLibrary 目录解析 (技能系统 2.0)
	PlayerRoster = GetDefaultPlayerRoster();
	EnemyRoster = {
		FBattleUnitStats(TEXT("Goblin"),  26.0f, 4, 13, 0.15f, -0.10f, -0.15f,  0.0f,   0.10f), // 哥布林: 灵敏脆弱
		FBattleUnitStats(TEXT("Orc"),     65.0f, 3, 6,  0.02f,  0.25f, -0.20f, -0.10f,  0.0f),  // 兽人: 重肉盾, 行动慢, 弱热能
		FBattleUnitStats(TEXT("Archer"),  28.0f, 4, 12, 0.10f,  0.0f,   0.0f,   0.0f,   0.0f),  // 射手: 远程游击
		FBattleUnitStats(TEXT("Shaman"),  35.0f, 3, 9,  0.05f, -0.10f,  0.20f,  0.20f,  0.20f), // 萨满: 元素守卫
	};
	EnemyRoster[0].SkillIds = { TEXT("Slash"), TEXT("Shoot") };
	EnemyRoster[1].SkillIds = { TEXT("Slash"), TEXT("FieldBarrier") };
	EnemyRoster[2].SkillIds = { TEXT("Shoot"), TEXT("AcidBomb") };
	EnemyRoster[3].SkillIds = { TEXT("Shock"), TEXT("ChainLightning") };
}

const TArray<FBattleUnitStats>& ABattleGameMode::GetDefaultPlayerRoster()
{
	static const TArray<FBattleUnitStats> Roster = []()
	{
		TArray<FBattleUnitStats> Units = {
			FBattleUnitStats(TEXT("力学"), 60.0f, 3, 8,  0.05f,  0.30f,  0.0f,  -0.20f,  0.10f), // 重甲高力抗, 弱电导
			FBattleUnitStats(TEXT("热学"), 38.0f, 4, 14, 0.20f,  0.05f, -0.10f,  0.05f,  0.0f),  // 高先攻大跑动距离, 高闪避
			FBattleUnitStats(TEXT("电学"), 32.0f, 3, 10, 0.05f, -0.15f,  0.35f,  0.25f,  0.10f), // 弱力学, 强元素抗性
			FBattleUnitStats(TEXT("化学"), 45.0f, 3, 9,  0.05f,  0.15f,  0.10f,  0.10f,  0.25f), // 均衡, 高化学抗性
		};

		// 技能构筑 (学派定位): 力学近战单体 / 热学周身新星 / 电学连锁 / 化学投掷 AoE
		Units[0].SkillIds = { TEXT("Slash"),    TEXT("ShieldBash"),   TEXT("PowerStrike"),   TEXT("FieldBarrier") };
		Units[1].SkillIds = { TEXT("Overheat"), TEXT("ScorchPulse"),  TEXT("NovaBlast") };
		Units[2].SkillIds = { TEXT("Shock"),    TEXT("ChainLightning"),TEXT("ArcStorm"),     TEXT("Overclock") };
		Units[3].SkillIds = { TEXT("AcidSpray"),TEXT("AcidBomb"),     TEXT("VenomCloud") };
		return Units;
	}();
	return Roster;
}

void ABattleGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// 防御性保护：如果当前地图是 Hall/大厅 地图，自动重定向到大厅专属 Pawn 与控制器，并绝对不生成战斗竞技场
	const FString CurrentMap = GetWorld() ? GetWorld()->GetMapName() : MapName;
	if (MapName.Contains(TEXT("Hall")) || CurrentMap.Contains(TEXT("Hall")))
	{
		UE_LOG(LogERBattle, Warning, TEXT("[BattleGameMode] BattleGameMode loaded in Hall map '%s'! Redirecting classes to HallCameraPawn."), *CurrentMap);
		DefaultPawnClass = AHallCameraPawn::StaticClass();
		PlayerControllerClass = AHallPlayerController::StaticClass();
		HUDClass = AHallHUD::StaticClass();
		return;
	}

	// 航线图战斗：从 URL 读取节点，决定本场的地形种子与队伍状态
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		// 场景生成器显式覆盖 (调试用, 如 ?gen=2): 平时由关卡文件声明, 不走 URL
		const FString GenOption = UGameplayStatics::ParseOption(Options, TEXT("gen"));
		if (!GenOption.IsEmpty())
		{
			PendingGeneratorChoice = FCString::Atoi(*GenOption);
			UE_LOG(LogERBattle, Log, TEXT("[Battle] URL 显式覆盖生成器: gen=%d"), PendingGeneratorChoice);
		}

		if (URunSubsystem* Run = GameInstance->GetSubsystem<URunSubsystem>())
		{
			const FString NodeOption = UGameplayStatics::ParseOption(Options, TEXT("node"));
			if (!NodeOption.IsEmpty() && Run->HasActiveRun())
			{
				const int32 NodeId = FCString::Atoi(*NodeOption);
				if (const FRouteNode* Node = Run->FindNode(NodeId))
				{
					PendingNodeId = Node->Id;
					PendingEncounterSeed = Node->EncounterSeed;
					// 场景种类由关卡文件声明 (一场景一关卡): BattleNature 节点开 Lvl_NatureArena,
					// 地图场景档案自动选自然岛生成器, 这里不再按节点推导生成器。
					bRunBattle = true;
					UE_LOG(LogERBattle, Log, TEXT("[Battle] 来自航线图节点 %d (第 %d 层, 遭遇种子 %d, 地图 %s)"),
						Node->Id, Node->Layer + 1, Node->EncounterSeed, *CurrentMap);
				}
			}
		}
	}

	// 在玩家 Pawn 出生之前创建竞技场 Actor (生成逻辑在其 BeginPlay 中执行)
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Arena = World->SpawnActor<ABattleArena>(ArenaClass ? ArenaClass.Get() : ABattleArena::StaticClass(),
			FTransform(ArenaOrigin), Params);
		if (Arena)
		{
			// 在 BeginPlay 生成之前写入本场战斗的三参数（后续可改为从 RunState 读取）
			Arena->Style = ArenaStyle;
			Arena->GeneratorChoice = PendingGeneratorChoice;
			Arena->GridCells = ArenaScale;
			Arena->StyleTable = WFCStyleTable;
			if (bRunBattle)
			{
				// 用路线节点的遭遇种子，保证同一局同一个节点的地形一致
				Arena->BaseSeed = PendingEncounterSeed;
			}
			Arena->OnArenaGenerated.AddDynamic(this, &ABattleGameMode::HandleArenaGenerated);
		}
	}
}

void ABattleGameMode::StartPlay()
{
	Super::StartPlay();

	const FString CurrentMap = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (CurrentMap.Contains(TEXT("Hall")))
	{
		return;
	}

	if (Arena && Arena->IsArenaReady() && !bSetupComplete)
	{
		HandleArenaGenerated(true);
	}
}

void ABattleGameMode::HandleArenaGenerated(bool bSuccess)
{
	if (!bSuccess)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red,
				TEXT("Arena generation FAILED. Check LogERBattle (is a NavMeshBoundsVolume present?)"));
		}
		return;
	}

	URunSubsystem* Run = GetRunSubsystem();
	if (bRunBattle && Run && Run->HasActiveRun())
	{
		// 只生成还活着的队员，并把上一场战斗剩下的完整度带进来
		const TArray<FRunUnitState>& Party = Run->GetParty();
		TArray<FBattleUnitStats> AliveRoster;
		PlayerPartyIndices.Reset();
		for (int32 Index = 0; Index < Party.Num(); ++Index)
		{
			if (Party[Index].Integrity > 0.0f)
			{
				AliveRoster.Add(Party[Index].Stats);
				PlayerPartyIndices.Add(Index);
			}
		}

		SpawnTeam(AliveRoster, Arena->GetPlayerSpawnPoints(), EBattleTeam::Player, PlayerUnits);
		for (int32 Index = 0; Index < PlayerUnits.Num() && Index < PlayerPartyIndices.Num(); ++Index)
		{
			PlayerUnits[Index]->ApplyPersistentIntegrity(Party[PlayerPartyIndices[Index]].Integrity);
		}
	}
	else
	{
		SpawnTeam(PlayerRoster, Arena->GetPlayerSpawnPoints(), EBattleTeam::Player, PlayerUnits);
	}

	SpawnTeam(EnemyRoster, Arena->GetEnemySpawnPoints(), EBattleTeam::Enemy, EnemyUnits);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
			FString::Printf(TEXT("Arena ready (seed %d): %d player units, %d enemy units"),
				Arena->GetUsedSeed(), PlayerUnits.Num(), EnemyUnits.Num()));
	}

	bSetupComplete = true;

	// 调试: ER.AutoShot > 0 (或命令行 -AutoShot) 时把镜头拉到全场中心并截一张图
	if (CVarAutoShot.GetValueOnGameThread() > 0
		|| ER::Debug::IsAutoShot())
	{
		const FVector Center = Arena->GetActorLocation() + FVector(3800.0f, 3800.0f, 0.0f);
		GetWorldTimerManager().SetTimer(AutoShotTimerHandle, FTimerDelegate::CreateWeakLambda(
			this, [this, Center]()
			{
				// 临时俯瞰相机: 比战术镜头看得全, 便于离线检查整场构图
				// -AutoShotTop 用正俯机位评估平面构图/破碎外壳剪影
				const bool bTop = ER::Debug::IsAutoShotTop();
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				ACameraActor* ShotCam = GetWorld()->SpawnActor<ACameraActor>(
					ACameraActor::StaticClass(),
					Center + FVector(0.0f, 0.0f, bTop ? 10000.0f : 5600.0f),
					bTop ? FRotator(-90.0f, 0.0f, 0.0f)
						 : FRotator(-58.0f, -32.0f, 0.0f),
					Params);
				if (ShotCam)
				{
					if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
					{
						PC->SetViewTargetWithBlend(ShotCam, 0.0f);
					}
					ShotCamera = ShotCam;
				}

				GetWorldTimerManager().SetTimer(AutoShotTimerHandle, FTimerDelegate::CreateWeakLambda(
					this, [this]()
					{
						if (ER::Debug::IsShowNav())
						{
							if (APlayerController* PC2 = GetWorld()->GetFirstPlayerController())
							{
								PC2->ConsoleCommand(TEXT("show Navigation"));
							}
						}
						FScreenshotRequest::RequestScreenshot(true);
						UE_LOG(LogERBattle, Log, TEXT("[Battle] AutoShot: 已请求截图。"));
					}), 2.5f, false);
			}), 1.5f, false);
	}

	// 创建并启动战斗管理器 (战斗随机种子 = f(Run 种子, 遭遇种子), 完全可复现)
	BattleManager = NewObject<UBattleManager>(this, TEXT("BattleManager"));
	const int32 BattleSeed = DeriveBattleSeed(GetRunSubsystem() ? GetRunSubsystem()->GetRunSeed() : 0, PendingEncounterSeed);
	BattleManager->InitializeBattle(this, PlayerUnits, EnemyUnits, BattleSeed);

	// 无论是否来自航线图都要接结果: 结算面板对两种来源都要弹
	BattleManager->OnBattleFinished.AddDynamic(this, &ABattleGameMode::HandleBattleFinished);

	if (bRunBattle)
	{
		// 调试用：ER.AutoWin > 0 时战斗开始若干秒后自动判胜，便于验证"战斗→航线图"回环。
		const int32 AutoWin = CVarAutoWin.GetValueOnGameThread();
		if (AutoWin > 0)
		{
			GetWorldTimerManager().SetTimer(AutoWinTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, AutoWin]()
			{
				UE_LOG(LogERBattle, Log, TEXT("[Battle] ER.AutoWin 触发，自动判胜。"));
				// 走和作弊按钮完全相同的路径: 管理器判胜负 -> GameMode 结算 -> 结算面板
				if (BattleManager)
				{
					BattleManager->DebugKillAllEnemies();
				}
				else
				{
					HandleBattleFinished(true);
				}

				// ER.AutoWin 2 = 连"点结算面板返回"也自动做一遍, 用于无头验证完整回环
				if (AutoWin > 1)
				{
					GetWorldTimerManager().SetTimer(AutoReturnTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						UE_LOG(LogERBattle, Log, TEXT("[Battle] ER.AutoWin=2: 自动点击结算返回。"));
						ReturnFromBattle();
					}), 2.5f, false);
				}
			}), 4.0f, false);
		}
	}

	OnBattleSetupComplete.Broadcast();
}

void ABattleGameMode::SpawnTeam(const TArray<FBattleUnitStats>& Roster, const TArray<FVector>& Points, EBattleTeam Team, TArray<ABattleUnit*>& OutUnits)
{
	OutUnits.Reset();
	UWorld* World = GetWorld();
	const int32 Count = FMath::Min(Roster.Num(), Points.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		const ABattleUnit* CDO = UnitClass ? GetDefault<ABattleUnit>(UnitClass) : GetDefault<ABattleUnit>();
		const float HalfHeight = CDO->GetCapsuleHalfHeight();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// 敌我双方朝向对方
		const FRotator Facing(0.0f, Team == EBattleTeam::Player ? 0.0f : 180.0f, 0.0f);
		const FVector Loc = Points[i] + FVector(0.0f, 0.0f, HalfHeight + 5.0f);

		if (ABattleUnit* Unit = World->SpawnActor<ABattleUnit>(UnitClass ? UnitClass.Get() : ABattleUnit::StaticClass(), Loc, Facing, Params))
		{
			Unit->InitUnit(Roster[i], Team);
			OutUnits.Add(Unit);
		}
	}
}

AActor* ABattleGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const FString CurrentMap = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (CurrentMap.Contains(TEXT("Hall")))
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AHallCameraPawn> It(World); It; ++It)
			{
				if (AHallCameraPawn* ExistingPawn = *It)
				{
					return ExistingPawn;
				}
			}
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* ABattleGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	const FString CurrentMap = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (CurrentMap.Contains(TEXT("Hall")))
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AHallCameraPawn> It(World); It; ++It)
			{
				if (AHallCameraPawn* ExistingPawn = *It)
				{
					return ExistingPawn;
				}
			}
		}
	}
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}

URunSubsystem* ABattleGameMode::GetRunSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<URunSubsystem>();
	}
	return nullptr;
}

void ABattleGameMode::HandleBattleFinished(bool bPlayerWon)
{
	if (bBattleResultHandled)
	{
		return;
	}
	bBattleResultHandled = true;
	bLastBattleWon = bPlayerWon;

	// 跳转不再自动发生: 结算面板弹出后由玩家点按钮决定何时离开,
	// 这样面板可以停留展示战果, 也方便以后往里加掉落 / 经验等结算内容。
	if (bRunBattle)
	{
		if (URunSubsystem* Run = GetRunSubsystem())
		{
			if (Run->HasActiveRun())
			{
				if (bPlayerWon)
				{
					Run->WriteBackParty(PlayerUnits, PlayerPartyIndices);
					Run->CompleteBattle(true);
					UE_LOG(LogERBattle, Log, TEXT("[Battle] 节点 %d 战斗胜利，等待玩家点击结算面板返回航线图"), PendingNodeId);
				}
				else
				{
					Run->CompleteBattle(false);
					UE_LOG(LogERBattle, Log, TEXT("[Battle] 本局失败，等待玩家点击结算面板返回大厅"));
				}
			}
		}
	}
	else
	{
		UE_LOG(LogERBattle, Log, TEXT("[Battle] 非航线图战斗结束 (胜利=%s)，等待玩家点击结算面板"),
			bPlayerWon ? TEXT("是") : TEXT("否"));
	}
}

void ABattleGameMode::ReturnFromBattle()
{
	if (ShouldReturnToRoute())
	{
		ReturnToRoute();
	}
	else
	{
		ReturnToHall();
	}
}

void ABattleGameMode::DebugKillAllEnemies()
{
	if (BattleManager)
	{
		BattleManager->DebugKillAllEnemies();
	}
}

void ABattleGameMode::ReturnToRoute()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Battle/Maps/Lvl_Route")), true,
		TEXT("game=/Script/EbbingRainbow.RouteGameMode"));
}

void ABattleGameMode::ReturnToHall()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Battle/Maps/Lvl_Hall")), true,
		TEXT("game=/Script/EbbingRainbow.HallGameMode"));
}
