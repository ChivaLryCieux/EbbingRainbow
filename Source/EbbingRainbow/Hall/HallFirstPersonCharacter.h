// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Experimental/LegionBattle/FPS/LegionCommanderCharacter.h"
#include "HallFirstPersonCharacter.generated.h"

/**
 * 大厅第一人称备战化身:
 * 直接继承军团战场指挥官 (Enhanced Input 移动/视角/跳跃/疾跑、第一人称手臂与相机
 * 以及同一套角色运动参数), 但清空出生武器并解除开火/切枪绑定。
 * 输入封锁条件改由 HallPlayerController 的标题菜单/终端面板状态驱动。
 */
UCLASS(Blueprintable)
class EBBINGRAINBOW_API AHallFirstPersonCharacter : public ALegionCommanderCharacter
{
	GENERATED_BODY()

public:
	AHallFirstPersonCharacter();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void DoAim(float Yaw, float Pitch) override;
	virtual void DoMove(float Right, float Forward) override;
	virtual void DoJumpStart() override;
	virtual void DoJumpEnd() override;
	virtual void SetSprinting(bool bSprint) override;

private:
	bool IsHallInputBlocked() const;
};
