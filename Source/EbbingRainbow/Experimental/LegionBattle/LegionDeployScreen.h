// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Motion/ERUIMotion.h"
#include "LegionDeployScreen.generated.h"

class UERActionButton;
class UImage;
class UProgressBar;
class UTextBlock;
class IConsoleVariable;

/**
 * 出击部署加载屏 —— 「虹膜神经接驳」。
 *
 * 两种模式:
 *  - 接驳模式 (Lvl_Legion 内, SetupInteractive):
 *      进入关卡后立即铺满全屏, 封锁指挥输入但世界正常流转 (草皮/流送得以继续,
 *      游戏不暂停 —— Landscape 草的生成由 ULandscapeSubsystem 驱动, 暂停即冻结)。
 *      期间开启 PrioritizeGrassCreation (相机距离近处优先 + 4 倍创建配额) 加速植被构筑,
 *      轮询「渲染资产流送清零 + 可见泡 (~80m) 内植被实例数连续稳定」判定接驳完成 ——
 *      全图植被几十万实例永远长不完, 但草种 EndCull 最远 70m, 泡外草皮进场后随移动增量流送即可,
 *      随后激活「进入战场」按钮; 点击后战斗才真正开始 (GameMode 的 Elapsed/索敌/空降波次
 *      全部在 BeginBattle 之前冻结, 敌军不会趁加载白嫖机动)。
 *  - 过渡模式 (大厅出击点击后, SetupTransit):
 *      纯静态视觉, 在 OpenLevel 卡顿期间盖住画面, 与关卡内的接驳屏视觉连续。
 *
 * 自动化截图链路 (-LegionShot/-SkyShot/-MapShot) 与 -NoDeployScreen 不创建本屏。
 */
UCLASS()
class EBBINGRAINBOW_API ULegionDeployScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 军团战场接驳模式: 轮询就绪度, 就绪后激活「进入战场」 */
	void SetupInteractive();

	/** 大厅过渡模式: 静态画面, 只作视觉衔接 */
	void SetupTransit(const FText& InTitle, const FText& InSub);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Build();
	void PollReadiness();
	int32 CountGrassInstances(bool& bOutAnyLandscape) const;
	void HandleEnterClicked();
	void ApplyGrassBoost();
	void RestoreGrassBoost();
	void SetProgressTarget(float Target);

	bool bTransit = false;
	bool bBuilt = false;
	bool bDismissing = false;
	bool bReady = false;
	bool bReadyApplied = false;
	bool bStreamDone = false;
	bool bTimeoutLogged = false;

	/** 过渡模式文案缓存: SetupTransit 早于首次建树, 先存下来待 NativeConstruct 落地 */
	FText PendingTitle;
	FText PendingSub;

	float LiveTime = 0.0f;
	float PollAccum = 0.0f;
	float FadeAlpha = 1.0f;
	float ReadyAutoEnterTimer = 0.0f;
	float TelemetryTimer = 0.0f;
	int32 LastGrassInstances = -1;
	int32 StablePolls = 0;
	int32 PeakWantingResources = 0;
	int32 LastGrassSeen = 0;

	/** 接驳屏最短驻留 (秒): 即使一切就绪也保留展示节奏, 避免一闪而过 */
	float MinDwellSeconds = 1.4f;
	/** 兜底超时 (秒): 流送/植被迟迟不定论时强制放行, 防止软锁 */
	float ForceReadySeconds = 20.0f;

	FERFloatSpring ProgressSpring = FERFloatSpring::Silky(0.0f);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ProgressBar;
	UPROPERTY(Transient) TObjectPtr<UERActionButton> EnterButton;

	IConsoleVariable* CVarMaxCreate = nullptr;
	int32 SavedMaxCreate = 0;
	bool bGrassBoosted = false;
	bool bGrassPrioritized = false;
};
