// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UBattleDebugSubsystem.generated.h"

class ABattleGameMode;
class ACameraActor;

/**
 * 战斗调试自动化 —— 原先散在 ABattleGameMode::HandleArenaGenerated 里:
 *  - ER.AutoShot / -AutoShot / -AutoShotTop / -ShowNav: 竞技场就绪后拉临时俯瞰相机截图
 *  - ER.AutoWin: 战斗开始数秒后自动判胜 (AutoWin>1 连结算返回也点掉), 验证"战斗->航线图"回环
 *
 * 全部是作弊/离线截图路径, Shipping 下整体为空实现。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleDebugSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 竞技场就绪且双方单位生成后调用; 按 CVar/命令行开关决定是否排调试任务 */
	void OnArenaReady(ABattleGameMode* GameMode);

private:
	/** 调试截图用的临时俯瞰相机 (仅非 Shipping 下被赋值) */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ShotCamera = nullptr;

#if !UE_BUILD_SHIPPING
	void ScheduleAutoShot(ABattleGameMode* GameMode);
	void ScheduleAutoWin(ABattleGameMode* GameMode, int32 AutoWin);

	FTimerHandle AutoShotTimerHandle;
	FTimerHandle AutoWinTimerHandle;
	FTimerHandle AutoReturnTimerHandle;
#endif
};
