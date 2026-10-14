// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"

class STextBlock;
class SBorder;
class SButton;
class SHorizontalBox;
class SVerticalBox;

/**
 * 明日方舟风格的高科技空间站主界面 Slate 覆层:
 * 1. 顶部栏: 系统按钮、实时日期时间走字、电池电量、三合一货币资源槽 (信用点/合成玉/源石)
 * 2. 左侧空间: 留空展示 3D 悬浮空间站与星球, 配备指挥官等级徽章与“全屏观景(隐藏UI)”功能
 * 3. 右侧面板: 经典的明日方舟大磁贴交互仪表盘:
 *    - 核心磁贴 [Terminal 战术终端 / 作战出击] (直通战斗关卡 L_Battle)
 *    - 次级磁贴 [Squads 编队] (可展开4人特勤小队属性弹窗)
 *    - 次级磁贴 [Operator 干员与卡组]
 *    - 商业磁贴 [Store 采购中心] & [Recruitment 公开招募]
 *    - 基础磁贴 [Mission 任务] & [Base 空间基建] & [Depot 仓库]
 */
class EBBINGRAINBOW_API SHallMainOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHallMainOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** 切换全屏无 UI 观景模式 (方便欣赏背景中 3D 空间站与星球) */
	void ToggleZenMode();

	/** 打开/关闭编队详情弹窗 */
	void ToggleSquadsModal();

	/** 发起出击进入战斗关卡 L_Battle */
	void LaunchBattleLevel();

private:
	// 动态时钟文本
	TSharedPtr<STextBlock> TimeTextBlock;

	// UI 容器与观景模式控制
	TSharedPtr<SOverlay> RootOverlay;
	TSharedPtr<SBorder> MainDashboardContainer;
	TSharedPtr<SBorder> ZenHintBorder;

	// 编队弹窗
	TSharedPtr<SBorder> SquadsModalBorder;

	bool bZenMode = false;
	bool bSquadsModalOpen = false;

	// 样式画刷
	FSlateBrush TerminalBgBrush;
	FSlateBrush SquadsBgBrush;
	FSlateBrush StoreBgBrush;
	FSlateBrush RecruitBgBrush;
	FSlateBrush MiniTileBgBrush;
	FSlateBrush TopBarPillBrush;
	FSlateBrush CurrencyChipBrush;
	FSlateBrush ModalBgBrush;
	FSlateBrush DarkGlassBrush;

	// 悬停动态效果支持
	float TerminalHoverAlpha = 0.0f;
	bool bTerminalHovered = false;

	FReply HandleTerminalClicked();
	FReply HandleSquadsClicked();
	FReply HandleOperatorClicked();
	FReply HandleStoreClicked();
	FReply HandleRecruitClicked();
	FReply HandleMissionClicked();
	FReply HandleBaseClicked();
	FReply HandleDepotClicked();
	FReply HandleQuitClicked();
	FReply HandleEyeToggleClicked();
};
