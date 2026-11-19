// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ERInteractiveWidget.h"
#include "Run/RunTypes.h"
#include "ERRouteNode.generated.h"

class UButton;
class UERRouteNodeSigil;
class UERRouteTypeIcon;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnERRouteNodeClicked, int32, NodeId);

/**
 * 航线图图例单条目控件 (WBP_RouteLegendItem):
 * [矢量轮廓图标 / 贴图图标] + [节点类型名称]
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERRouteLegendItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteLegend")
	void SetupLegendItem(ERouteNodeType InType, const FText& InLabel, FLinearColor InAccent, UTexture2D* OptionalIcon = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteLegend", meta = (ExposeOnSpawn = "true"))
	ERouteNodeType NodeType = ERouteNodeType::Safe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteLegend", meta = (ExposeOnSpawn = "true"))
	FText LegendLabel = INVTEXT("安全节点");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteLegend", meta = (ExposeOnSpawn = "true"))
	FLinearColor AccentColor = FLinearColor(0.42f, 0.78f, 0.92f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteLegend")
	TObjectPtr<UTexture2D> CustomIcon = nullptr;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERRouteTypeIcon> TypeIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CustomIconImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LabelText;

private:
	void Build();
	bool bBuilt = false;
};

/**
 * 航线图单节点 UMG 控件 (WBP_RouteNode):
 *   [SButton 点击响应]
 *   [NodeSizeBox 120x150]
 *     [BadgeSizeBox 88x88] -> Overlay { Sigil, CustomIconImage(可选), GlyphText }
 *     [LabelText 节点名]
 * 全部子项均为标准可视 UMG 控件, 可在 WBP_RouteNode 中自由调节字号、阴影、图标贴图、印记样式与动画。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERRouteNode : public UERInteractiveWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteNode")
	void SetupNode(int32 InNodeId, ERouteNodeType InType, const FText& InGlyph, const FText& InLabel,
		const FText& InTooltip, FLinearColor InAccent, bool bInFinal, UTexture2D* OptionalIcon = nullptr);

	UFUNCTION(BlueprintCallable, Category = "ERUI|RouteNode")
	void SetNodeVisual(ERouteNodeVisual InVisual, bool bInReachable);

	UFUNCTION(BlueprintPure, Category = "ERUI|RouteNode")
	int32 GetNodeId() const { return NodeId; }

	UPROPERTY(BlueprintAssignable, Category = "ERUI|RouteNode")
	FOnERRouteNodeClicked OnRouteNodeClicked;

	// ---- 状态配色 (可在蓝图直接改) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteNode")
	FLinearColor CurrentGlyphColor = FLinearColor(1.0f, 0.90f, 0.52f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteNode")
	FLinearColor CurrentLabelColor = FLinearColor(1.0f, 0.86f, 0.46f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteNode")
	FLinearColor AvailableGlyphColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** 若传入了贴图图标则自动隐藏单字 GlyphText */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|RouteNode")
	bool bHideGlyphWhenIconValid = true;

	// ---- 设计期预览 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	ERouteNodeType PreviewNodeType = ERouteNodeType::Battle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	ERouteNodeVisual PreviewVisualState = ERouteNodeVisual::Available;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewReachable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewFinal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewGlyph = INVTEXT("战");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewLabel = INVTEXT("战斗节点");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FLinearColor PreviewAccent = FLinearColor(1.00f, 0.44f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	TObjectPtr<UTexture2D> PreviewIcon = nullptr;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|RouteNode", meta = (DisplayName = "On Node Visual Changed"))
	void BP_OnNodeVisualChanged(ERouteNodeVisual NewVisual, bool bIsReachable);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* GetHoverCheckWidget() const override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleClicked();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> HitButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> NodeSizeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> BadgeSizeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERRouteNodeSigil> Sigil;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CustomIconImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GlyphText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LabelText;

private:
	void Build();
	void BindDelegates();
	void ApplyVisual();

	int32 NodeId = INDEX_NONE;
	ERouteNodeVisual Visual = ERouteNodeVisual::Locked;
	bool bReachable = false;
	bool bVisualInit = false;
	FLinearColor Accent = FLinearColor::White;
	bool bBuilt = false;
	bool bDelegatesBound = false;
};
