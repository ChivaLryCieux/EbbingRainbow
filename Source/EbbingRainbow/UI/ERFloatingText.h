// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERFloatingText.generated.h"

class UImage;
class UTextBlock;
class UWidgetAnimation;

/**
 * 战场伤害/治疗/护盾/状态跳字 UMG 控件 (WBP_FloatingText):
 * 由 ABattleFloatingText 的屏幕空间 WidgetComponent 承载, 支持中文复合字体、阴影描边及蓝图自定义。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERFloatingText : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|FloatingText")
	void SetFloatingData(const FText& InText, FLinearColor InColor, float InWorldSize = 36.0f);

	UFUNCTION(BlueprintCallable, Category = "ERUI|FloatingText")
	void SetPopupTransform(float InScale, float InAlpha);

	/** 参考世界字号 (InWorldSize 等于此值时使用 Designer 的基础字号, 更大/更小按比例缩放) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float ReferenceWorldSize = 36.0f;

	/** 默认基础字号 (仅当 C++ 兜底建树或允许按比例调字号时使用) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|FloatingText")
	float BaseFontSize = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewText = INVTEXT("-48 暴击");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FLinearColor PreviewColor = FLinearColor(0.95f, 0.35f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	float PreviewWorldSize = 44.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|FloatingText", meta = (DisplayName = "On Floating Text Acquired"))
	void BP_OnAcquired(const FText& InText, FLinearColor InColor, float InWorldSize);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FloatingTextBlock;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackdropImage;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Pop;

private:
	void Build();
	float SizeRatio = 1.0f;
	bool bBuilt = false;
};
