// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERHeroPlate.generated.h"

class ABattleUnit;
class UERPipBar;
class UImage;
class UProgressBar;
class UTextBlock;

/**
 * 当前单位状态牌 (左下):
 *   名字 (Micro 弱化)
 *   [Display 大数字 HP]  [/Max]  [+N 屏障]
 *   [2px 血线]
 *   [AP 点阵]  [3/5]
 *   [状态摘要 Micro, 空则折叠]
 * 大数字做主角, 文字减到最少。
 */
UCLASS()
class EBBINGRAINBOW_API UERHeroPlate : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindUnit(ABattleUnit* InUnit);
	void Refresh();
	void ShowWaiting(bool bWaiting);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void Build();

	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> Unit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HpNumber;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HpMaxText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BarrierText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HpLine;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERPipBar> ApPips;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ApText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TeamTick;

	bool bBuilt = false;
};
