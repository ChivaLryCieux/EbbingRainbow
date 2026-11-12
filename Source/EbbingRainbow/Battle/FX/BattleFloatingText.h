// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleFloatingText.generated.h"

class UTextRenderComponent;
class UBattleFXPool;

/**
 * 战场 3D 浮动跳字 (伤害数字 / 治疗 / 状态提示)
 * 自带向上浮动、重力回落、缩放冲击与淡出, 始终朝向玩家摄像机。
 *
 * 经 UBattleFXPool 池化: 到期不销毁, 隐藏回池复用。
 */
UCLASS()
class EBBINGRAINBOW_API ABattleFloatingText : public AActor
{
	GENERATED_BODY()

public:
	ABattleFloatingText();

	virtual void Tick(float DeltaSeconds) override;

	/** 池化获取入口 (由 UBattleFXPool 调用): 传送到位 + 重置内容 + 显示开 Tick */
	void PoolAcquire(const FVector& Location, const FString& Text, const FColor& InColor, float InWorldSize = 36.0f);

	/** 池化归还: 隐藏 + 停 Tick (由 UBattleFXPool 调用) */
	void PoolRelease();

	/** 便捷静态生成函数 (走对象池, 保留旧签名) */
	static ABattleFloatingText* SpawnFloatingText(UWorld* World, const FVector& Location, const FString& Text, const FColor& Color, float WorldSize = 36.0f);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> TextRender;

	FColor BaseColor = FColor(255, 80, 80);
	FVector Velocity = FVector::ZeroVector;
	float LifeTime = 1.1f;
	float ElapsedTime = 0.0f;
	float PopStartScale = 1.4f;

	/** 上一帧写入的 Alpha (量化后), 相同则跳过 SetTextRenderColor */
	uint8 LastWrittenAlpha = 255;
};
