// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "GameFramework/HUD.h"
#include "LegionHUD.generated.h"

class ALegionGameMode;
class ALegionPlayerController;
class ALegionUnit;
class UERGalaCommandPanel;
class ULegionHUDWidget;
class ULegionPauseScreen;
class UTexture2D;

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

	/** 等高线地图视角顶部「返回战场」按钮矩形 (视口像素) */
	static FBox2D GetMapReturnButtonRect(int32 ViewportSizeX, int32 ViewportSizeY);

	/** 等高线地图番号牌命中测试 (命中框在 DrawStrategicMap 中按真实文本尺寸生成) */
	ALegionUnit* PickStrategicBadge(const FVector2D& ScreenPos) const;

	/** 取某兵团番号牌的屏幕中心 (上一帧布局结果) */
	bool GetStrategicBadgeCenter(const ALegionUnit* Unit, FVector2D& OutCenter) const;

	/** 第一人称战场视角: 命中测试抬头瞄准的敌军天空部队 Logo (命中框由 DrawHUD 实时生成) */
	ALegionUnit* PickEnemySkyBanner(const FVector2D& ScreenPos) const;

	/** 取某敌军天空 Logo 的屏幕中心 (上一帧投影结果) */
	bool GetEnemySkyBannerScreenCenter(const ALegionUnit* Unit, FVector2D& OutCenter) const;

	/** 取某敌军天空 Logo 的绘制底边 Y (供准星提示避让, 防止文字重叠) */
	bool GetEnemySkyBannerBottomY(const ALegionUnit* Unit, float& OutBottomY) const;

	/** 同步等高线地图视角状态到 UMG 控件 (地图视角下隐藏底部四张部队卡) */
	void NotifyStrategicMapOpen(bool bMapOpen);

	/** Esc 暂停界面与设置弹窗生命周期 */
	void SetPauseMenuVisible(bool bVisible);
	void OpenPauseSettings();
	void ClosePauseSettings();
	bool IsPauseMenuVisible() const;
	bool IsPauseSettingsOpen() const;
	ULegionPauseScreen* GetPauseScreen() const { return PauseScreen.Get(); }

	ULegionHUDWidget* GetHUDWidget() const { return Widget.Get(); }

private:
	UFUNCTION()
	void HandlePauseSettingsClosed();
	void LoadPlaceholderLogos();
	UTexture2D* GetClassLogoTexture(ELegionClass Archetype, bool bAlly) const;
	void DrawMinimap(ALegionGameMode* GM, ALegionPlayerController* PC);
	void DrawStrategicMap(ALegionGameMode* GM, ALegionPlayerController* PC);
	void DrawCrosshair(ALegionPlayerController* PC);
	/** Canvas 版「白底墨字彩条」底板: 反白石膏 + 墨黑描边 + 左侧点睛/光谱条 */
	void DrawBonePlate(float X, float Y, float W, float H, const FLinearColor& Accent,
		bool bPrism, bool bEmphasis, bool bHover);
	/** 《控制:共振》式高对比纯字 + 黑色错位阴影 (Canvas 绘制) */
	void DrawResonantText(const FString& Text, const FLinearColor& Color, float X, float Y,
		UFont* Font, float Scale, FVector2D ShadowOffset = FVector2D(1.8f, 2.2f), float ShadowAlpha = 0.88f);
	void DrawOrderOverlays(ALegionGameMode* GM, ALegionPlayerController* PC);
	void DrawUnitBadge(ALegionUnit* Unit, bool bSelected);
	void DrawEnemySkyBanner(ALegionUnit* Unit, ALegionPlayerController* PC, bool bHovered, bool bFocusedTarget);
	void DrawProjectedLine(const FVector& A, const FVector& B, const FLinearColor& Color, float Thickness);
	/** 带相机近平面裁剪的 3D 线段投影: 抬头看天时地面起点在相机后方仍能完整画出通天竖线 */
	void DrawClippedWorldLine(const FVector& A, const FVector& B, const FLinearColor& Color, float Thickness);
	void DrawDropZoneWarning(const FVector& WorldPos, const FString& Label, float RemainingSec);

	struct FStrategicBadgeRect
	{
		TWeakObjectPtr<ALegionUnit> Unit;
		FBox2D Rect;
	};
	TArray<FStrategicBadgeRect> StrategicBadgeRects;

	struct FEnemySkyBannerEntry
	{
		TWeakObjectPtr<ALegionUnit> Unit;
		FBox2D HitRect;
		FVector2D Center = FVector2D::ZeroVector;
		float DistSq = 0.0f;
	};
	TArray<FEnemySkyBannerEntry> EnemySkyBannerEntries;

	UPROPERTY(Transient)
	TMap<ELegionClass, TObjectPtr<UTexture2D>> AllyClassLogos;

	UPROPERTY(Transient)
	TMap<ELegionClass, TObjectPtr<UTexture2D>> EnemyClassLogos;

	UPROPERTY(Transient)
	TObjectPtr<ULegionHUDWidget> Widget;

	UPROPERTY(Transient)
	TObjectPtr<ULegionPauseScreen> PauseScreen;

	UPROPERTY(Transient)
	TObjectPtr<UERGalaCommandPanel> SettingsPanel;
};
