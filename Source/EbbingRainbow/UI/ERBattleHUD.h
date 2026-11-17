// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/Core/BattleTypes.h"
#include "UI/ERBattleResult.h"
#include "ERBattleHUD.generated.h"

class ABattleGameMode;
class ABattlePlayerController;
class ABattleUnit;
class UBattleManager;
class UCanvasPanel;
class UERActionButton;
class UERHeroPlate;
class UERSkillCard;
class UERUnitToken;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UVerticalBox;

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
 * 布局全部程序化构建, 样式只读 UERUITheme::Get();
 * WBP 子类可整体替换本类 (Project Settings -> ER UI)。
 */
UCLASS()
class EBBINGRAINBOW_API UERBattleHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeBattleUI(UBattleManager* InManager, ABattlePlayerController* InPC);

	/** 战报通知 (由 ABattleHUD::AddBattleLog 转发) */
	void PostNotification(const FString& Message);

	/** 鼠标是否在任何交互控件上 (防点地穿透) */
	bool IsMouseOverInteractiveUI() const;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();

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

	// ---- 控件树 (BindWidgetOptional: WBP 提供则用 WBP 的, 否则 C++ 建默认布局) ----
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> InitiativeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> InitiativeRule;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> SquadBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERHeroPlate> HeroPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> HandBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERActionButton> EndTurnButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NotifyText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TopScrim;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> BottomScrim;

	// 悬停详情面板 (少文字: 只名字+元信息一行)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TooltipRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TooltipBg;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TooltipTitle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TooltipMeta;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UERBattleResult> ResultPanel;

	// ---- 池化控件类 (可在 WBP/DA 层换蓝图子类换皮) ----
	UPROPERTY(EditDefaultsOnly, Category = "ERUI|Classes")
	TSubclassOf<UERUnitToken> InitiativeTokenClass;
	UPROPERTY(EditDefaultsOnly, Category = "ERUI|Classes")
	TSubclassOf<UERUnitToken> SquadTokenClass;
	UPROPERTY(EditDefaultsOnly, Category = "ERUI|Classes")
	TSubclassOf<UERSkillCard> SkillCardClass;

	// ---- 对象池 ----
	UPROPERTY(Transient) TArray<TObjectPtr<UERUnitToken>> InitiativeTokens;
	UPROPERTY(Transient) TArray<TObjectPtr<UERUnitToken>> SquadTokens;
	UPROPERTY(Transient) TArray<TObjectPtr<UERSkillCard>> Cards;

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
	bool bLayoutBuilt = false;
};
