// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Experimental/LegionBattle/LegionTypes.h"
#include "LegionPlayerController.generated.h"

class ALegionUnit;

/**
 * 虹膜军团战术控制器:
 *  - 左键点击 / 拖拽框选: 多选我方兵团 (支持 Shift 增减);
 *  - 右键点击敌团: 集火进攻; 右键点击地面: 整队横列机动;
 *  - 右键按住拖拽: 自动触发战术时缓并拉出目标阵型与驻守朝向箭头;
 *  - 1..8: 快选并聚焦兵团; Z: 释放选中兵团主动协议; X: 全停;
 *  - Tab: 切换常速/0.2x时缓; Space: 战术暂停; G: 全军突击; Esc: 取消选中; R: 重开。
 */
UCLASS()
class EBBINGRAINBOW_API ALegionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALegionPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Legion")
	ALegionUnit* GetSelectedLegion() const;

	const TArray<TWeakObjectPtr<ALegionUnit>>& GetSelectedLegions() const { return SelectedLegions; }
	bool IsLegionSelected(const ALegionUnit* Legion) const;

	void SelectLegionByIndex(int32 Index);
	void TriggerSelectedSkills();

	// ---- HUD 绘制用实时交互状态 ----
	bool IsMarqueeActive() const { return bMarqueeActive; }
	void GetMarqueeScreenRect(FVector2D& OutMin, FVector2D& OutMax) const;

	bool IsRightDragActive() const { return bRightDragging && SelectedLegions.Num() > 0; }
	void GetRightDragWorld(FVector& OutStart, FVector& OutEnd) const
	{
		OutStart = RightDragStartWorld;
		OutEnd = RightDragCurrentWorld;
	}

	/** 计算当前选中兵团在目标点与朝向下的编队预览中心列表 */
	void ComputeFormationSlots(const FVector& Center, const FVector& FacingDir, TArray<FVector>& OutSlots) const;

protected:
	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnRightReleased();

	void OnKeySelect(int32 Index);
	void OnKey1() { OnKeySelect(0); }
	void OnKey2() { OnKeySelect(1); }
	void OnKey3() { OnKeySelect(2); }
	void OnKey4() { OnKeySelect(3); }
	void OnKey5() { OnKeySelect(4); }
	void OnKey6() { OnKeySelect(5); }
	void OnKey7() { OnKeySelect(6); }
	void OnKey8() { OnKeySelect(7); }
	void OnKeyZ() { TriggerSelectedSkills(); }
	void OnKeyX();
	void OnKeyG();
	void OnKeyTab();
	void OnKeySpace();
	void OnKeyRestart();
	void OnEscape();

	void ClearSelection();
	void SetSingleSelection(ALegionUnit* Legion);
	void ToggleSelection(ALegionUnit* Legion);
	void PruneInvalidSelections();

	void OrderAttack(ALegionUnit* Target);
	void OrderFormationMove(const FVector& Center, const FVector& FacingDir, bool bUseCustomFacing);

	ALegionUnit* PickLegion(FHitResult& OutHit) const;
	bool TraceGroundUnderCursor(FVector& OutWorld) const;

private:
	TArray<TWeakObjectPtr<ALegionUnit>> SelectedLegions;
	TWeakObjectPtr<ALegionUnit> HoveredLegion;

	bool bLeftDown = false;
	bool bMarqueeActive = false;
	FVector2D LeftDragStartScreen = FVector2D::ZeroVector;
	FVector2D LeftDragCurScreen = FVector2D::ZeroVector;

	bool bRightDragging = false;
	bool bAutoSlowMoByDrag = false;
	FVector RightDragStartWorld = FVector::ZeroVector;
	FVector RightDragCurrentWorld = FVector::ZeroVector;
};
