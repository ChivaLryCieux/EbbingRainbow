// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERAssetCatalog.h"
#include "Core/ERLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

UERAssetCatalog* UERAssetCatalog::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	return World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UERAssetCatalog>() : nullptr;
}

UObject* UERAssetCatalog::LoadAsset(const TCHAR* Path)
{
	if (!Path || !*Path)
	{
		return nullptr;
	}
	return LoadAsset(FSoftObjectPath(Path));
}

UObject* UERAssetCatalog::LoadAsset(const FSoftObjectPath& Path)
{
	if (!Path.IsValid())
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<UObject>* Found = Cache.Find(Path))
	{
		if (UObject* Obj = Found->Get())
		{
			return Obj;
		}
	}

	UObject* Obj = Path.TryLoad();
	if (Obj)
	{
		Cache.Add(Path, Obj);
	}
	else if (!Cache.Contains(Path))
	{
		// 只告警一次, 失败路径也记入缓存避免反复尝试
		Cache.Add(Path, nullptr);
		UE_LOG(LogEbbingRainbow, Warning, TEXT("[AssetCatalog] 加载失败: %s"), *Path.ToString());
	}
	return Obj;
}
