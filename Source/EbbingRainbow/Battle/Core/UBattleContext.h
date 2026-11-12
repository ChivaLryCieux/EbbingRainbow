// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Battle/Skills/BattleRandomStream.h"
#include "UBattleContext.generated.h"

class ABattleUnit;
class UBattleManager;

/**
 * 战斗世界上下文 (WorldSubsystem) —— 取代散落的 TActorIterator 扫描
 * 与 GetAuthGameMode()->GetBattleManager() 逐级下钻。
 *
 * 职责:
 *  - 战斗单位注册表 (ABattleUnit 在 BeginPlay/EndPlay 自注册);
 *  - 战斗管理器指针缓存 (InitializeBattle 时登记);
 *  - 战斗随机流入口 (有管理器用管理器的种子流, 否则给兜底流)。
 *
 * 调用点全部变成 O(1) 子系统查询 + O(单位数) 的注册表遍历,
 * 注册表顺序 = 生成顺序, 确定性可复现。
 */
UCLASS()
class EBBINGRAINBOW_API UBattleContext : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBattleContext* Get(const UWorld* World);

	// ---------- 单位注册表 ----------

	void RegisterUnit(ABattleUnit* Unit);
	void UnregisterUnit(ABattleUnit* Unit);

	/** 全部已注册战斗单位 (按生成顺序; 含阵亡单位, 调用点自查 IsAlive) */
	const TArray<TObjectPtr<ABattleUnit>>& GetUnits() const { return Units; }

	/** 单位集合变更计数 (UI 轮询时对比, 避免每帧重建数组) */
	int32 GetUnitsRevision() const { return UnitsRevision; }

	// ---------- 管理器 / 随机流 ----------

	void SetManager(UBattleManager* InManager) { Manager = InManager; }
	UBattleManager* GetManager() const { return Manager; }

	/** 全战斗唯一随机源; 无管理器时返回兜底流 (固定种子, 仍可复现) */
	FBattleRandomStream* GetRng();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABattleUnit>> Units;

	UPROPERTY(Transient)
	TObjectPtr<UBattleManager> Manager = nullptr;

	int32 UnitsRevision = 0;
	FBattleRandomStream FallbackRng;
};
