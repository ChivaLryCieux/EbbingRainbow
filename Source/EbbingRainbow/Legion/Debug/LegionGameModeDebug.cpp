// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Core/ERLog.h"
#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#include "Experimental/AITacticalCommand/UBattleAICommandService.h"
#include "Experimental/LegionBattle/FPS/LegionCommanderCharacter.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionHUDWidget.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Legion/LegionPerf.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

bool ALegionGameMode::RunLegionAISelfTest()
{
	int32 Fails = 0;
	auto Check = [&Fails](const char* Name, bool bOk, const FString& Detail = TEXT(""))
	{
		UE_LOG(LogERLegion, Log, TEXT("[LegionAISelfTest] %s: %s %s"),
			bOk ? TEXT("PASS") : TEXT("FAIL"), UTF8_TO_TCHAR(Name), *Detail);
		if (!bOk) { ++Fails; }
	};

	ALegionUnit* P0 = PlayerLegions.Num() > 0 ? PlayerLegions[0].Get() : nullptr;
	Check("have player legion for micro-AI inspection", P0 && !P0->IsWiped());

	if (P0 && !P0->IsWiped())
	{
		TMap<ELegionSoldierState, int32> Dist;
		int32 MovingCnt = 0;
		float YawSpread = 0.0f;
		P0->GetSoldierStateDistribution(Dist, MovingCnt, YawSpread);
		const int32 Alive = P0->GetAliveCount();
		const bool bAsyncMix = (Alive >= 4) ? (Dist.Num() >= 2 || (MovingCnt > 0 && MovingCnt < Alive)) : (Alive > 0);
		Check("soldiers in single legion are asynchronous (not all frozen or identical)",
			bAsyncMix && YawSpread > 10.0f,
			FString::Printf(TEXT("states=%d moving=%d/%d yawSpread=%.1fdeg"), Dist.Num(), MovingCnt, Alive, YawSpread));
	}

	// 检查敌方弱 AI 指令多样性 (至少 2 种不同战术指令)
	TSet<uint8> EnemyDirs;
	int32 ValidPatrolPts = 0;
	for (ALegionUnit* E : EnemyLegions)
	{
		if (!E || E->IsWiped()) { continue; }
		EnemyDirs.Add((uint8)E->GetDirective());
		if (E->GetDirective() == ELegionDirective::Patrol && E->GetPatrolWaypoints().Num() >= 2)
		{
			++ValidPatrolPts;
		}
	}
	Check("enemy legions exhibit diverse weak-AI directives",
		EnemyDirs.Num() >= 2,
		FString::Printf(TEXT("distinctDirectives=%d patrolUnits=%d"), EnemyDirs.Num(), ValidPatrolPts));

	// 测试戒备与接触通报切换
	if (P0 && EnemyLegions.Num() > 0 && EnemyLegions[0] && !EnemyLegions[0]->IsWiped())
	{
		ALegionUnit* PTest = PlayerLegions.Num() > 1 ? PlayerLegions[1].Get() : P0;
		PTest->CommandGuard(PTest->GetActorLocation(), PTest->GetFacingYaw());
		Check("CommandGuard sets Guard directive & Alertness>=Alert",
			PTest->GetDirective() == ELegionDirective::Guard && PTest->GetAlertness() != ELegionAlertness::Relaxed);
	}

	UE_LOG(LogERLegion, Log, TEXT("[LegionAISelfTest] done: %s (%d failed)"),
		Fails == 0 ? TEXT("ALL PASS") : TEXT("HAS FAILURES"), Fails);
	return Fails == 0;
}

#if !UE_BUILD_SHIPPING
void ALegionGameMode::ForceBattleEndForTest(bool bVictory)
{
	FinishBattle(bVictory);
}
#endif

