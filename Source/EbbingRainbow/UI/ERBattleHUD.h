// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/Core/BattleTypes.h"
#include "UI/ERBattleResult.h"
#include "UI/Motion/ERUIMotion.h"
#include "ERBattleHUD.generated.h"

class ABattleGameMode;
class ABattlePlayerController;
class ABattleUnit;
class UBattleManager;
class UBorder;
class UCanvasPanel;
class UERActionButton;
class UERCornerReticle;
class UERHeroPlate;
class UERSkillCard;
class UERSkillTooltip;
class UERUnitToken;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidgetAnimation;

/**
 * 战斗 HUD (国际主义平面风, UMG 实现):
 *
 *  顶     —— 先攻条: 细线 + 单位方块标记 (当前金色下划线)
 *  左中   —— 编队方块列 (活跃/聚焦/悬停时显名)
 *  左下   —— 状态牌: Micro 名 / 大数字 HP / 血线 / AP 点阵
 *  底中   —— 竖条手牌 (费用数字+图标+主题色底线)
 *  右下   —— 幽灵"结束回合" (无事可做时转金呼吸)
 *  顶中偏下 —— 无框战报一行
 *  结算   —— letterbox 大词
 *
 * WBP_BattleHUD 提供完整可视控件树供手动拖拽/换皮, 所有子蓝图类/间距/动效/预览均暴露于 Details。
 */
