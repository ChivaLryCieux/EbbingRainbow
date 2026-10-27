// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"

class SBorder;
class SButton;

/**
 * 液态玻璃药丸按钮 (苹果质感 · 近未来主义):
 * - 烟熏半透明玻璃底 + 白色发丝描边, 大圆角胶囊造型
 * - 主按钮 (bPrimary) 带彩色弥散光晕与缓慢呼吸动画
 * - 悬停: 玻璃提亮 / 描边增强 (指数平滑过渡)
 * - 按压: 整体轻微缩放, 模拟液态回弹手感
 */
class EBBINGRAINBOW_API SHallGlassButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHallGlassButton)
		: _Label(FText::GetEmpty())
		, _Caption(FText::GetEmpty())
		, _AccentColor(FLinearColor(0.32f, 0.76f, 1.0f, 1.0f))
		, _bPrimary(false)
		, _ButtonWidth(264.0f)
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Caption)
		SLATE_ARGUMENT(FLinearColor, AccentColor)
		SLATE_ARGUMENT(bool, bPrimary)
		SLATE_ARGUMENT(float, ButtonWidth)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** 依据悬停/按压/呼吸状态刷新画刷与缩放 */
	void RefreshVisuals(double CurrentTime, float DeltaTime);

	// 外观参数
	FLinearColor AccentColor = FLinearColor(0.32f, 0.76f, 1.0f, 1.0f);
	bool bPrimary = false;

	// 子控件
	TSharedPtr<SButton> RootButton;
	TSharedPtr<SBorder> GlowBorder;
	TSharedPtr<SBorder> BodyBorder;
	TSharedPtr<SBorder> LedBorder;

	// 动态画刷 (每帧按动画状态原地修改后 Invalidate)
	FSlateBrush GlowBrush;
	FSlateBrush BodyBrush;
	FSlateBrush LedBrush;

	// 动画状态 (0..1, 指数平滑)
	float HoverAlpha = 0.0f;
	float PressAlpha = 0.0f;
};
