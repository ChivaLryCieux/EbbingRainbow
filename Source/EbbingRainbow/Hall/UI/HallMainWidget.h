// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HallMainWidget.generated.h"

class SHallMainOverlay;

/**
 * 空间站大厅 UMG 控件:
 * 承载明日方舟风格高科技主界面 (终端/编队/干员/采购/基建/全屏观景)。
 */
UCLASS()
class EBBINGRAINBOW_API UHallMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ToggleZenMode();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SHallMainOverlay> SlateOverlay;
};
