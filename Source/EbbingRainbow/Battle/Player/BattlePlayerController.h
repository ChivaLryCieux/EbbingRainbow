// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Battle/Core/BattleTypes.h"
#include "BattlePlayerController.generated.h"

class UBattleManager;
class ABattleUnit;

/** 回合制战斗玩家控制器: 支持鼠标点击 UI、自由移动路线预览、技能选定与自动镜头追踪 */
UCLASS()
class EBBINGRAINBOW_API ABattlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABattlePlayerController();

	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	// ---------- 交互模式控制 ----------

	UFUNCTION(BlueprintPure, Category = "Battle")
	EPlayerInteractionMode GetInteractionMode() const { return InteractionMode; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetInteractionMode(EPlayerInteractionMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ToggleMoveMode();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartSkillTargeting(int32 SkillIndex);

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetSelectedSkillIndex() const { return SelectedSkillIndex; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void CancelCurrentAction();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void RequestEndTurn();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void FocusOnActiveUnit();

	/** 摄像机平滑对准指定单位 (编队面板点击名片时调用); 传 nullptr 取消聚焦回到全景 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void FocusOnUnit(ABattleUnit* Unit);

	/** 当前被名牌聚焦的单位 ( nullptr 表示未聚焦 ) */
	UFUNCTION(BlueprintPure, Category = "Battle")
	ABattleUnit* GetFocusedUnit() const { return FocusedUnit.Get(); }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTurnStarted(ABattleUnit* Unit);

	/** 绑定回合开始委托, 并补做一次当前回合的聚焦 */
	void BindTurnFocus();

	/** 把待聚焦单位特写到摄像机 (Pawn 未附体时保留待办) */
	void ApplyAutoFocus();

	/** 是否已绑定 OnTurnStarted (避免重复 AddDynamic) */
	bool bTurnFocusBound = false;

	/** 回合切换时等待摄像机就位的聚焦目标 */
	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> PendingAutoFocusUnit = nullptr;

private:
	void OnLeftClick();
	void OnRightClick();
	void OnKey1();
	void OnKey2();
	void OnKeyM();
	void OnKeySpace();
	void OnKeyF();

	UBattleManager* GetBattleManager() const;
	bool RaycastUnderCursor(FHitResult& OutHit, ECollisionChannel Channel = ECC_Visibility) const;
	void UpdateHoveredUnit();
	void DrawMovePreview();
	void DrawSkillPreview();

	EPlayerInteractionMode InteractionMode = EPlayerInteractionMode::Idle;
	int32 SelectedSkillIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> CurrentHoveredUnit = nullptr;

	/** 编队面板当前聚焦的单位 */
	UPROPERTY(Transient)
	TObjectPtr<ABattleUnit> FocusedUnit = nullptr;
};
