// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallGameMode.h"
#include "Core/ERLog.h"
#include "Core/ERDebug.h"
#include "Hall/HallCameraPawn.h"
#include "Hall/HallFPPawn.h"
#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"
#include "EngineUtils.h"

AHallGameMode::AHallGameMode()
{
	// 默认第一人称大厅 Pawn; -HallShowcase 回到旧的 HallCameraPawn 观景模式
	DefaultPawnClass = ER::Debug::IsHallShowcase()
		? AHallCameraPawn::StaticClass()
		: AHallFPPawn::StaticClass();
	PlayerControllerClass = AHallPlayerController::StaticClass();
	HUDClass = AHallHUD::StaticClass();
}

AActor* AHallGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (ER::Debug::IsHallShowcase())
	{
		// 优先将摆放好的 AHallCameraPawn 作为出生点，这样 SpawnTransform 和 SpawnRotation 会 100% 精确保留其位置与朝向
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

APawn* AHallGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// 观景模式: 如果场景中已经手动摆放了 AHallCameraPawn，优先接管该 Pawn，不再在默认原点重复生成
	if (ER::Debug::IsHallShowcase())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AHallCameraPawn> It(World); It; ++It)
			{
				if (AHallCameraPawn* ExistingPawn = *It)
				{
					UE_LOG(LogERHall, Log, TEXT("[HallGameMode] Possessing manually placed HallCameraPawn at %s"),
						*ExistingPawn->GetActorLocation().ToString());
					return ExistingPawn;
				}
			}
		}
	}
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}
