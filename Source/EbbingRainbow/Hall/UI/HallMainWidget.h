// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HallMainWidget.generated.h"

class SHallMainOverlay;

/**
 * 空间站大厅 UMG 控件:
 * 承载主界面液态玻璃底部 Dock (人员管理 / 航线管理与全屏观景)。
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
