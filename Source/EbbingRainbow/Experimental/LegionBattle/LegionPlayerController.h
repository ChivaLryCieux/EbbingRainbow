// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Experimental/LegionBattle/LegionGameMode.h"
#include "Experimental/LegionBattle/LegionTypes.h"
#include "LegionPlayerController.generated.h"

class ALegionUnit;

/**
 * 等高线地图视角的视图状态: 屏幕坐标 = 视口中心 + (UV - CenterUV) * Side。
 * Side = min(视口宽, 视口高) * Zoom; Zoom=1 时整张图刚好内接于视口。
 */
struct FLegionMapView
{
	static constexpr float MinZoom = 1.0f;
	static constexpr float MaxZoom = 5.0f;

	float Zoom = 1.0f;
	FVector2D CenterUV = FVector2D(0.5, 0.5);

	float Side(int32 SX, int32 SY) const { return (float)FMath::Min(SX, SY) * Zoom; }

	FVector2D UVToScreen(const FVector2D& UV, int32 SX, int32 SY) const
	{
		return FVector2D(SX * 0.5, SY * 0.5) + (UV - CenterUV) * (double)Side(SX, SY);
	}

	FVector2D ScreenToUV(const FVector2D& Screen, int32 SX, int32 SY) const
	{
		return CenterUV + (Screen - FVector2D(SX * 0.5, SY * 0.5)) / (double)FMath::Max(1.0f, Side(SX, SY));
	}

	/** 约束缩放范围, 并保证视口不超出地图 (地图小于视口的轴向居中) */
	void Clamp(int32 SX, int32 SY)
	{
		Zoom = FMath::Clamp(Zoom, MinZoom, MaxZoom);
		const double S = FMath::Max(1.0f, Side(SX, SY));
		const double HalfU = (SX * 0.5) / S;
		const double HalfV = (SY * 0.5) / S;
		CenterUV.X = HalfU >= 0.5 ? 0.5 : FMath::Clamp(CenterUV.X, HalfU, 1.0 - HalfU);
		CenterUV.Y = HalfV >= 0.5 ? 0.5 : FMath::Clamp(CenterUV.Y, HalfV, 1.0 - HalfV);
	}
};

