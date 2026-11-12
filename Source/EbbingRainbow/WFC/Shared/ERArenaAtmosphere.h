// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
struct FRandomStream;

/**
 * 竞技场氛围共享实现 —— Voxel/Nature 两个生成器原先各抄一份 (~120 行逐字重复)。
 *
 * 只在 -Atmo 命令行显式开启时生效 (ER::Debug::IsAtmoEnabled):
 * 关卡自摆光照雾效归用户手调, 程序氛围仅供离线截图/诊断。
 */
namespace ER::Atmosphere
{
	/**
	 * 种子化三点光 + 虚空雾。
	 * 关卡里已放的 ExponentialHeightFog/SkyLight/SkyAtmosphere 会被调谐或隐藏,
	 * 不重复 spawn; 新生成的演员记入 OutSpawned 供 ClearGenerated 统一销毁。
	 * @param Owner       归属生成器 (Spawn 的 Owner + 跳过自身迭代的基准)
	 * @param Rng         种子化随机流 (主光方向随种子变化)
	 * @param OutSpawned  新生成的氛围演员容器
	 */
	void SpawnVoidAtmosphere(AActor* Owner, FRandomStream& Rng, TArray<TObjectPtr<AActor>>& OutSpawned);
}