void ALegionGameMode::HandleAutoShot()
{
#if !UE_BUILD_SHIPPING
	// -LegionPerf: 骨骼兵性能标尺 — 全军突击诱使高峰交战, 切俯瞰机位 (全兵可见的最坏渲染情形)
	// 采样真实帧耗时写 [LegionPerf] 单行日志, 供逐档兵力对照; 与其它截图自动化互斥
	if (bPerfTestRequested)
	{
		if (!bBattleStarted) { return; }
		PerfTestTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

		UWorld* World = GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;

		if (PerfTestStage == 0 && PerfTestTimer > 0.5f)
		{
			AllOutAssault(ELegionTeam::Player);
			PerfTestStage = 1;
		}
		else if (PerfTestStage == 1 && PerfTestTimer > 10.0f)
		{
			// 与 -LegionShot 相同的俯瞰机位: 全部士兵入镜, 构成渲染最坏情形
			const FRotator RelRot(0.0f, BattleAxisYawDeg - 90.0f, 0.0f);
			const FVector CamPos = DeployCenter + RelRot.RotateVector(FVector(-1400.0f, -2600.0f, 3400.0f));
			if (PC)
			{
				if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(CamPos,
					FRotator(-36.0f, FRotator::NormalizeAxis(68.0f + (BattleAxisYawDeg - 90.0f)), 0.0f)))
				{
					PC->SetViewTargetWithBlend(Cam, 0.4f, VTBlend_Linear);
				}
			}
			PerfTestStage = 2;
		}
		else if (PerfTestStage == 2 && PerfTestTimer > 12.0f)
		{
			PerfTestSamples.Reset(1024);
			LegionPerf::Reset();
			if (GEngine) { TArray<float> Discard; GEngine->GetAverageUnitTimes(Discard); }
			int32 AliveTotal = 0;
			for (ALegionUnit* L : PlayerLegions) { if (L) { AliveTotal += L->GetAliveCount(); } }
			for (ALegionUnit* L : EnemyLegions)  { if (L) { AliveTotal += L->GetAliveCount(); } }
			UE_LOG(LogERLegion, Log, TEXT("[LegionPerf] sampling start: aliveTotal=%d legions=%d/%d skeletal=%d"),
				AliveTotal, PlayerLegions.Num(), EnemyLegions.Num(), (int32)bUseAnimatedAlly);
			PerfTestStage = 3;
		}
		else if (PerfTestStage == 3)
		{
			PerfTestSamples.Add(FApp::GetDeltaTime() * 1000.0f);
			if (PerfTestTimer > 22.0f)
			{
				TArray<float> Sorted = PerfTestSamples;
				Sorted.Sort();
				float Sum = 0.0f;
				for (float Ms : Sorted) { Sum += Ms; }
				const float Mean = Sorted.Num() > 0 ? Sum / Sorted.Num() : 0.0f;
				const float P50 = Sorted.Num() > 0 ? Sorted[Sorted.Num() / 2] : 0.0f;
				const float P95 = Sorted.Num() > 0 ? Sorted[FMath::Min(Sorted.Num() - 1, (int32)(Sorted.Num() * 0.95f))] : 0.0f;
				const float MaxV = Sorted.Num() > 0 ? Sorted.Last() : 0.0f;
				UE_LOG(LogERLegion, Log,
					TEXT("[LegionPerf] frames=%d mean=%.2fms(%.0ffps) p50=%.2fms p95=%.2fms max=%.2fms"),
					Sorted.Num(), Mean, Mean > 0.0f ? 1000.0f / Mean : 0.0f, P50, P95, MaxV);
				if (GEngine)
				{
					TArray<float> UnitTimes;
					GEngine->GetAverageUnitTimes(UnitTimes);
					if (UnitTimes.Num() >= 5)
					{
						UE_LOG(LogERLegion, Log,
							TEXT("[LegionPerf] unit: frame=%.2fms game=%.2fms render=%.2fms gpu=%.2fms rhi=%.2fms"),
							UnitTimes[0], UnitTimes[1], UnitTimes[3], UnitTimes[2], UnitTimes[4]);
					}
				}
				{
					const LegionPerf::FAccum& A = LegionPerf::Get();
					const double Den = FMath::Max(1, A.Frames);
					UE_LOG(LogERLegion, Log,
						TEXT("[LegionPerf] gamecode/frame: microAI=%.2fms skelRep=%.2fms bullets=%.2fms unitTick(all)=%.2fms gameModeTick=%.2fms over %d frames"),
						A.MicroAIMs / Den, A.SkeletalRepMs / Den, A.BulletsMs / Den, A.UnitTickMs / Den, A.GameModeTickMs / Den, A.Frames);
					UE_LOG(LogERLegion, Log,
						TEXT("[LegionPerf] tickdetail/frame: directive=%.2fms terrainConform=%.2fms actorMove=%.2fms deathLoop=%.2fms"),
						A.DirectiveMs / Den, A.TerrainConformMs / Den, A.ActorMoveMs / Den, A.DeathLoopMs / Den);
				}
				if (PC && PC->GetPawn()) { PC->SetViewTargetWithBlend(PC->GetPawn(), 0.4f, VTBlend_Linear); }
				PerfTestStage = 4;
			}
		}
		return;
	}

	// -ResultShot: 验证战终结算画面 (游戏暂停 / 第一人称操控切断 / 鼠标出现 / 点击返回大厅)
	if (bResultShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		if (AutoShotStage == 0 && AutoShotTimer > 1.2f)
		{
			AutoShotStage = 1;
			if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
			{
				CV->Set(0);
			}
			const bool bTestVictory = !FParse::Param(FCommandLine::Get(), TEXT("ResultDefeat"));
			ForceBattleEndForTest(bTestVictory);

			TWeakObjectPtr<ALegionGameMode> WeakGM(this);
			TWeakObjectPtr<UGameInstance> WeakGI(GetGameInstance());
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakGM, WeakGI, bTestVictory](float)
			{
				ALegionGameMode* G = WeakGM.Get();
				UWorld* W = G ? G->GetWorld() : nullptr;
				ALegionPlayerController* PC = W ? Cast<ALegionPlayerController>(W->GetFirstPlayerController()) : nullptr;
				ALegionHUD* HUD = PC ? Cast<ALegionHUD>(PC->GetHUD()) : nullptr;
				ULegionHUDWidget* Widget = HUD ? HUD->GetHUDWidget() : nullptr;
				ALegionCommanderCharacter* Cmd = PC ? Cast<ALegionCommanderCharacter>(PC->GetPawn()) : nullptr;

				int32 Fails = 0;
				auto Check = [&Fails](const char* Name, bool bOk)
				{
					UE_LOG(LogERLegion, Log, TEXT("[LegionResultTest] %s: %s"),
						bOk ? TEXT("PASS") : TEXT("FAIL"), UTF8_TO_TCHAR(Name));
					if (!bOk) { ++Fails; }
				};

				Check("game is paused on battle end", G && UGameplayStatics::IsGamePaused(G) && G->GetTacticalTimeMode() == ELegionTimeMode::Paused);
				Check("controller locked into battle-over UI mode", PC && PC->IsBattleOverInputLocked());
				Check("mouse cursor is visible", PC && PC->bShowMouseCursor);
				Check("move & look inputs are ignored", PC && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());

				bool bAimBlocked = false;
				if (PC && Cmd)
				{
					const FRotator BeforeRot = PC->GetControlRotation();
					Cmd->DoAim(45.0f, 15.0f);
					bAimBlocked = PC->GetControlRotation().Equals(BeforeRot, 0.01f);
				}
				Check("first-person aim input blocked", bAimBlocked);
				Check("result overlay & return-hall button ready", Widget && Widget->IsResultOverlayVisible() && Widget->IsReturnHallButtonReady());
				UE_LOG(LogERLegion, Log, TEXT("[LegionResultTest] done: %s (%d failed)"),
					Fails == 0 ? TEXT("ALL PASS") : TEXT("HAS FAILURES"), Fails);

				const FString ShotName = bTestVictory ? TEXT("LegionResult_Victory") : TEXT("LegionResult_Defeat");
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / ShotName, true, true);

				// 验证点击「返回大厅」按钮能解除暂停并顺利切回 Lvl_Hall
				TWeakObjectPtr<ULegionHUDWidget> WeakWidget(Widget);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWidget, WeakGI](float)
				{
					if (ULegionHUDWidget* Wg = WeakWidget.Get())
					{
						Wg->ClickReturnHallForTest();
					}
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakGI](float)
					{
						if (UGameInstance* GI = WeakGI.Get())
						{
							UWorld* NewWorld = GI->GetWorld();
							const FString MapName = NewWorld ? NewWorld->GetMapName() : TEXT("<null>");
							const bool bAtHall = MapName.Contains(TEXT("Lvl_Hall"));
							const bool bUnpaused = NewWorld && !UGameplayStatics::IsGamePaused(NewWorld);
							UE_LOG(LogERLegion, Log, TEXT("[LegionResultTest] %s: ReturnToHall opened %s (unpaused=%d)"),
								(bAtHall && bUnpaused) ? TEXT("PASS") : TEXT("FAIL"), *MapName, bUnpaused ? 1 : 0);
						}
						return false;
					}), 2.5f);
					return false;
				}), 0.8f);
				return false;
			}), 0.5f);
		}
		return;
	}

	// -PauseShot: 验证 Esc 暂停画面 (游戏暂停 / 操控切断 / 光标出现 / 三按钮 / 设置弹窗 / 恢复)
	if (bPauseShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		if (AutoShotStage == 0 && AutoShotTimer > 1.2f)
		{
			AutoShotStage = 1;
			if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
			{
				CV->Set(0);
			}
			if (ALegionPlayerController* PC = GetWorld() ? Cast<ALegionPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr)
			{
				PC->SetPauseMenuOpen(true);
			}

			TWeakObjectPtr<ALegionGameMode> WeakGM(this);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakGM](float)
			{
				ALegionGameMode* G = WeakGM.Get();
				UWorld* W = G ? G->GetWorld() : nullptr;
				ALegionPlayerController* PC = W ? Cast<ALegionPlayerController>(W->GetFirstPlayerController()) : nullptr;
				ALegionHUD* HUD = PC ? Cast<ALegionHUD>(PC->GetHUD()) : nullptr;
				ALegionCommanderCharacter* Cmd = PC ? Cast<ALegionCommanderCharacter>(PC->GetPawn()) : nullptr;

				int32 Fails = 0;
				auto Check = [&Fails](const char* Name, bool bOk)
				{
					UE_LOG(LogERLegion, Log, TEXT("[LegionPauseTest] %s: %s"),
						bOk ? TEXT("PASS") : TEXT("FAIL"), UTF8_TO_TCHAR(Name));
					if (!bOk) { ++Fails; }
				};

				Check("game is paused when pause menu opens", G && UGameplayStatics::IsGamePaused(G) && G->GetTacticalTimeMode() == ELegionTimeMode::Paused);
				Check("controller pause state active", PC && PC->IsPauseMenuOpen());
				Check("mouse cursor is visible", PC && PC->bShowMouseCursor);
				Check("move & look inputs are ignored", PC && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());
				bool bAimBlocked = false;
				if (PC && Cmd)
				{
					const FRotator BeforeRot = PC->GetControlRotation();
					Cmd->DoAim(45.0f, 15.0f);
					bAimBlocked = PC->GetControlRotation().Equals(BeforeRot, 0.01f);
				}
				Check("first-person aim blocked while paused", bAimBlocked);
				Check("pause screen visible", HUD && HUD->IsPauseMenuVisible());

				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionPause"), true, true);

				TWeakObjectPtr<ALegionHUD> WeakHUD(HUD);
				TWeakObjectPtr<ALegionPlayerController> WeakPC(PC);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakGM, WeakHUD, WeakPC, Fails](float) mutable
				{
					ALegionHUD* H = WeakHUD.Get();
					ALegionPlayerController* P = WeakPC.Get();
					if (H) { H->OpenPauseSettings(); }

					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakGM, WeakHUD, WeakPC, Fails](float) mutable
					{
						ALegionHUD* H2 = WeakHUD.Get();
						ALegionPlayerController* P2 = WeakPC.Get();
						ALegionGameMode* G2 = WeakGM.Get();
						auto Check2 = [&Fails](const char* Name, bool bOk)
						{
							UE_LOG(LogERLegion, Log, TEXT("[LegionPauseTest] %s: %s"),
								bOk ? TEXT("PASS") : TEXT("FAIL"), UTF8_TO_TCHAR(Name));
							if (!bOk) { ++Fails; }
						};
						Check2("settings panel opened from pause menu", H2 && H2->IsPauseSettingsOpen());
						FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionPauseSettings"), true, true);

						FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakGM, WeakHUD, WeakPC, Fails](float) mutable
						{
							ALegionHUD* H3 = WeakHUD.Get();
							ALegionPlayerController* P3 = WeakPC.Get();
							ALegionGameMode* G3 = WeakGM.Get();
							if (H3) { H3->ClosePauseSettings(); }
							if (P3) { P3->SetPauseMenuOpen(false); }
							const bool bResumed = G3 && !UGameplayStatics::IsGamePaused(G3) && P3 && !P3->IsPauseMenuOpen();
							UE_LOG(LogERLegion, Log, TEXT("[LegionPauseTest] %s: resume restored unpaused state"),
								bResumed ? TEXT("PASS") : TEXT("FAIL"));
							if (!bResumed) { ++Fails; }
							UE_LOG(LogERLegion, Log, TEXT("[LegionPauseTest] done: %s (%d failed)"),
								Fails == 0 ? TEXT("ALL PASS") : TEXT("HAS FAILURES"), Fails);
							return false;
						}), 0.4f);
						return false;
					}), 0.4f);
					return false;
				}), 0.4f);
				return false;
			}), 0.4f);
		}
		return;
	}
