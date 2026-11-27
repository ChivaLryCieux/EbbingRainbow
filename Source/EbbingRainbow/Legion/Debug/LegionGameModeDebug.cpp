// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Experimental/AITacticalCommand/ERAICommandPanel.h"
#include "Experimental/AITacticalCommand/UBattleAICommandService.h"
#include "Experimental/LegionBattle/LegionHUD.h"
#include "Experimental/LegionBattle/LegionHUDWidget.h"
#include "Experimental/LegionBattle/LegionPlayerController.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ALegionGameMode::HandleAutoShot()
{
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
