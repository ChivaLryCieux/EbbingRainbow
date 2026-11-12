// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HallFPPawn.generated.h"

class UCameraComponent;

/**
 * 空间站大厅第一人称 Pawn:
 * ACharacter 胶囊行走 + 眼位相机, WASD 移动 / Shift 加速 / 鼠标视角。
 * 输入用 PlayerInput->IsKeyPressed 轮询 (与工程现有 EKeys 风格一致, 不依赖轴映射)。
 * 默认由 HallGameMode 生成; -HallShowcase 命令行回到旧的 HallCameraPawn 观景模式。
 */
UCLASS()
class EBBINGRAINBOW_API AHallFPPawn : public ACharacter
{
	GENERATED_BODY()

public:
	AHallFPPawn();

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** 行走/冲刺速度 (cm/s) 与鼠标灵敏度 */
	float WalkSpeed = 420.0f;
	float SprintSpeed = 760.0f;
	float LookSensitivity = 0.55f;
};
