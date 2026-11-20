// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "ERHeroPlate.generated.h"

class ABattleUnit;
class UERPipBar;
class UERTechFrame;
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
 * 大数字做主角, 文字减到最少。支持设计期实时数值预览与格式覆盖。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERHeroPlate : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|HeroPlate")
	void BindUnit(ABattleUnit* InUnit);

	UFUNCTION(BlueprintCallable, Category = "ERUI|HeroPlate")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "ERUI|HeroPlate")
	void ShowWaiting(bool bWaiting);

	// ---- 可调参数 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HeroPlate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHpThreshold = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|HeroPlate")
	FText WaitingPlaceholder = INVTEXT("—");

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewName = INVTEXT("观测者 · 01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewHp = 84;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewMaxHp = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewBarrier = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewAp = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewMaxAp = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewStatus = INVTEXT("稳态 · 掩体");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewIsPlayer = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|HeroPlate", meta = (DisplayName = "On Unit Bound"))
	void BP_OnUnitBound(ABattleUnit* BoundUnit);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UERTechFrame> PlateFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UnitTagText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HpNumber;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HpMaxText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BarrierText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HpTrailLine;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

private:
	void Build();
	void ApplyValues(const FString& DisplayName, bool bIsPlayer, float CurHp, float MaxHp, float Barrier, int32 CurAp, int32 MaxAp, const FString& StatusSummary);

	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> Unit;

	FERFloatSpring HpSpring = FERFloatSpring::Snappy(1.0f);
	FERFloatSpring HpTrailSpring = FERFloatSpring::Silky(1.0f);
	float TargetHpPct = 1.0f;
	float SweepProgress = 1.0f;
	bool bBuilt = false;
};
