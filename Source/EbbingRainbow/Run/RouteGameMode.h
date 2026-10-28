// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RouteGameMode.generated.h"

/**
 * 航线图关卡 GameMode。
 * 进入关卡时：若已有进行中的 Run 就继续，否则用种子（URL ?seed= / DebugSeed / 随机）开新局。
 */
UCLASS()
class EBBINGRAINBOW_API ARouteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARouteGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;

	/** 调试用：非 0 时优先使用该种子（直接 PIE Lvl_Route 时生效） */
	UPROPERTY(EditDefaultsOnly, Category = "Route")
	int32 DebugSeed = 0;
};
