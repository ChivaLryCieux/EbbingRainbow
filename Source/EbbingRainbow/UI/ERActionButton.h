// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ERInteractiveWidget.h"
#include "ERActionButton.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnERActionClicked);

/**
 * 幽灵按钮 (发丝框 + Micro 文字 + 箭头):
 * 常态骨白发丝; 紧急态 (该点结束回合时) 描边转金并轻微呼吸。
 */
UCLASS()
class EBBINGRAINBOW_API UERActionButton : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	void SetLabel(const FText& InLabel);
	void SetButtonEnabled(bool bEnabled);
	void SetUrgent(bool bInUrgent);

	UPROPERTY(BlueprintAssignable, Category = "ERUI")
	FOnERActionClicked OnActionClicked;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

private:
	void Build();
	void ApplyStyle();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ArrowText;

	FText Label;
	bool bUrgent = false;
	bool bEnabledState = true;
	bool bBuilt = false;
	float UrgentPhase = 0.0f;
};
