// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "ERUITheme.generated.h"

class AActor;
class UFont;
class UFontFace;
class UTexture2D;
class UUserWidget;

/**
 * 字号档位 —— 全局统一, 拒绝逐控件拍脑袋。
 */
UENUM(BlueprintType)
enum class EERTypeScale : uint8
{
	Micro    UMETA(DisplayName = "Micro (标签/快捷键/图例)"),
	Caption  UMETA(DisplayName = "Caption (次级说明)"),
	Body     UMETA(DisplayName = "Body (正文)"),
	Title    UMETA(DisplayName = "Title (卡牌费数/单位名)"),
	Display  UMETA(DisplayName = "Display (大数字 HP)"),
	Hero     UMETA(DisplayName = "Hero (结算词)")
};

/**
 * UI 设计令牌 (国际主义平面风):
 * 所有颜色/字号/间距/动效参数的唯一权威来源。
 * C++ 通过 UERUITheme::Get() 读取 (Settings 里配置的 DA, 缺省回退 CDO)。
 * 蓝图/WBP 里同样直接引用 DA 资产属性, 改一处全场景生效。
 */
UCLASS(BlueprintType)
class EBBINGRAINBOW_API UERUITheme : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---------------- 调色板 (黑 - 白 - 点睛彩) ----------------

	/** 面板黑曜石墨底 (#08090C 哑光黑, 高对比承载纯白排印) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor InkPanel = FLinearColor(0.008f, 0.009f, 0.012f, 0.92f);

	/** 次级抬升炭黑表面 (#12141A) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor ObsidianElevated = FLinearColor(0.018f, 0.020f, 0.026f, 0.94f);

	/** 纯墨黑 (用于反白卡片上的黑色文字) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor PureInk = FLinearColor(0.006f, 0.007f, 0.009f, 1.00f);

	/** 屏幕边缘渐压色 (深渊纯黑) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor InkScrim = FLinearColor(0.004f, 0.005f, 0.007f, 1.00f);

	/** 极纯白 / 石膏白 —— 主文字与反白模块 (#F5F5F2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Bone = FLinearColor(0.92f, 0.92f, 0.90f, 1.00f);

	/** 纯白二阶 (次级说明文字, 0.62) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BoneSecondary = FLinearColor(0.92f, 0.92f, 0.90f, 0.62f);

	/** 纯白三阶 (微标/遥测索引, 0.36) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BoneTertiary = FLinearColor(0.92f, 0.92f, 0.90f, 0.36f);

	/** 1px 发丝结构线 (0.16) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BoneGhost = FLinearColor(0.92f, 0.92f, 0.90f, 0.16f);

	/** 折射暖金 —— 全局主点睛色 (焦点/当前行动/主控确认) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Accent = FLinearColor(0.96f, 0.56f, 0.04f, 1.00f);

	/** 我方遥测冷青 (仅 2px 刻度与点睛使用) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor AllyMark = FLinearColor(0.05f, 0.56f, 0.94f, 1.00f);

	/** 敌方 / 控制朱红 (仅 2px 刻度与危险告警使用) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor EnemyMark = FLinearColor(0.84f, 0.05f, 0.03f, 1.00f);

	/** 低血量/高危朱红 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor DangerColor = FLinearColor(0.84f, 0.05f, 0.03f, 1.00f);

	/** 《虹不我与》招牌窄光谱条三段色 (冷青 -> 折射暖金 -> 控制朱红) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Prism")
	FLinearColor PrismStart = FLinearColor(0.04f, 0.62f, 0.96f, 1.00f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Prism")
	FLinearColor PrismMid = FLinearColor(0.96f, 0.56f, 0.04f, 1.00f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Prism")
	FLinearColor PrismEnd = FLinearColor(0.86f, 0.06f, 0.04f, 1.00f);

	/** 阵亡态着色 (整体乘算) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor DeadTint = FLinearColor(0.60f, 0.60f, 0.62f, 0.28f);

	// ---------------- 字体 (Linux Biolinum + Noto, 控制:共振式巨细对比) ----------------

	/** 正文中文字体 (Noto Sans SC Regular) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	TObjectPtr<UFontFace> FontBody = nullptr;

	/** 粗体中文字体 (Noto Sans SC Bold, 用于巨型标题与夸张层级对比) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	TObjectPtr<UFontFace> FontBodyBold = nullptr;

	/** 拉丁/数字字体 (Linux Biolinum Bold, 统一用于英文与数字) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	TObjectPtr<UFontFace> FontLatin = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 MicroSize = 9;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 CaptionSize = 11;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 BodySize = 13;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 TitleSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 DisplaySize = 54;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 HeroSize = 96;

	/** Micro/Caption 档位的字距 (Slate FontInfo LetterSpacing, 1/1000 em 量级) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
	int32 MicroTracking = 280;

	// ---------------- 度量 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float ScreenMargin = 48.0f;

	/** 先攻标记边长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float ChipSize = 34.0f;

	/** 编队标记边长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float SquadChipSize = 46.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	FVector2D CardSize = FVector2D(68.0f, 96.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float PipSize = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float PipGap = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float RuleThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics")
	float TokenGap = 10.0f;

	// ---------------- 动效 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float EntranceDuration = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float HoverInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float NotifyDuration = 3.5f;

	// ---------------- 持久化资产覆盖 (可选: 指向 Content/UI 资产供 WBP 与运行时共用) ----------------

	/** 预制复合字体资产 (如 /Game/UI/Fonts/F_ERUI_Composite); 为空时运行时自动按 FontLatin+FontBody 组装 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UFont> CompositeFontAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UTexture2D> WhiteTextureAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UTexture2D> RingTextureAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UTexture2D> ScrimTextureAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UTexture2D> DefaultPortraitAsset = nullptr;

	// ---------------- 工具 ----------------

	/** 档位字号。FontObject 始终是复合字体 UFont: Linux Biolinum 为默认 typeface,
	 *  Noto (Regular + Bold) 挂在 Fallback typeface, 中文字形自动回落。
	 *  Title/Display/Hero 自动选用 Bold 字重形成《控制:共振》式粗细巨幅对比。 */
	UFUNCTION(BlueprintPure, Category = "ERUI|Theme")
	FSlateFontInfo GetFont(EERTypeScale Scale, bool bLatin = false) const;

	/** 指定字重/字号/字距的快捷字体构造 */
	FSlateFontInfo GetFontExplicit(int32 Size, bool bBold = false, int32 Tracking = 0) const;

	/** 惰性构建的运行时复合字体 (UFont), FontFace 不能直接当 FontObject 用
	 *  (UFontFace 只有 IFontFaceInterface, 无 IFontProviderInterface) */
	const UFont* GetRuntimeFont() const;

	/** 当前主题: Settings 配置的 DA, 缺省回退 CDO 默认值 */
	UFUNCTION(BlueprintPure, Category = "ERUI|Theme", meta = (DisplayName = "Get ER UI Theme"))
	static const UERUITheme* Get();

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<UFont> RuntimeFont;
};

/**
 * UI 全局设置 (Project Settings -> Game -> ER UI):
 *  - Theme: DA_ERUITheme 资产路径 (config 序列化, 打包自动依赖)
 *  - HallHUDWidgetClass: 空间站大厅主界面 WBP
 *  - RouteHUDWidgetClass: 航线图主界面 WBP
 *  - BattleHUDWidgetClass: 战斗 HUD 的 WBP 类
 *  - BattleFloatingTextClass: 战场飘字 Actor 蓝图类
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER UI"))
class EBBINGRAINBOW_API UERUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TSoftObjectPtr<UERUITheme> Theme;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Screens")
	TSoftClassPtr<UUserWidget> HallHUDWidgetClass;

	/** 大厅「人员管理」面板 (WBP_PersonnelPanel); 为空时用 C++ 类 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Screens")
	TSoftClassPtr<UUserWidget> PersonnelPanelWidgetClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Screens")
	TSoftClassPtr<UUserWidget> RouteHUDWidgetClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Screens")
	TSoftClassPtr<UUserWidget> BattleHUDWidgetClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle FX")
	TSoftClassPtr<AActor> BattleFloatingTextClass;

	static const UERUISettings* Get() { return GetDefault<UERUISettings>(); }
};
