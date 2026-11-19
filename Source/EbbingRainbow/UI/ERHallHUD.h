// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ERHallHUD.generated.h"

class UBorder;
class UCanvasPanel;
class UERHallButton;
class UHorizontalBox;
class UTextBlock;
class UWidgetAnimation;

/**
 * 空间站大厅主界面 UMG 控件 (WBP_HallHUD):
 *  - 底部居中液态玻璃双按钮 Dock: [人员管理 PERSONNEL] + [航线管理 ROUTES]
 *  - Esc / 点击空白切换全屏观景模式 (Zen Mode)
 *  - 全部控件均在 WBP_HallHUD 可视树中供手动调整位置、文案、外观与动画
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERHallHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 切换全屏无 UI 观景模式 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Hall")
	void ToggleZenMode();

	UFUNCTION(BlueprintCallable, Category = "ERUI|Hall")
	void SetZenMode(bool bInZenMode);

	UFUNCTION(BlueprintPure, Category = "ERUI|Hall")
	bool IsZenMode() const { return bZenMode; }

	/** 发起远征, 打开航线图关卡 */
	UFUNCTION(BlueprintCallable, Category = "ERUI|Hall")
	void LaunchRouteMap();

	// ---- 可调参数 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Hall")
	FName RouteLevelName = FName(TEXT("/Game/Battle/Maps/Lvl_Route"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Hall")
	FString RouteLevelOptions = TEXT("game=/Script/EbbingRainbow.RouteGameMode");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Hall")
	FText ZenHintMessage = INVTEXT("观景模式 · 点击任意位置或按 Esc 返回界面");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	bool bEnableEntranceAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceOffsetY = 34.0f;

	// ---- 设计期预览 ----

	/** 在 Designer 中勾选可预览观景模式提示胶囊并调位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewZenMode = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Hall", meta = (DisplayName = "On Personnel Clicked"))
	void BP_OnPersonnelClicked();

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Hall", meta = (DisplayName = "On Routes Clicked"))
	void BP_OnRoutesClicked();

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|Hall", meta = (DisplayName = "On Zen Mode Changed"))
	void BP_OnZenModeChanged(bool bIsZen);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION() void HandlePersonnelClicked();
	UFUNCTION() void HandleRoutesClicked();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> DashboardContainer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> DockBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERHallButton> PersonnelButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERHallButton> RoutesButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> ZenHintBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ZenHintText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Intro;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_ZenEnter;

private:
	void BuildLayout();
	void ApplyZenState(bool bInZen);
	UWorld* ResolvePlayWorld() const;

	FVector2D BaseDockTranslation = FVector2D::ZeroVector;
	float BaseDockOpacity = 1.0f;
	bool bBaseCached = false;

	bool bZenMode = false;
	float EntranceProgress = 0.0f;
	bool bLayoutBuilt = false;
};
