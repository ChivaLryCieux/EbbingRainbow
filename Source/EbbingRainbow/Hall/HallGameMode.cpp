// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/HallGameMode.h"
#include "Hall/HallCameraPawn.h"
#include "Hall/HallPlayerController.h"
#include "Hall/HallHUD.h"
#include "EngineUtils.h"

AHallGameMode::AHallGameMode()
{
	DefaultPawnClass = AHallCameraPawn::StaticClass();
	PlayerControllerClass = AHallPlayerController::StaticClass();
	HUDClass = AHallHUD::StaticClass();
}

AActor* AHallGameMode::ChoosePlayerStart_Implementation(AController* Player)
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
	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AHallGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// 如果场景中已经手动摆放了 AHallCameraPawn，优先接管该 Pawn，不再在默认原点重复生成
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHallCameraPawn> It(World); It; ++It)
		{
			if (AHallCameraPawn* ExistingPawn = *It)
			{
				UE_LOG(LogTemp, Log, TEXT("[HallGameMode] Possessing manually placed HallCameraPawn at %s"),
					*ExistingPawn->GetActorLocation().ToString());
				return ExistingPawn;
			}
		}
	}
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}
