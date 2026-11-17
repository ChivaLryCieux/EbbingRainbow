// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Input/Reply.h"
#include "Run/RunSubsystem.h"

class SRouteEdgeLayer;
class SRouteLayerRuler;
class SRouteNodeWidget;
class SCanvas;
class STextBlock;
class SBorder;

/** 节点被点击（只对可选节点触发）。 */
DECLARE_DELEGATE_OneParam(FOnRouteNodeClicked, int32);

/**
 * 航线图主界面（纯 Slate）。
 *
 * 布局：全屏背景压暗 + 居中的固定虚拟画布（ERRouteLayout::MapW x MapH，按窗口等比缩放）。
 * 层级沿 X 轴从左到右推进（起点在左、终点在右），Slot 沿 Y 轴分布；
 * 固定虚拟坐标的好处是节点位置只由 Layer/Slot 决定，窗口尺寸变化不会导致连线错位。
 * 节点坐标/画布常数全部来自 Run/RouteLayout.h，与 3D 场景共用。
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

	/** 顶部临时提示（例如"安全节点 · 无事发生"）。 */
	void ShowNotification(const FText& Message, float Duration = 2.6f);

	/**
	 * 画布在视口中的真实落点: OutOriginPx = 画布(0,0)的视口像素, OutScalePx = 画布单位->像素。
	 * 读 Slate 实际布局结果 (不自己推公式), 首帧布局完成前返回 false。
	 */
	bool GetMapLayout(FVector2D& OutOriginPx, float& OutScalePx) const;

private:
	FReply HandleNodeButtonClicked(int32 NodeId);
	void RebuildNodes();
	FText GetStatLabel() const;
	FText GetProgressLabel() const;

	TWeakObjectPtr<URunSubsystem> RunSubsystem;
	FOnRouteNodeClicked OnNodeClicked;

	TSharedPtr<SRouteEdgeLayer> EdgeLayer;
	TSharedPtr<SRouteLayerRuler> Ruler;
	TSharedPtr<SCanvas> NodeCanvas;
	TArray<TSharedPtr<SRouteNodeWidget>> NodeWidgets;
	TArray<FVector2D> NodePositions;

	TSharedPtr<STextBlock> StatText;
	TSharedPtr<STextBlock> ProgressText;
	TSharedPtr<SBorder> NotificationBorder;
	TSharedPtr<STextBlock> NotificationText;

	float NotificationTimeLeft = 0.0f;

	FSlateBrush PillBrush;
};
