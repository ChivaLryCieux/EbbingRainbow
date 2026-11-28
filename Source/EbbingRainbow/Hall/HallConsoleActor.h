// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HallConsoleActor.generated.h"

class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class AHallPlayerController;

/**
 * 大厅虹膜指挥终端控制台 (由关卡设计师手工摆放):
 * 玩家进入 InteractionSphere 且大致面朝控制台时, HallPlayerController 显示「按 E」提示;
 * 按 E 后由该 Actor 唤醒银河委托 / 兵团整备 / 银河局势终端。
 *
 * 默认使用 WFCKit 的 SM_prop_console 作为可替换占位网格, 所有交互参数均可在 Details 面板覆盖。
 */
UCLASS(Blueprintable, ClassGroup = "EbbingRainbow|Hall")
class EBBINGRAINBOW_API AHallConsoleActor : public AActor
{
	GENERATED_BODY()

public:
	AHallConsoleActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 执行交互 (默认打开银河指挥终端; InitialTab: 0=委托, 1=整备, 2=局势, 3=设置) */
	UFUNCTION(BlueprintCallable, Category = "Hall|Console")
	void Interact(AHallPlayerController* RequestingController);

	/** 视点是否满足交互距离与朝向要求; OutDistanceSq 供多控制台竞争时取最近者 */
	bool IsInteractionCandidate(const FVector& ViewLocation, const FVector& ViewForward, float& OutDistanceSq) const;

	UFUNCTION(BlueprintPure, Category = "Hall|Console")
	FText GetInteractionPrompt() const { return InteractionPrompt; }

	/** 首次打开终端时落在哪个页签; 面板内仍可切换全部页签 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hall|Console", meta = (ClampMin = "0", ClampMax = "3"))
	int32 InitialTab = 0;

	/** 交互半径 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hall|Console", meta = (ClampMin = "40.0", Units = "cm"))
	float InteractionRadius = 280.0f;

	/** 玩家视线在水平面上至少朝向前方的程度 (1=必须正对, 0=180° 内均可) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hall|Console", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinViewDot = 0.25f;

	/** 低于该距离时不再要求面朝 (玩家已经贴着控制台) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hall|Console", meta = (ClampMin = "0.0", Units = "cm"))
	float CloseRangeOverride = 130.0f;

	/** HUD 提示文案 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hall|Console")
	FText InteractionPrompt = INVTEXT("E · 接入虹膜指挥终端");

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ConsoleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> ScreenLight;
};
