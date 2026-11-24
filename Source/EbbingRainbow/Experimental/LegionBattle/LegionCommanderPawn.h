// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LegionCommanderPawn.generated.h"

class UCameraComponent;

/**
 * 军团战场指挥官化身 (第一人称):
 *  - 玩家在战场地表上步行/疾跑, 视角由控制器的 ControlRotation 驱动;
 *  - 活动范围被夹在寻路网格覆盖的战场内, 保证等高线地图始终覆盖玩家所在位置;
 *  - 本身不参与战斗结算, 兵团通过控制器命令 / 跟随指令行动。
 */
UCLASS()
class EBBINGRAINBOW_API ALegionCommanderPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ALegionCommanderPawn();

	virtual void Tick(float DeltaSeconds) override;

	void SetSprinting(bool bSprint);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float WalkSpeed = 780.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float SprintSpeed = 1900.0f;

	/** 眼睛离胶囊中心的高度 (cm); 胶囊半高 88 => 离地约 158cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float EyeOffsetZ = 70.0f;

	/** 战场边缘留白 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commander")
	float BoundsMargin = 600.0f;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;
};
