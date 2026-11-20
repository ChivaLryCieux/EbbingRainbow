// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "ERHallHUD.generated.h"

class UBorder;
class UCanvasPanel;
class UERCornerReticle;
class UERHallButton;
class UERPrismBar;
class UERTechDivider;
class UERTechFrame;
class UHorizontalBox;
class UTextBlock;
class UWidgetAnimation;

/**
 * 空间站大厅主界面 UMG 控件 (WBP_HallHUD):
 *  - 全屏静谧四角光学标尺 (UERCornerReticle), 92%+ 空间留给日光建筑圣殿
 *  - 左上《控制:共振》式巨型粗体标题碑: 「虹不我与」48pt 粗体 + 窄光谱虹彩条 + 极小 Linux Biolinum 遥测副标
 *  - 底部双机能切角主控模块: [01 人员管理] (黑曜石) + [02 航线管理] (反白石膏+虹彩点睛)
 *  - 错峰建筑展开 + 英文副标译码 + Esc/空白切观景模式
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERHallHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ERUI|Hall")
	void ToggleZenMode();

	UFUNCTION(BlueprintCallable, Category = "ERUI|Hall")
	void SetZenMode(bool bInZenMode);

	UFUNCTION(BlueprintPure, Category = "ERUI|Hall")
	bool IsZenMode() const { return bZenMode; }

	UFUNCTION(BlueprintCallable, Category = "ERUI|Hall")
	void LaunchRouteMap();

	// ---- 可调参数 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Hall")
	FName RouteLevelName = FName(TEXT("/Game/Battle/Maps/Lvl_Route"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Hall")
	FString RouteLevelOptions = TEXT("game=/Script/EbbingRainbow.RouteGameMode");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Hall")
	FText ZenHintMessage = INVTEXT("ZEN MODE // 观景模式 · 点击任意位置或按 ESC 返回");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	bool bEnableEntranceAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceDuration = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	float EntranceOffsetY = 24.0f;

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
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERCornerReticle> CornerReticle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MastheadContainer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERTechFrame> MastheadFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERPrismBar> MastheadPrism;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MastheadTagText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MastheadTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERTechDivider> MastheadDivider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MastheadMetaText;
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
	void ApplyEntrancePose();
	UWorld* ResolvePlayWorld() const;

	FVector2D BaseDockTranslation = FVector2D::ZeroVector;
	FVector2D BaseMastheadTranslation = FVector2D::ZeroVector;
	float BaseDockOpacity = 1.0f;
	bool bBaseCached = false;

	bool bZenMode = false;
	FERRevealTrack EntranceTrack;
	FERCipherReveal MastheadTagCipher;
	FERCipherReveal MastheadMetaCipher;
	bool bLayoutBuilt = false;
};