UCLASS(BlueprintType, Blueprintable)
class EBBINGRAINBOW_API UERBattleHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeBattleUI(UBattleManager* InManager, ABattlePlayerController* InPC);

	/** 战报通知 (由 ABattleHUD::AddBattleLog 转发) */
	UFUNCTION(BlueprintCallable, Category = "ERUI|BattleHUD")
	void PostNotification(const FString& Message);

	/** 鼠标是否在任何交互控件上 (防点地穿透) */
	UFUNCTION(BlueprintPure, Category = "ERUI|BattleHUD")
	bool IsMouseOverInteractiveUI() const;

	// ---- 池化子蓝图类 (在 WBP Class Defaults 换任意蓝图子类) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERUnitToken> InitiativeTokenClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERUnitToken> SquadTokenClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Classes")
	TSubclassOf<UERSkillCard> SkillCardClass;

	// ---- 间距与布局细节 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Layout")
	float InitiativeTokenGap = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Layout")
	float SquadTokenGap = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Layout")
	float HandCardGap = 5.0f;

	// ---- 移动卡与文案配置 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|MoveCard")
	bool bShowMoveCard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|MoveCard")
	FText MoveCardName = INVTEXT("机动");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|MoveCard")
	FText MoveCardHotkey = INVTEXT("M");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|MoveCard")
	FLinearColor MoveCardAccent = FLinearColor(0.42f, 0.70f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|MoveCard")
	TObjectPtr<UTexture2D> MoveCardIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Labels")
	FText EndTurnLabel = INVTEXT("结束回合");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Labels")
	FText VictoryTitle = INVTEXT("胜  利");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Labels")
	FText DefeatTitle = INVTEXT("败  北");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Labels")
	FText ReturnToRouteLabel = INVTEXT("返回航线图");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Labels")
	FText ReturnToHallLabel = INVTEXT("返回空间站");

	// ---- 动效 (相对 Designer 基准偏移叠加, 不覆盖手调坐标) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	bool bEnableEntranceAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	FVector2D InitiativeEntranceOffset = FVector2D(0.0f, -14.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	FVector2D HandEntranceOffset = FVector2D(0.0f, 18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Motion")
	FVector2D SquadEntranceOffset = FVector2D(-14.0f, 0.0f);

	// ---- 设计期预览开关 (在 WBP_BattleHUD Details 中勾选即可直观调图层) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewPopulateContainers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview", meta = (ClampMin = "0", ClampMax = "10"))
	int32 PreviewInitiativeCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview", meta = (ClampMin = "0", ClampMax = "6"))
	int32 PreviewSquadCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview", meta = (ClampMin = "0", ClampMax = "8"))
	int32 PreviewHandCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	TArray<TObjectPtr<UTexture2D>> PreviewSkillIcons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewNotification = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	FText PreviewNotificationText = INVTEXT("观测者 施放 相位斩击 · 命中 48");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewTooltip = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERUI|Preview")
	bool bPreviewResultPanel = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|BattleHUD", meta = (DisplayName = "On Turn Changed"))
	void BP_OnTurnChanged(ABattleUnit* ActiveUnit);

	UFUNCTION(BlueprintImplementableEvent, Category = "ERUI|BattleHUD", meta = (DisplayName = "On Notification Posted"))
	void BP_OnNotificationPosted(const FString& Message);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- 控件树 (BindWidgetOptional: WBP 提供则用 WBP 的, 否则 C++ 建默认布局) ----
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERCornerReticle> CornerReticle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> InitiativeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> InitiativeRule;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> SquadBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERHeroPlate> HeroPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> HandBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERActionButton> EndTurnButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> NotifyBanner;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NotifyText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TopScrim;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> BottomScrim;

	// 悬停详情面板 (支持独立 WBP_SkillTooltip 子蓝图, 也兼容旧内联 TooltipRoot)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERSkillTooltip> SkillTooltip;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TooltipRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TooltipBg;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TooltipTitle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TooltipMeta;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERBattleResult> ResultPanel;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Intro;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Notify;

private:
	void BuildLayout();
	void BuildDesignPreview();
	void CacheDesignerBaseTransforms();

	// ---- 刷新 (修订号比对, 事件委托只在能挂的地方挂) ----
	void RebuildInitiative();
	void RebuildSquad();
	void RefreshHand(ABattleUnit* Unit);
	void RefreshHeroPlate(ABattleUnit* Unit);
	void RefreshEndTurn(ABattleUnit* Unit, EBattleState State);
	void RefreshResult(EBattleState State);
	void RefreshAll();

	// ---- 控件回调 ----
	UFUNCTION() void HandleTokenClicked(ABattleUnit* Unit);
	UFUNCTION() void HandleCardClicked(int32 Index, bool bIsMove);
	UFUNCTION() void HandleCardHover(UERSkillCard* Card, bool bHovered);
	UFUNCTION() void HandleEndTurnClicked();
	UFUNCTION() void HandleResultPrimary();
	UFUNCTION() void HandleTurnStarted(ABattleUnit* Unit);
	UFUNCTION() void HandleBattleFinished(bool bPlayerWon);

	ABattleGameMode* GetBattleGameMode() const;

	TWeakObjectPtr<UBattleManager> BattleManager;
	TWeakObjectPtr<ABattlePlayerController> PlayerController;

	// ---- 对象池 ----
	UPROPERTY(Transient) TArray<TObjectPtr<UERUnitToken>> InitiativeTokens;
	UPROPERTY(Transient) TArray<TObjectPtr<UERUnitToken>> SquadTokens;
	UPROPERTY(Transient) TArray<TObjectPtr<UERSkillCard>> Cards;

	// ---- Designer 基准状态 ----
	FVector2D BaseInitiativeTranslation = FVector2D::ZeroVector;
	FVector2D BaseHandTranslation = FVector2D::ZeroVector;
	FVector2D BaseSquadTranslation = FVector2D::ZeroVector;
	float BaseHeroOpacity = 1.0f;
	float BaseEndTurnOpacity = 1.0f;
	bool bBaseTransformsCached = false;

	// ---- 缓存比对 ----
	TWeakObjectPtr<ABattleUnit> CachedActiveUnit;
	int32 CachedQueueRev = -1;
	int32 CachedUnitsRev = -1;
	int32 CachedIntegritySum = INT32_MIN;
	float CachedAP = -1.0f;
	int32 CachedHP = -1;
	EPlayerInteractionMode CachedMode = EPlayerInteractionMode::Idle;
	EBattleState CachedState = EBattleState::Initializing;
	int32 CachedSkillSignature = -1;

	float NotifyTimer = 0.0f;
	float EntranceT = 0.0f;
	FERRevealTrack EntranceTrack;
	bool bLayoutBuilt = false;
};
