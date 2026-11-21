// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LegionHUD.generated.h"

class ALegionGameMode;
class ALegionPlayerController;
class ALegionUnit;
class ULegionHUDWidget;

/**
 * 虹膜军团战场 HUD:
 *  - 承载 ULegionHUDWidget 战略终端界面;
 *  - 通过 Canvas::DrawHUD() 绘制黑白等高线小地图 (蓝/红点标注敌我位置)、
 *    世界空间投影旗标、弹道轨迹、增援预警、框选框与阵型拖拽箭头。
 */
UCLASS()
class EBBINGRAINBOW_API ALegionHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

	/** 计算当前视口像素坐标系下的小地图矩形 */
	static void GetMinimapScreenRect(int32 ViewportSizeY, float& OutX, float& OutY, float& OutSize);

private:
	void DrawMinimap(ALegionGameMode* GM, ALegionPlayerController* PC);
	void DrawUnitBadge(ALegionUnit* Unit, bool bSelected);
	void DrawProjectedLine(const FVector& A, const FVector& B, const FLinearColor& Color, float Thickness);
	void DrawDropZoneWarning(const FVector& WorldPos, const FString& Label, float RemainingSec);

	UPROPERTY(Transient)
	TObjectPtr<ULegionHUDWidget> Widget;
};
