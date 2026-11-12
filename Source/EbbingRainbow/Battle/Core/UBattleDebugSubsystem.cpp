// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Core/UBattleDebugSubsystem.h"
#include "Core/ERDebug.h"
#include "Core/ERLog.h"
#include "Battle/Core/BattleGameMode.h"
#include "Battle/Core/BattleManager.h"
#include "Battle/Arena/BattleArena.h"
#include "Engine/World.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

/** 调试：>0 时战斗开始 4 秒后自动判胜，便于验证"战斗→航线图"回环。 */
static TAutoConsoleVariable<int32> CVarAutoWin(
	TEXT("ER.AutoWin"), 0, TEXT("调试：>0 时战斗开始 4 秒后自动判定胜利。"), ECVF_Cheat);

/** 调试：>0 时竞技场就绪后自动拉远镜头截一张图，便于离线检查生成结果。 */
static TAutoConsoleVariable<int32> CVarAutoShot(
	TEXT("ER.AutoShot"), 0, TEXT("调试：>0 时竞技场就绪后自动截图到 Saved/Screenshots。"), ECVF_Cheat);
#endif

void UBattleDebugSubsystem::OnArenaReady(ABattleGameMode* GameMode)
{
#if !UE_BUILD_SHIPPING
	if (!GameMode)
	{
		return;
	}

	if (CVarAutoShot.GetValueOnGameThread() > 0 || ER::Debug::IsAutoShot())
	{
		ScheduleAutoShot(GameMode);
	}

	// 自动判胜只服务航线图回环验证; 非航线图战斗 AutoWin 没有意义
	if (GameMode->IsRunBattle())
	{
		const int32 AutoWin = CVarAutoWin.GetValueOnGameThread();
		if (AutoWin > 0)
		{
			ScheduleAutoWin(GameMode, AutoWin);
		}
	}
#endif
}

#if !UE_BUILD_SHIPPING
void UBattleDebugSubsystem::ScheduleAutoShot(ABattleGameMode* GameMode)
{
	UWorld* World = GetWorld();
	ABattleArena* Arena = GameMode->GetArena();
	if (!World || !Arena)
	{
		return;
	}

	const FVector Center = Arena->GetActorLocation() + FVector(3800.0f, 3800.0f, 0.0f);
	World->GetTimerManager().SetTimer(AutoShotTimerHandle, FTimerDelegate::CreateWeakLambda(
		this, [this, World, Center]()
		{
			// 临时俯瞰相机: 比战术镜头看得全, 便于离线检查整场构图
			// -AutoShotTop 用正俯机位评估平面构图/破碎外壳剪影
			const bool bTop = ER::Debug::IsAutoShotTop();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACameraActor* ShotCam = World->SpawnActor<ACameraActor>(
				ACameraActor::StaticClass(),
				Center + FVector(0.0f, 0.0f, bTop ? 10000.0f : 5600.0f),
				bTop ? FRotator(-90.0f, 0.0f, 0.0f)
					 : FRotator(-58.0f, -32.0f, 0.0f),
				Params);
			if (ShotCam)
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					PC->SetViewTargetWithBlend(ShotCam, 0.0f);
				}
				ShotCamera = ShotCam;
			}

			World->GetTimerManager().SetTimer(AutoShotTimerHandle, FTimerDelegate::CreateWeakLambda(
				this, [this, World]()
				{
					if (ER::Debug::IsShowNav())
					{
						if (APlayerController* PC2 = World->GetFirstPlayerController())
						{
							PC2->ConsoleCommand(TEXT("show Navigation"));
						}
					}
					FScreenshotRequest::RequestScreenshot(true);
					UE_LOG(LogERBattle, Log, TEXT("[Battle] AutoShot: 已请求截图。"));
				}), 2.5f, false);
		}), 1.5f, false);
}

void UBattleDebugSubsystem::ScheduleAutoWin(ABattleGameMode* GameMode, int32 AutoWin)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(AutoWinTimerHandle, FTimerDelegate::CreateWeakLambda(
		this, [this, GameMode, World, AutoWin]()
		{
			UE_LOG(LogERBattle, Log, TEXT("[Battle] ER.AutoWin 触发，自动判胜。"));
			// 走和作弊按钮完全相同的路径: 管理器判胜负 -> GameMode 结算 -> 结算面板
			if (UBattleManager* Manager = GameMode->GetBattleManager())
			{
				Manager->DebugKillAllEnemies();
			}
			else
			{
				GameMode->HandleBattleFinished(true);
			}

			// ER.AutoWin 2 = 连"点结算面板返回"也自动做一遍, 用于无头验证完整回环
			if (AutoWin > 1)
			{
				World->GetTimerManager().SetTimer(AutoReturnTimerHandle, FTimerDelegate::CreateWeakLambda(
					this, [GameMode]()
					{
						UE_LOG(LogERBattle, Log, TEXT("[Battle] ER.AutoWin=2: 自动点击结算返回。"));
						GameMode->ReturnFromBattle();
					}), 2.5f, false);
			}
		}), 4.0f, false);
}
#endif
