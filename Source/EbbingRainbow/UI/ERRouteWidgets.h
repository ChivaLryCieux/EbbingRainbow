// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Run/RunTypes.h"
#include "Run/RouteLayout.h"
#include "Run/UI/SRouteMapVisuals.h"
#include "ERRouteWidgets.generated.h"

class SERRouteSigilLeaf;
class SERRouteEdgeLeaf;
class SERRouteRulerLeaf;
class SERRouteTypeIconLeaf;

/**
 * 航线图节点印记 UMG 原子控件:
 * 出现在 UMG Palette ("ER UI|Route") 中, 可在 WBP_RouteNode 里直接选中调节半径、环宽、颜色与预览状态。
 * 严守视觉铁律: 仅 Current 节点带微弱内晕 (外层大范围辉光交由 3D M_Route_Halo), 其余状态一律不发光。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Route Node Sigil"))
class EBBINGRAINBOW_API UERRouteNodeSigil : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	ERouteNodeType NodeType = ERouteNodeType::Battle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	FLinearColor AccentColor = FLinearColor(1.00f, 0.44f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	FLinearColor CurrentGoldColor = FLinearColor(1.0f, 0.80f, 0.34f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	bool bIsFinal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	float BaseRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	float FinalScaleMultiplier = 1.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteSigil")
	FVector2D DesiredWidgetSize = FVector2D(88.0f, 88.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	ERouteNodeVisual VisualState = ERouteNodeVisual::Available;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bReachable = true;

	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteSigil")
	void SetNodeParams(ERouteNodeType InType, FLinearColor InAccent, bool bInFinal, float InPhase);

	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteSigil")
	void SetVisualState(ERouteNodeVisual InVisual, bool bInReachable);

	void AdvanceInteraction(float DeltaTime, bool bHovered, bool bPressed);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Route"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	float Phase = 0.0f;
	TSharedPtr<SERRouteSigilLeaf> MySigil;
};

/**
 * 航线图贝塞尔连线层 UMG 控件:
 * 在 WBP_RouteHUD 中绘制全部 DAG 实线路径, 支持在 Details 中调各状态颜色/线宽, 并在设计期渲染完整预览图。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Route Edge Layer"))
class EBBINGRAINBOW_API UERRouteEdgeLayer : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	FLinearColor AvailableEdgeColor = FLinearColor(0.78f, 0.90f, 1.00f, 0.80f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	float AvailableEdgeThickness = 1.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	FLinearColor TakenEdgeColor = FLinearColor(1.00f, 0.82f, 0.46f, 0.70f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	float TakenEdgeThickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	FLinearColor ReachableEdgeColor = FLinearColor(0.62f, 0.74f, 0.95f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	float ReachableEdgeThickness = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	FLinearColor LockedEdgeColor = FLinearColor(0.55f, 0.65f, 0.85f, 0.10f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	float LockedEdgeThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteEdges")
	float EdgeTrimRadius = ERRouteLayout::EdgeTrim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bShowDesignPreview = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewSeed = 42;

	void SetEdges(const TArray<SRouteEdgeLayer::FEdgeDraw>& InEdges);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Route"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TArray<SRouteEdgeLayer::FEdgeDraw> CachedEdges;
	TSharedPtr<SERRouteEdgeLeaf> MyEdgeLeaf;
};

/**
 * 航线图底部层级标尺 UMG 控件:
 * 绘制层刻度线、当前菱形点金与层号文字 (默认接入复合中文字体 F_ERUI_Composite)。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Route Layer Ruler"))
class EBBINGRAINBOW_API UERRouteLayerRuler : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteRuler")
	float RulerY = ERRouteLayout::RulerY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteRuler")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.80f, 0.34f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteRuler")
	FLinearColor BaseLineColor = FLinearColor(0.60f, 0.72f, 0.92f, 0.20f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteRuler")
	FText FinalLayerText = INVTEXT("终");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteRuler")
	int32 NumberFontSize = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteRuler")
	int32 ActiveNumberFontSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewNumLayers = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	int32 PreviewCurrentLayer = 2;

	void SetRulerData(const TArray<float>& InLayerXs, int32 InCurrentLayer);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Route"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TArray<float> CachedLayerXs;
	int32 CachedCurrentLayer = 0;
	TSharedPtr<SERRouteRulerLeaf> MyRulerLeaf;
};

/**
 * 航线图图例小图标 UMG 控件:
 * 绘制安全(圆)/战斗(菱形)/自然(六边形)轮廓图标, 供 WBP_RouteLegendItem 与自定义图例使用。
 */
UCLASS(BlueprintType, meta = (DisplayName = "ER Route Type Icon"))
class EBBINGRAINBOW_API UERRouteTypeIcon : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteIcon")
	ERouteNodeType NodeType = ERouteNodeType::Safe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteIcon")
	FLinearColor AccentColor = FLinearColor(0.42f, 0.78f, 0.92f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteIcon")
	float ShapeRadius = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteIcon")
	float StrokeThickness = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteIcon")
	FVector2D IconDesiredSize = FVector2D(20.0f, 20.0f);

	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteIcon")
	void SetIconParams(ERouteNodeType InType, FLinearColor InAccent);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return INVTEXT("ER UI|Route"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SERRouteTypeIconLeaf> MyIconLeaf;
};