/**
 * 虹膜军团战术控制器 (3C: 第一人称指挥官 + 等高线地图双视角):
 *
 * 【战场视角 (默认)】 第一人称步行:
 *  - WASD 移动, 鼠标转向, Shift 疾跑;
 *  - 准星瞄兵团 左键 点选 (Ctrl 增减), 右键 点地面机动 / 点敌团集火, 右键按住转动视角拉出阵型朝向;
 *  - 1..8 快选兵团, Z 主动协议, X 停驻, G 全军突击, F 令选中兵团(无选中则全军)跟随/取消跟随;
 *  - 按住 Alt 呼出光标 (点左上小地图 / 底部兵团牌 / 按钮); M 或点小地图进入等高线地图。
 *
 * 【等高线地图视角】 战场自动暂停, 地图全屏铺开:
 *  - 滚轮以鼠标为中心缩放, 左键拖拽 / 中键拖拽 平移;
 *  - 左键点我方番号选中 (Ctrl/Shift 增减), 再点敌方番号集火, 点空地机动, 右键同样下令;
 *  - 顶部「返回战场」按钮 (或 M / Esc) 退出并恢复原时间流速。
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

	// ---- 视角状态 ----
	bool IsStrategicMapOpen() const { return bMapOpen; }
	void SetStrategicMapOpen(bool bOpen);
	const FLegionMapView& GetMapView() const { return MapView; }
	bool IsLegionFollowing(const ALegionUnit* Legion) const;

	/** 以屏幕点为锚缩放地图 (滚轮与自检共用) */
	void ZoomMapAt(int32 Dir, const FVector2D& ScreenPos);

	/** 控制台 / 自动截图链路调用: 在地图视角里模拟缩放、点番号、集火、机动、点返回, 日志输出 PASS/FAIL */
	UFUNCTION(Exec)
	void ERMapSelfTest();

	/** 自检主体; bCloseAtEnd=false 时保持地图打开 (便于截图), 之后调用 FinishMapSelfTest */
	void RunMapSelfTest(bool bCloseAtEnd);
	void FinishMapSelfTest();

	/** 战场视角下准星当前指向的兵团 (用于准星变色) */
	ALegionUnit* GetAimedLegion() const { return HoveredLegion.Get(); }

	// ---- HUD 绘制用实时交互状态 ----
	bool IsMarqueeActive() const { return false; }
	void GetMarqueeScreenRect(FVector2D& OutMin, FVector2D& OutMax) const { OutMin = OutMax = FVector2D::ZeroVector; }

	bool IsRightDragActive() const { return bRightDragging && SelectedLegions.Num() > 0; }
	void GetRightDragWorld(FVector& OutStart, FVector& OutEnd) const
	{
		OutStart = RightDragStartWorld;
		OutEnd = RightDragCurrentWorld;
	}

	/** 计算当前选中兵团在目标点与朝向下的编队预览中心列表 */
	void ComputeFormationSlots(const FVector& Center, const FVector& FacingDir, TArray<FVector>& OutSlots) const;

	/** 鼠标灵敏度 (度/像素) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Input")
	float LookSensitivity = 0.11f;

	/** 反转鼠标 Y 轴 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Input")
	bool bInvertLookY = false;

	/** 跟随指令: 兵团列队于指挥官身后的距离 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Input")
	float FollowDistance = 2000.0f;

	/** 地图滚轮每格缩放倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legion|Input")
	float MapZoomStep = 1.2f;

protected:
	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnRightReleased();
	void OnMidPressed();
	void OnMidReleased() { bMidDown = false; }
	void OnWheelUp() { OnMapWheel(+1); }
	void OnWheelDown() { OnMapWheel(-1); }

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
	void OnKeyF();
	void OnKeyTab();
	void OnKeyPause();
	void OnJumpPressed();
	void OnJumpReleased();
	void OnKeyRestart();
	void OnEscape();
	void OnKeyMapToggle() { SetStrategicMapOpen(!bMapOpen); }

	void ClearSelection();
	void SetSingleSelection(ALegionUnit* Legion);
	void ToggleSelection(ALegionUnit* Legion);
	void PruneInvalidSelections();

	void OrderAttack(ALegionUnit* Target);
	void OrderFormationMove(const FVector& Center, const FVector& FacingDir, bool bUseCustomFacing);

	/** 战场视角: 光标可见时取光标下兵团, 否则取准星下兵团 */
	ALegionUnit* PickLegion(FHitResult& OutHit) const;
	/** 战场视角: 光标可见时取光标下地面, 否则取准星下地面 */
	bool TraceGroundAim(FVector& OutWorld) const;

	// ---- 地图视角 ----
	void OnMapWheel(int32 Dir);
	void TickMap();
	void HandleMapClick(const FVector2D& ScreenPos, bool bRight);
	void ResetMapView();
	void ApplyInputMode();

	// ---- 战场视角 ----
	void TickGround(float DeltaTime);
	void TickFollow(float DeltaTime);
	void ComputeSlotsFor(const TArray<ALegionUnit*>& Legions, const FVector& Center, const FVector& FacingDir, TArray<FVector>& OutSlots) const;
	void StopFollow(const ALegionUnit* Legion);

private:
	TArray<TWeakObjectPtr<ALegionUnit>> SelectedLegions;
	TArray<TWeakObjectPtr<ALegionUnit>> FollowLegions;
	TWeakObjectPtr<ALegionUnit> HoveredLegion;

	bool bMapOpen = false;
	bool bAltCursor = false;
	FLegionMapView MapView;
	ELegionTimeMode PrevTimeMode = ELegionTimeMode::Normal;

	bool bLeftDown = false;
	bool bMapDragged = false;
	bool bMidDown = false;
	FVector2D LeftDragStartScreen = FVector2D::ZeroVector;
	FVector2D MapLastMouse = FVector2D::ZeroVector;

	bool bRightDragging = false;
	bool bAutoSlowMoByDrag = false;
	FVector RightDragStartWorld = FVector::ZeroVector;
	FVector RightDragCurrentWorld = FVector::ZeroVector;

	float FollowTimer = 0.0f;

	// 自检状态
	TWeakObjectPtr<ALegionUnit> TestMover;
	FVector TestMoverStart = FVector::ZeroVector;
	int32 TestFails = 0;
};
