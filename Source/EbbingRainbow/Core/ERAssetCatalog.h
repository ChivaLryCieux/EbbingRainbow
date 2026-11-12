// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ERAssetCatalog.generated.h"

/**
 * 资产路径目录: 替换散落的 LoadObject / StaticLoadObject / ConstructorHelpers。
 *
 * - 同一资产进程内只解析一次 (弱引用缓存, 被 GC 后可重载);
 * - 所有硬编码路径收敛到这一个类的调用点, 以后上软引用/异步预加载只需改这里;
 * - 路径统一用 /Game/ 软对象路径风格 (TEXT("/Game/...Asset.Asset"))。
 */
UCLASS()
class EBBINGRAINBOW_API UERAssetCatalog : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 世界上下文取目录; 无世界时返回 nullptr */
	static UERAssetCatalog* Get(const UObject* WorldContextObject);

	/** 同步加载 + 缓存。失败返回 nullptr 并打一条警告 (同一路径只告警一次) */
	UObject* LoadAsset(const TCHAR* Path);
	UObject* LoadAsset(const FSoftObjectPath& Path);

	template<typename T>
	T* LoadAsset(const TCHAR* Path) { return Cast<T>(LoadAsset(Path)); }

private:
	/** 已解析路径 -> 弱引用对象 */
	TMap<FSoftObjectPath, TWeakObjectPtr<UObject>> Cache;
};
