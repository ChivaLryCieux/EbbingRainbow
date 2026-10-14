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

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTurnStarted(ABattleUnit* Unit);

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
};
