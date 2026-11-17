// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERInteractiveWidget.generated.h"

class UButton;

/**
 * 可交互 UI 元素基类: 全局悬停注册表。
 * HUD 用它判断"鼠标是否在任何卡片/标记/按钮上", 防止点击穿透到战场。
 * 子类重载 GetHoverCheckWidget() 返回真正用于命中检测的内部控件 (通常是内部 UButton)。
 */
UCLASS(Abstract)
class EBBINGRAINBOW_API UERInteractiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 命中检测目标: 默认用自身根控件; 子类可指向内部 UButton */
	virtual UWidget* GetHoverCheckWidget() const { return const_cast<UERInteractiveWidget*>(this); }

	/** 任何已注册交互控件当前正被悬停 */
	static bool IsAnyHovered();

private:
	static TArray<TWeakObjectPtr<UERInteractiveWidget>>& Registry()
	{
		static TArray<TWeakObjectPtr<UERInteractiveWidget>> R;
		return R;
	}
};
