// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HallHUD.generated.h"

class UHallMainWidget;

/**
 * 空间站大厅 HUD:
 * 加载并呈现明日方舟风格高科技大厅主界面, 支持观景模式切换。
 */
UCLASS()
class EBBINGRAINBOW_API AHallHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHallHUD();

	virtual void BeginPlay() override;

	/** 切换观景模式 */
	void ToggleZenMode();

	UHallMainWidget* GetHallWidget() const { return HallWidget; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UHallMainWidget> HallWidget;
};
