// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERPipBar.generated.h"

class UHorizontalBox;
class UTexture2D;

/**
 * AP 点阵条: 方块点替代文字 (国际主义 — 形状即信息)。
 * 实格=骨白, 空格=幽灵线框色; 数量变化时才重建。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERPipBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 更新点阵: Filled 实格数, Total 总格数 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|PipBar")
	void SetPips(int32 Filled, int32 Total);

	/** 实格颜色 (默认骨白; 敌方/特殊态可换) */
	UFUNCTION(BlueprintCallable, Category = "ERUI|PipBar")
	void SetPipColor(FLinearColor Color);

	/** 单格边长覆盖 (< 0 时取 UERUITheme::PipSize) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PipBar")
	float PipSizeOverride = -1.0f;

	/** 格间距覆盖 (< 0 时取 UERUITheme::PipGap) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PipBar")
	float PipGapOverride = -1.0f;

	/** 点阵贴图 (缺省用纯白贴图, 可换圆点/菱形贴图) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PipBar")
	TObjectPtr<UTexture2D> PipTexture = nullptr;

	/** 为 false 时使用下方自定义 FilledColor / EmptyColor, 不被 Theme 覆盖 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PipBar")
	bool bUseThemeColors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PipBar", meta = (EditCondition = "!bUseThemeColors"))
	FLinearColor CustomFilledColor = FLinearColor(0.92f, 0.90f, 0.87f, 1.00f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|PipBar", meta = (EditCondition = "!bUseThemeColors"))
	FLinearColor CustomEmptyColor = FLinearColor(0.92f, 0.90f, 0.87f, 0.16f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewFilled = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewTotal = 5;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	void EnsureContainer();
	void Rebuild();
	void RefreshColors();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PipBox;

	FLinearColor PipColor = FLinearColor::White;
	bool bHasExplicitColor = false;
	int32 CurFilled = -1;
	int32 CurTotal = -1;
	bool bBuilt = false;
};
