// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Input/Reply.h"
#include "Run/RunSubsystem.h"

class SRouteEdgeLayer;
class SRouteNodeWidget;
class SCanvas;
class STextBlock;
class SBorder;

/** 节点被点击（只对可选节点触发）。 */
DECLARE_DELEGATE_OneParam(FOnRouteNodeClicked, int32);

/**
 * 航线图主界面（纯 Slate）。
 *
 * 布局：全屏星空背景 + 居中的固定虚拟画布（1960x880，按窗口等比缩放）。
 * 层级沿 X 轴从左到右推进（起点在左、终点在右），Slot 沿 Y 轴分布；
 * 固定虚拟坐标的好处是节点位置只由 Layer/Slot 决定，窗口尺寸变化不会导致连线错位。
 */
class SRouteMapOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRouteMapOverlay)
		: _RunSubsystem(nullptr)
	{}
		SLATE_ARGUMENT(URunSubsystem*, RunSubsystem)
		SLATE_EVENT(FOnRouteNodeClicked, OnNodeClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** 重新读取 Run 状态并刷新所有节点/连线。 */
	void Refresh();

	/** 底部临时提示（例如"安全节点 · 无事发生"）。 */
	void ShowNotification(const FText& Message, float Duration = 2.6f);

private:
	FReply HandleNodeButtonClicked(int32 NodeId);
	void RebuildNodes();
	FText GetSeedLabel() const;
	FText GetProgressLabel() const;

	TWeakObjectPtr<URunSubsystem> RunSubsystem;
	FOnRouteNodeClicked OnNodeClicked;

	TSharedPtr<SRouteEdgeLayer> EdgeLayer;
	TSharedPtr<SCanvas> NodeCanvas;
	TArray<TSharedPtr<SRouteNodeWidget>> NodeWidgets;
	TArray<FVector2D> NodePositions;

	TSharedPtr<STextBlock> SeedText;
	TSharedPtr<STextBlock> ProgressText;
	TSharedPtr<SBorder> NotificationBorder;
	TSharedPtr<STextBlock> NotificationText;

	float NotificationTimeLeft = 0.0f;

	FSlateBrush PanelBrush;
	FSlateBrush PillBrush;

	static constexpr float MapWidth = 2040.0f;
	static constexpr float MapHeight = 920.0f;
	static constexpr float NodeWidth = 100.0f;
	static constexpr float NodeHeight = 112.0f;
	static constexpr float MarginX = 150.0f;
	static constexpr float MarginY = 84.0f;
};