#endif

	// -LegionAIShot: 四态连拍验证弱 AI 与单兵自治动画 (放松态 / 战略图巡逻侦察 / 警戒巡逻近景 / 交战抢射界近景)
	if (bAIShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		UWorld* World = GetWorld();
		ALegionPlayerController* LPC = World ? Cast<ALegionPlayerController>(World->GetFirstPlayerController()) : nullptr;
		APawn* Pawn = LPC ? LPC->GetPawn() : nullptr;
		ALegionUnit* P0 = PlayerLegions.Num() > 0 ? PlayerLegions[0].Get() : nullptr;
		ALegionUnit* PatrolEnemy = nullptr;
		for (ALegionUnit* E : EnemyLegions)
		{
			if (E && !E->IsWiped() && (E->GetDirective() == ELegionDirective::Patrol || E->GetDirective() == ELegionDirective::Recon))
			{
				PatrolEnemy = E;
				break;
			}
		}
		if (!PatrolEnemy && EnemyLegions.Num() > 0) { PatrolEnemy = EnemyLegions[0].Get(); }
		if (!LPC || !Pawn || !P0) { return; }

		auto DisableMotionBlur = []()
		{
			if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
			{
				CV->Set(0);
			}
		};
		auto TeleportWatch = [&](const ALegionUnit* TargetUnit, const FVector& Offset)
		{
			if (!TargetUnit) { return; }
			const FVector Anchor = TargetUnit->GetActorLocation();
			const FVector AimAt = Anchor + FVector(0.0f, 0.0f, 105.0f);
			const FVector Candidates[4] = {
				Offset,
				FVector(-Offset.X, Offset.Y, Offset.Z),
				FVector(Offset.Y, -Offset.X, Offset.Z),
				FVector(-Offset.Y, Offset.X, Offset.Z)
			};
			FVector BestPos = Anchor + Offset;
			int32 BestTreeScore = MAX_int32;
			for (const FVector& CandOff : Candidates)
			{
				FVector Cand = Anchor + CandOff;
				if (TerrainGrid.IsBuilt())
				{
					float GZ = Cand.Z, Slope = 0.0f;
					if (TerrainGrid.SampleHeightAndSlope(Cand, GZ, Slope))
					{
						Cand.Z = FMath::Max(Cand.Z, GZ + 185.0f);
					}
					const int32 Score = TerrainGrid.GetTreeCountWorld(Cand) * 3
						+ TerrainGrid.GetTreeCountWorld(FMath::Lerp(Cand, AimAt, 0.35f)) * 2
						+ TerrainGrid.GetTreeCountWorld(FMath::Lerp(Cand, AimAt, 0.70f));
					if (Score < BestTreeScore)
					{
						BestTreeScore = Score;
						BestPos = Cand;
						if (Score == 0) { break; }
					}
				}
				else
				{
					BestPos = Cand;
					break;
				}
			}
			const FRotator Face = (AimAt - BestPos).Rotation();
			Pawn->TeleportTo(BestPos, Face, false, true);
			LPC->SetControlRotation(Face);
		};
		auto Shoot = [](const FString& Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / Name, true, true);
		};

		if (AutoShotStage == 0 && AutoShotTimer > 1.0f)
		{
			DisableMotionBlur();
			for (ALegionUnit* L : PlayerLegions) { if (L) { L->ResetMotionTelemetry(); } }
			for (ALegionUnit* L : EnemyLegions)  { if (L) { L->ResetMotionTelemetry(); } }
			AutoShotStage = 1;
		}
		else if (AutoShotStage == 1 && AutoShotTimer > 2.2f)
		{
			// 1. 放松休整态近景 (P0 未接敌, 观察垂枪呼吸与个体异步漫步)
			TeleportWatch(P0, FVector(340.0f, -520.0f, 150.0f));
			AutoShotStage = 2;
		}
		else if (AutoShotStage == 2 && AutoShotTimer > 2.65f)
		{
			// 评估放松/巡逻态多帧稳定性 (Z跳变/高频翻转/AnimDir抖动)
			FString StabSummary;
			const bool bP0Stable = P0->EvaluateMotionStability(StabSummary);
			UE_LOG(LogERLegion, Log, TEXT("[LegionStabilityTest] %s: relaxed window P0 (%s)"),
				bP0Stable ? TEXT("PASS") : TEXT("FAIL"), *StabSummary);
			Shoot(TEXT("LegionAI_RelaxedA"));
			AutoShotStage = 3;
		}
		else if (AutoShotStage == 3 && AutoShotTimer > 2.95f)
		{
			Shoot(TEXT("LegionAI_RelaxedB"));
			AutoShotStage = 4;
		}
		else if (AutoShotStage == 4 && AutoShotTimer > 3.6f)
		{
			// 2. 运行弱 AI 自检并打开战略地图验证各旅巡逻/戒备/侦察路线与态势标签
			RunLegionAISelfTest();
			LPC->SetStrategicMapOpen(true);
			TWeakObjectPtr<ALegionGameMode> WeakGM(this);
			TWeakObjectPtr<ALegionPlayerController> WeakPC(LPC);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionAI_StrategicAI"), true, true);
				return false;
			}), 0.6f);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, WeakGM](float)
			{
				if (ALegionPlayerController* P = WeakPC.Get())
				{
					P->SetStrategicMapOpen(false);
					if (ALegionGameMode* G = WeakGM.Get())
					{
						if (G->PlayerLegions.Num() > 1 && G->PlayerLegions[1])
						{
							const FVector Base = G->PlayerLegions[1]->GetActorLocation();
							G->PlayerLegions[1]->CommandPatrol({ Base, Base + FVector(1200.0f, 1000.0f, 0.0f) });
						}
					}
				}
				return false;
			}), 1.3f);
			AutoShotStage = 5;
		}
		else if (AutoShotStage == 5 && AutoShotTimer > 6.5f)
		{
			// 3. 警戒巡逻/侦察部队近景 (观察持枪戒备八向移动与扇面扫视)
			TeleportWatch(PatrolEnemy ? PatrolEnemy : P0, FVector(420.0f, -560.0f, 180.0f));
			AutoShotStage = 6;
		}
		else if (AutoShotStage == 6 && AutoShotTimer > 6.95f)
		{
			Shoot(TEXT("LegionAI_AlertPatrol"));
			// 4. 全军突击触发交战态抢射界与战术游走
			AllOutAssault(ELegionTeam::Player);
			AutoShotStage = 7;
		}
		else if (AutoShotStage == 7 && AutoShotTimer > 12.0f)
		{
			// 避开上一张截图卡顿帧后再清零交战窗口稳定性遥测
			for (ALegionUnit* L : PlayerLegions) { if (L) { L->ResetMotionTelemetry(); } }
			for (ALegionUnit* L : EnemyLegions)  { if (L) { L->ResetMotionTelemetry(); } }
			AutoShotStage = 8;
		}
		else if (AutoShotStage == 8 && AutoShotTimer > 20.5f)
		{
			TeleportWatch(P0, FVector(380.0f, -540.0f, 165.0f));
			AutoShotStage = 9;
		}
		else if (AutoShotStage == 9 && AutoShotTimer > 21.0f)
		{
			TMap<ELegionSoldierState, int32> Dist;
			int32 MovingCnt = 0;
			float YawSpread = 0.0f;
			P0->GetSoldierStateDistribution(Dist, MovingCnt, YawSpread);
			UE_LOG(LogERLegion, Log, TEXT("[LegionAISelfTest] Combat micro-states on P0: types=%d moving=%d/%d yawSpread=%.1f"),
				Dist.Num(), MovingCnt, P0->GetAliveCount(), YawSpread);
			FString CombatStabSummary;
			const bool bCombatStable = P0->EvaluateMotionStability(CombatStabSummary);
			UE_LOG(LogERLegion, Log, TEXT("[LegionStabilityTest] %s: combat window P0 (%s)"),
				bCombatStable ? TEXT("PASS") : TEXT("FAIL"), *CombatStabSummary);
			Shoot(TEXT("LegionAI_CombatSkirmish"));
			AutoShotStage = 10;
		}
		return;
	}

	// -DeployShot: 接驳屏三态连拍 (加载中 -> 就绪 -> 进入战场后带草皮的战场实况);
	// 与别的截图链路不同, 此模式故意不跳过接驳屏 —— 它验证的就是接驳屏本身
	if (bDeployShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		if (AutoShotStage == 0 && AutoShotTimer > 1.0f)
		{
			// 加载中态: MinDwellSeconds=1.4 保证 1.0s 时必然未就绪, 按钮禁用
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionDeployLoading"), true, true);
			AutoShotStage = 1;
		}
		else if (AutoShotStage == 1 && AutoShotTimer > 4.5f)
		{
			// 就绪态: 加速构筑下通常 2~4s 接驳完成, 按钮激活
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionDeployReady"), true, true);
			AutoShotStage = 2;
		}
		else if (AutoShotStage == 2 && AutoShotTimer > 9.0f)
		{
			// 战场实况: 接驳屏就绪后 1.2s 自动点击进入, 此时草皮应已铺满视野
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionDeployField"), true, true);
			AutoShotStage = 3;
		}
		return;
	}

	if (bMapShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		UWorld* World = GetWorld();
		ALegionPlayerController* LPC = World ? Cast<ALegionPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!LPC) { return; }

		if (AutoShotStage == 0 && AutoShotTimer > 1.0f)
		{
			// 切开等高线地图视角 (含 1.0x 全景密集集群 + ERMapSelfTest 2.4x 细节命令双截图)
			LPC->SelectLegionByIndex(0);
			LPC->SetStrategicMapOpen(true);
			TWeakObjectPtr<ALegionPlayerController> WeakPC(LPC);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC](float)
			{
				if (ALegionPlayerController* P = WeakPC.Get())
				{
					// 先缩放到 1.0x 截整张等高线全景 (测试密集集群防碰撞)
					int32 SX = 0, SY = 0;
					P->GetViewportSize(SX, SY);
					for (int32 i = 0; i < 12; ++i) { P->ZoomMapAt(-1, FVector2D(SX * 0.5, SY * 0.5)); }
				}
				return false;
			}), 0.3f);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionMap1x"), true, true);
				return false;
			}), 0.8f);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC](float)
			{
				if (ALegionPlayerController* P = WeakPC.Get())
				{
					// 展开左上角「接入 AI」面板并运行大兵团 AI 工具链自检 (模拟"全部部队优先合力进攻对方第一旅")
					if (ALegionHUD* HUD = Cast<ALegionHUD>(P->GetHUD()))
					{
						if (ULegionHUDWidget* W = HUD->GetHUDWidget())
						{
							if (UERAICommandPanel* AI = W->GetAICommandPanel())
							{
								AI->SetPanelOpen(true);
							}
						}
					}
					if (UBattleAICommandService* Svc = UBattleAICommandService::Get(P->GetWorld()))
					{
						Svc->RunLegionToolSelfTest();
					}
				}
				return false;
			}), 1.4f);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionMapAI"), true, true);
				return false;
			}), 2.1f);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC](float)
			{
				if (ALegionPlayerController* P = WeakPC.Get()) { P->SetStrategicMapOpen(false); }
				return false;
			}), 2.8f);
			AutoShotStage = 1;
		}
		return;
	}

	if (bSkyShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		UWorld* World = GetWorld();
		ALegionPlayerController* LPC = World ? Cast<ALegionPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!LPC) { return; }

		if (AutoShotStage == 0 && AutoShotTimer > 1.0f)
		{
			// 1. 水平第一人称视角: 敌军头顶无番号, 只见直插天空的立柱与高空 Logo
			LPC->SelectLegionByIndex(0);
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionSkyHorizon"), true, true);
			AutoShotStage = 1;
		}
		else if (AutoShotStage == 1 && AutoShotTimer > 2.2f)
		{
			// 2. 运行防误触 + 抬头瞄准天空 Logo 右键集火自检
			LPC->ERSkyBannerSelfTest();
			AutoShotStage = 2;
		}
		else if (AutoShotStage == 2 && AutoShotTimer > 3.2f)
		{
			// 3. 此时镜头已抬头锁定天空 Logo 并下达集火, 截取抬头视角的锁定效果
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionSkyLookUp"), true, true);
			AutoShotStage = 3;
		}
		return;
	}

	if (bCloseupShotRequested)
	{
		AutoShotTimer += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		UWorld* World = GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		ALegionUnit* FirstUnit = PlayerLegions.Num() > 0 ? PlayerLegions[0].Get() : nullptr;
		if (!PC || !Pawn || !FirstUnit) { return; }

		// 传送与截图分两帧: 传送到编队侧翼 5m 后隔 0.3s 等 TAA 收敛再截 (运动模糊已在入场时关闭)
		auto TeleportNearLegion = [&]()
		{
			const FVector Anchor = FirstUnit->GetActorLocation();
			const FVector Pos = Anchor + FVector(280.0f, -420.0f, 110.0f);
			const FVector AimAt = Anchor + FVector(0.0f, 0.0f, 110.0f);
			const FRotator Face = (AimAt - Pos).Rotation();
			Pawn->TeleportTo(Pos, Face, false, true);
			PC->SetControlRotation(Face);
		};
		auto Shoot = [&](const FString& Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / Name, true, true);
		};

		if (AutoShotStage == 0 && AutoShotTimer > 0.5f)
		{
			if (IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
			{
				IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"))->Set(0);
			}
			AllOutAssault(ELegionTeam::Player);
			AutoShotStage = 1;
		}
		else if (AutoShotStage == 1 && AutoShotTimer > 8.0f)  { TeleportNearLegion(); AutoShotStage = 2; }
		else if (AutoShotStage == 2 && AutoShotTimer > 8.35f) { Shoot(TEXT("LegionCloseupA")); AutoShotStage = 3; }
		else if (AutoShotStage == 3 && AutoShotTimer > 15.0f) { TeleportNearLegion(); AutoShotStage = 4; }
		else if (AutoShotStage == 4 && AutoShotTimer > 15.35f){ Shoot(TEXT("LegionCloseupB")); AutoShotStage = 5; }
		else if (AutoShotStage == 5 && AutoShotTimer > 24.0f) { TeleportNearLegion(); AutoShotStage = 6; }
		else if (AutoShotStage == 6 && AutoShotTimer > 24.35f){ Shoot(TEXT("LegionCloseupC")); AutoShotStage = 7; }
		return;
	}

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
		const FRotator RelRot(0.0f, BattleAxisYawDeg - 90.0f, 0.0f);
		const FVector CamPos = DeployCenter + RelRot.RotateVector(FVector(-1400.0f, -2600.0f, 3400.0f));
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(CamPos,
			FRotator(-36.0f, FRotator::NormalizeAxis(68.0f + (BattleAxisYawDeg - 90.0f)), 0.0f));
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
	else if (AutoShotStage == 4 && AutoShotTimer > 3.0f)
	{
		// 回到第一人称视角后截一张战场视角图
		FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionFPS"), true, true);
		AutoShotStage = 5;
	}
	else if (AutoShotStage == 5 && AutoShotTimer > 4.5f)
	{
		// 等高线地图视角会暂停世界 (本 Tick 随之停摆), 所以改用 Core Ticker 延时截图
		if (ALegionPlayerController* LPC = Cast<ALegionPlayerController>(PC))
		{
			LPC->SetStrategicMapOpen(true);
		}
		// 世界暂停后本 Tick 停摆, 后续步骤都走 Core Ticker:
		//  +0.6s 跑地图交互自检 (缩放 / 点番号 / 集火 / 机动), 保持地图打开并下好命令
		//  +1.8s 截图 (带命令线 / 终点靶标 / 朝向箭头)
		//  +2.6s 点「返回战场」并收尾自检, +3.2s 截第一人称里的命令反馈
		TWeakObjectPtr<ALegionPlayerController> WeakPC = Cast<ALegionPlayerController>(PC);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC](float)
		{
			if (ALegionPlayerController* P = WeakPC.Get()) { P->RunMapSelfTest(false); }
			return false;
		}), 0.6f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionMap"), true, true);
			return false;
		}), 1.8f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC](float)
		{
			if (ALegionPlayerController* P = WeakPC.Get()) { P->FinishMapSelfTest(); }
			return false;
		}), 2.6f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("LegionOrders"), true, true);
			return false;
		}), 3.4f);
		AutoShotStage = 6;
	}
}
