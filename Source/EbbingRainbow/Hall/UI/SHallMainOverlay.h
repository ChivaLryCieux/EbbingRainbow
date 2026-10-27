// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"

class SBorder;

/**
 * 空间站主界面覆层 (苹果液态玻璃 · 近未来极简):
 * 仅保留底部居中的两枚玻璃药丸按钮 ——
 *   [人员管理 PERSONNEL] (暂无功能)
 *   [航线管理 ROUTES]    (临时作为开始战斗入口, 直通 Lvl_Battle)
 * 按 Esc 可切换全屏观景模式, 欣赏 3D 场景。
 */
class EBBINGRAINBOW_API SHallMainOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHallMainOverlay)
		: _OwningWorld(nullptr)
	{}
		SLATE_ARGUMENT(UWorld*, OwningWorld)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** 切换全屏无 UI 观景模式 (Esc) */
	void ToggleZenMode();

	/** 发起出击进入战斗关卡 Lvl_Battle */
	void LaunchBattleLevel();

private:
	TWeakObjectPtr<UWorld> OwningWorld;
	UWorld* FindPlayWorld() const;

	// UI 容器
	TSharedPtr<SBorder> MainDashboardContainer;
	TSharedPtr<SBorder> DockContainer;
	TSharedPtr<SBorder> ZenHintBorder;

	bool bZenMode = false;
	float EntranceProgress = 0.0f;   // 底部 Dock 入场动画 0..1

	// 观景模式提示胶囊底座
	FSlateBrush ZenHintBrush;

	FReply HandlePersonnelClicked();  // 人员管理 (暂无功能)
	FReply HandleRoutesClicked();     // 航线管理 → 开始战斗 (临时)
};
