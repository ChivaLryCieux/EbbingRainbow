// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleFloatingText.generated.h"

class UTextRenderComponent;

/**
 * 战场 3D 浮动跳字 (伤害数字 / 治疗 / 状态提示)
 * 自带向上浮动、重力回落、缩放冲击与淡出销毁动画, 始终朝向玩家摄像机。
 */
UCLASS()
class EBBINGRAINBOW_API ABattleFloatingText : public AActor
{
	GENERATED_BODY()

public:
	ABattleFloatingText();

	virtual void Tick(float DeltaSeconds) override;

	/** 设置浮动文字内容、初始颜色与基础缩放 */
	void SetupFloatingText(const FString& Text, const FColor& InColor, float InWorldSize = 36.0f);

	/** 便捷静态生成函数 */
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
	float BaseWorldSize = 36.0f;
};
