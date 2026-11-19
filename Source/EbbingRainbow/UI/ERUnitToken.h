// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "ERUnitToken.generated.h"

class ABattleUnit;
class UButton;
class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnERTokenClicked, ABattleUnit*, Unit);

/**
 * 单位标记 (先攻条小方块 / 左侧编队方块):
 *   [队伍色刻度]
 *   [名字首字符 / 头像]  <- 方块主体
 *   [2px 血线]
 *   [金色下划线] <- 仅当前行动单位
 *   [Micro 名字] <- 仅编队面板且 活跃/聚焦/悬停 时
 * 默认尊重 WBP Designer 中调好的尺寸与字号, 不强行覆盖。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERUnitToken : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	/** bIsSquad: 编队面板大标记 (可显名字); false: 先攻条小标记 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|UnitToken")
	void Setup(ABattleUnit* InUnit, bool bIsSquad);

	/** 从单位刷新血线/死活/颜色 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|UnitToken")
	void RefreshFromUnit();

	UFUNCTION(BlueprintCallable, Category = "ERUI|UnitToken")
	void SetActiveFlag(bool bInActive);

	UFUNCTION(BlueprintCallable, Category = "ERUI|UnitToken")
	void SetFocusedFlag(bool bInFocused);

	UFUNCTION(BlueprintCallable, Category = "ERUI|UnitToken")
	void SetShowName(bool bInShow) { bShowName = bInShow; UpdateNameVisibility(); }

	UFUNCTION(BlueprintPure, Category = "ERUI|UnitToken")
	ABattleUnit* GetUnit() const { return Unit; }

	UPROPERTY(BlueprintAssignable, Category = "ERUI|UnitToken")
	FOnERTokenClicked OnTokenClicked;

	// ---- 可在蓝图 Details 手调的表现开关 ----

	/** 为 false 时完全保留 WBP Designer 里给 ChipBox 设定的宽高; 为 true 时按 Theme 的 SquadChipSize/ChipSize 覆盖 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|UnitToken")
	bool bOverrideSizeFromTheme = false;

	/** 为 false 时保留 WBP Designer 里给 GlyphText 设定的字体/字号 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|UnitToken")
	bool bOverrideGlyphFontFromTheme = false;

	/** 始终显示单位名字 (不受悬停/激活限制) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|UnitToken")
	bool bAlwaysShowName = false;

	/** 阵亡时的整体透明度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|UnitToken")
	float DeadOpacity = 0.32f;

	/** 低血量变红阈值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|UnitToken")
	float LowHpThreshold = 0.30f;

	/** 可选默认立绘贴图 (传给 PortraitImage 动态材质的 Portrait 参数) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|UnitToken")
	TObjectPtr<UTexture2D> DefaultPortrait = nullptr;

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewGlyph = INVTEXT("锋");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewName = INVTEXT("观测者");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewHpPercent = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewIsPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewFocused = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewAlive = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|UnitToken", meta = (DisplayName = "On Unit Token Refreshed"))
	void BP_OnUnitRefreshed(ABattleUnit* InUnit, float HpPercent, bool bIsActive, bool bIsFocused);

	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHoverBegin();

	UFUNCTION()
	void HandleHoverEnd();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TeamTick;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GlyphText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HpLine;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ActiveRule;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> FocusedFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> ChipBox;

	/** 头像 (WBP 提供): 灰度比例=已损失 HP, 从右上角向左下角推进 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

private:
	void Build(float ChipSize);
	void BindDelegates();
	void UpdateNameVisibility();
	void ApplyDeadVisual();
	void ApplyPreview();

	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> Unit;

	bool bIsSquad = false;
	bool bShowName = false;
	bool bActive = false;
	bool bFocused = false;
	bool bHovered = false;
	bool bBuilt = false;
	bool bBuiltInCode = false;
	bool bDelegatesBound = false;
};
