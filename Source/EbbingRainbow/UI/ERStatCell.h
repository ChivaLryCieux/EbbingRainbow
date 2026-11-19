// Copyright Epic Games, Inc. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERStatCell.generated.h"

class UTextBlock;

/**
 * 单条属性格 (标签 + 数值), 人员管理面板的基本属性/抗性/次要数值共用。
 * 标签走 Micro 档 (字距放大), 数值走 Title 档; 颜色由调用方给 (学派色/危险色/骨白)。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERStatCell : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|StatCell")
	void SetStat(const FText& InLabel, const FText& InValue, FLinearColor InTint);

	UFUNCTION(BlueprintCallable, Category = "ERUI|StatCell")
	void SetValue(const FText& InValue, FLinearColor InTint);

	UFUNCTION(BlueprintCallable, Category = "ERUI|StatCell")
	void SetLabel(const FText& InLabel);

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewLabel = INVTEXT("完整度");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewValue = INVTEXT("60");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FLinearColor PreviewTint = FLinearColor(0.92f, 0.90f, 0.87f, 1.0f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

private:
	void Build();
	bool bBuilt = false;
};
