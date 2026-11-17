// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERPipBar.generated.h"

class UHorizontalBox;

/**
 * AP 点阵条: 方块点替代文字 (国际主义 — 形状即信息)。
 * 实格=骨白, 空格=幽灵线框色; 数量变化时才重建。
 */
UCLASS()
class EBBINGRAINBOW_API UERPipBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 更新点阵: Filled 实格数, Total 总格数 */
	void SetPips(int32 Filled, int32 Total);

	/** 实格颜色 (默认骨白; 敌方/特殊态可换) */
	void SetPipColor(FLinearColor Color) { PipColor = Color; RefreshColors(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void Rebuild();
	void RefreshColors();

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PipBox;

	FLinearColor PipColor = FLinearColor::White;
	int32 CurFilled = -1;
	int32 CurTotal = -1;
	bool bBuilt = false;
};
