// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RouteCameraPawn.generated.h"

class UCameraComponent;
class USceneComponent;

/**
 * 航线图关卡的观察者。当前 UI 是纯 Slate 全屏绘制，这里只提供一个稳定的相机，
 * 以后想加 3D 星图/飞船时直接在这里做平移与缩放即可。
 */
UCLASS()
class EBBINGRAINBOW_API ARouteCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARouteCameraPawn();

	UCameraComponent* GetRouteCamera() const { return Camera; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Route")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Route")
	TObjectPtr<UCameraComponent> Camera;
};
