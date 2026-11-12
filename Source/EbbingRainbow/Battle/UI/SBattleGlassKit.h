// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"

/**
 * 战斗界面玻璃质感设计系统 —— 白色半透明圆角矩形风。
 *
 * 设计原则:
 *  - 白色半透明底 + 冷灰发丝描边, 场景仍可从面板后透出轮廓
 *  - 大圆角 (面板 22 / 卡片 18 / 芯片 12), 柔和不割裂
 *  - 白底上文字改深石板色三阶: 主 / 次 / 弱, 靠透明度而非灰色区分
 *  - 所有状态切换走指数平滑插值, 禁止硬切
 *  - 悬停提亮、按压缩放、聚焦呼吸, 三层反馈逐级递进
 */
namespace BattleGlass
{
	// ---------------- 玻璃底色 (白色半透明) ----------------

	/**
	 * 常规玻璃: 乳白磨砂。
	 * alpha 0.60: 竞技场地面偏亮, 白底 0.60 既能透出场景层次,
	 * 又让深石板文字保持 >= 4.5:1 的可读对比。
	 */
	static const FLinearColor GlassFill(0.96f, 0.97f, 1.00f, 0.60f);
	/** 悬停提亮: 更实的白 */
	static const FLinearColor GlassFillHover(1.00f, 1.00f, 1.00f, 0.74f);
	/** 按下/激活再实一档 */
	static const FLinearColor GlassFillActive(1.00f, 1.00f, 1.00f, 0.84f);
	/** 阵亡单位: 去饱和的灰白 */
	static const FLinearColor GlassFillDead(0.80f, 0.82f, 0.86f, 0.50f);

	// ---------------- 描边 ----------------

	/** 冷灰发丝描边 (常态) —— 白底上白色描边不可见, 用低饱和冷灰勾边 */
	static const FLinearColor Hairline(0.40f, 0.46f, 0.56f, 0.42f);
	/** 悬停增强 */
	static const FLinearColor HairlineHover(0.34f, 0.40f, 0.52f, 0.72f);
	/** 顶部高光发丝线 (内顶光, 让面板有厚度) */
	static const FLinearColor TopHighlight(1.00f, 1.00f, 1.00f, 0.60f);

	// ---------------- 文字 (深石板三阶) ----------------

	static const FLinearColor TextPrimary(0.10f, 0.13f, 0.19f, 0.96f);
	static const FLinearColor TextSecondary(0.24f, 0.30f, 0.40f, 0.78f);
	static const FLinearColor TextTertiary(0.34f, 0.40f, 0.50f, 0.55f);
	static const FLinearColor TextDead(0.48f, 0.52f, 0.58f, 0.60f);

	// ---------------- 主题色 (白底降明度保对比) ----------------

	/** 我方深蓝 (与脚底光环底盘一致) */
	static const FLinearColor AccentPlayer(0.08f, 0.44f, 0.88f, 1.0f);
	/** 敌方深红 */
	static const FLinearColor AccentEnemy(0.86f, 0.20f, 0.12f, 1.0f);
	/** 当前行动回合暗金 */
	static const FLinearColor AccentGold(0.80f, 0.55f, 0.05f, 1.0f);

	// ---------------- 生命条 ----------------

	static const FLinearColor HealthHigh(0.14f, 0.62f, 0.30f, 1.0f);
	static const FLinearColor HealthMid(0.88f, 0.55f, 0.06f, 1.0f);
	static const FLinearColor HealthLow(0.86f, 0.20f, 0.14f, 1.0f);
	static const FLinearColor HealthTrack(0.12f, 0.16f, 0.24f, 0.16f);
	static const FLinearColor BarrierFill(0.20f, 0.55f, 0.95f, 1.0f);

	// ---------------- 圆角 ----------------

	static constexpr float RadiusPanel = 22.0f;
	static constexpr float RadiusCard = 18.0f;
	static constexpr float RadiusChip = 12.0f;
	static constexpr float RadiusPill = 14.0f;
	static constexpr float RadiusAvatar = 24.0f;

	// ---------------- 动效参数 ----------------

	/** 悬停插值速度 (指数平滑, 越大越跟手) */
	static constexpr float HoverInterpSpeed = 14.0f;
	/** 按压缩放插值速度 */
	static constexpr float PressInterpSpeed = 22.0f;
	/** 入场动画时长 (秒) */
	static constexpr float EntranceDuration = 0.85f;

	/** 把画刷配置成圆角矩形玻璃面 (底色 + 发丝描边) */
	inline void ConfigureGlassBrush(FSlateBrush& Brush, float CornerRadius,
		FLinearColor Fill, float OutlineWidth, FLinearColor OutlineColor)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
	}

	/**
	 * 白底可读性: 彩色按 0.72 亮度压深 (保色相, 用于描边/小字这类
	 * 直接铺在白色半透明底上的元素)。
	 */
	inline FLinearColor DeepenForWhite(FLinearColor Color, float Scale = 0.72f)
	{
		return FLinearColor(Color.R * Scale, Color.G * Scale, Color.B * Scale, Color.A);
	}

	/** 血量对应的颜色 (绿 → 黄 → 红) */
	inline FLinearColor HealthColorFor(float Percent)
	{
		if (Percent > 0.55f) { return HealthHigh; }
		if (Percent > 0.25f) { return HealthMid; }
		return HealthLow;
	}

	/** 阵营主题色 */
	inline FLinearColor TeamAccent(int32 Team)
	{
		return Team == 0 ? AccentPlayer : AccentEnemy;
	}

	/** ease-out cubic: 入场动画专用, 起步快收尾柔 */
	inline float EaseOutCubic(float T)
	{
		const float C = 1.0f - FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - C * C * C;
	}
}
