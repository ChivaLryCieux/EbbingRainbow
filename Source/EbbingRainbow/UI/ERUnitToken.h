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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnERTokenClicked, ABattleUnit*, Unit);

/**
 * 单位标记 (先攻条小方块 / 左侧编队方块):
 *   [队伍色刻度]
 *   [名字首字符]  <- 方块主体
 *   [2px 血线]
 *   [金色下划线] <- 仅当前行动单位
 *   [Micro 名字] <- 仅编队面板且 活跃/聚焦/悬停 时
 * 零文字默认态, 形状+颜色承载全部信息。
 */
UCLASS()
class EBBINGRAINBOW_API UERUnitToken : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	/** bIsSquad: 编队面板大标记 (可显名字); false: 先攻条小标记 */
	void Setup(ABattleUnit* InUnit, bool bIsSquad);

	/** 从单位刷新血线/死活/颜色 */
	void RefreshFromUnit();

	void SetActiveFlag(bool bInActive);
	void SetFocusedFlag(bool bInFocused);
	void SetShowName(bool bInShow) { bShowName = bInShow; UpdateNameVisibility(); }

	ABattleUnit* GetUnit() const { return Unit; }

	UPROPERTY(BlueprintAssignable, Category = "ERUI")
	FOnERTokenClicked OnTokenClicked;

	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHoverBegin();

	UFUNCTION()
	void HandleHoverEnd();

private:
	void Build(float ChipSize);
	void UpdateNameVisibility();
	void ApplyDeadVisual();

	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> Unit;

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
	TObjectPtr<USizeBox> ChipBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	bool bIsSquad = false;
	bool bShowName = false;
	bool bActive = false;
	bool bFocused = false;
	bool bHovered = false;
	bool bBuilt = false;
};
