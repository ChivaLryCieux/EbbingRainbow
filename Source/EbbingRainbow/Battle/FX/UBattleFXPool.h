// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UBattleFXPool.generated.h"

class ABattleFloatingText;

/**
 * 战场特效对象池 —— 当前只管浮动跳字 (ABattleFloatingText)。
 *
 * 原实现每次掉血 SpawnActor + 到点 Destroy: 高频小战斗里 GC/组件注册开销不小。
 * 池化后跳字到寿命只是隐藏 + 停 Tick, 下次复用; 超过上限时偷最老的活跃实例。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleFXPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBattleFXPool* Get(const UWorld* World);

	/** 生成 (或复用) 一个浮动跳字 */
	ABattleFloatingText* SpawnFloatingText(const FVector& Location, const FString& Text,
		const FColor& Color, float WorldSize);

	/** 由跳字自身到期时回调; 隐藏并压回空闲栈 */
	void ReleaseFloatingText(ABattleFloatingText* Text);

private:
	/** 空闲可复用实例 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABattleFloatingText>> FreeTexts;

	/** 全部已创建实例 (活跃+空闲), 用于上限淘汰 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABattleFloatingText>> AllTexts;

	/** 池上限; 超出时回收最老活跃实例 */
	int32 MaxPooledTexts = 48;
};
