// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Run/RunTypes.h"
#include "ERRouteHUD.generated.h"

class UBorder;
class UCanvasPanel;
class UERRouteEdgeLayer;
class UERRouteLayerRuler;
class UERRouteLegendItem;
class UERRouteNode;
class UHorizontalBox;
class UImage;
class UOverlay;
class URunSubsystem;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnERRouteHUDNodeSelected, int32, NodeId);

/** 允许在 WBP_RouteHUD 上按节点类型覆盖名字/单字/主题色/自定义图标贴图 */
USTRUCT(BlueprintType)
struct FERRouteNodeStyleOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Route")
	FText DisplayNameOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Route")
	FText GlyphOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Route")
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Route", meta = (EditCondition = "bOverrideColor"))
	FLinearColor ColorOverride = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Route")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
};

/**
 * 航线图主界面 UMG 控件 (WBP_RouteHUD):
 *  - 包含: 顶/底渐隐遮罩、左上标题组、右上层数/战绩、左下动态图例、底部引导语、顶部胶囊通知、
 *    以及中央 ScaleToFit 2040x980 虚拟画布 (Ruler + EdgeLayer + NodeCanvas)
 *  - GetMapLayout 直接读取 NodeCanvas 的真实 Slate 视口几何, 保证 3D ARouteSceneBuilder 光晕像素级对齐
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERRouteHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRouteUI(URunSubsystem* InRunSubsystem);

	/** 重新读取 Run 状态并刷新节点/连线/标尺/统计 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteHUD")
	void Refresh();

	/** 顶部胶囊提示 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteHUD")
	void ShowNotification(const FText& Message, float Duration = 2.6f);

	/** NodeCanvas 在视口中的真实像素原点与缩放 (供 ARouteSceneBuilder 3D 对齐) */
	bool GetMapLayout(FVector2D& OutOriginPx, float& OutScalePx) const;

	UPROPERTY(BlueprintAssignable, Category = "ERUI|RouteHUD")
	FOnERRouteHUDNodeSelected OnNodeSelected;

	// ---- 子蓝图类与节点样式覆盖 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERRouteNode> NodeWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERRouteLegendItem> LegendItemClass;

	/** 可选: 在 WBP_RouteHUD 上直接按类型覆盖颜色、字形或替换成美术图标贴图 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|NodeStyles")
	TMap<ERouteNodeType, FERRouteNodeStyleOverride> NodeTypeOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Layout")
	float LegendItemSpacing = 22.0f;

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewGraphInDesigner = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewSeed = 424242;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewNotification = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewNotificationText = INVTEXT("安全节点 · 无事发生");

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleNodeClicked(int32 NodeId);

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TopScrim;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> BottomScrim;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScaleBox> MapScaleBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> MapSizeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UOverlay> MapOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERRouteLayerRuler> Ruler;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERRouteEdgeLayer> EdgeLayer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> NodeCanvas;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TitleAccentRule;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ProgressText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> LegendBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HintText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> NotificationBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NotificationText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Intro;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Notify;

private:
	void BuildLayout();
	void BuildDesignPreviewGraph();
	void RebuildLegend();
	void RebuildNodes();
	void ResolveTypeVisual(ERouteNodeType Type, FText& OutName, FText& OutGlyph, FLinearColor& OutColor, UTexture2D*& OutIcon) const;

	TWeakObjectPtr<URunSubsystem> RunSubsystem;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UERRouteNode>> NodeWidgets;

	TArray<FVector2D> NodePositions;
	float NotificationTimeLeft = 0.0f;
	bool bLayoutBuilt = false;
};
