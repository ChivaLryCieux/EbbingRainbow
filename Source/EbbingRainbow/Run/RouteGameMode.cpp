// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RouteGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "Run/RouteCameraPawn.h"
#include "Run/RouteHUD.h"
#include "Run/RoutePlayerController.h"
#include "Run/RunSubsystem.h"
#include "Run/RouteGraphGenerator.h"

ARouteGameMode::ARouteGameMode()
{
	DefaultPawnClass = ARouteCameraPawn::StaticClass();
	PlayerControllerClass = ARoutePlayerController::StaticClass();
	HUDClass = ARouteHUD::StaticClass();
}

void ARouteGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UGameInstance* GameInstance = GetGameInstance();
	URunSubsystem* Run = GameInstance ? GameInstance->GetSubsystem<URunSubsystem>() : nullptr;
	if (!Run)
	{
		UE_LOG(LogTemp, Error, TEXT("[Route] 找不到 URunSubsystem。"));
		return;
	}

	if (Run->HasActiveRun())
	{
		// 从战斗返回：CompleteBattle 已消费待定节点；若是中途退出战斗则清掉残留。
		Run->ClearPendingNode();
		UE_LOG(LogTemp, Log, TEXT("[Route] 继续本局 seed=%d 当前节点=%d"), Run->GetRunSeed(), Run->GetCurrentNodeId());
		return;
	}

	int32 Seed = DebugSeed;
	const FString SeedOption = UGameplayStatics::ParseOption(Options, TEXT("seed"));
	if (!SeedOption.IsEmpty())
	{
		Seed = FCString::Atoi(*SeedOption);
	}
	if (Seed == 0)
	{
		Seed = FMath::RandRange(1, 2147483646);
	}

	if (!Run->StartNewRun(Seed))
	{
		UE_LOG(LogTemp, Error, TEXT("[Route] 新局生成失败 seed=%d"), Seed);
	}
}

void ARouteGameMode::StartPlay()
{
	Super::StartPlay();

	UGameInstance* GameInstance = GetGameInstance();
	URunSubsystem* Run = GameInstance ? GameInstance->GetSubsystem<URunSubsystem>() : nullptr;
	if (Run && Run->HasActiveRun())
	{
		UE_LOG(LogTemp, Log, TEXT("[Route] %s"), *FRouteGraphGenerator::MakeReport(Run->GetGraph()));
	}
}
