// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"

/**
 * 战斗界面玻璃质感设计系统 (与 Hall 大厅的液态玻璃药丸一脉相承)。
 *
 * 设计原则 (苹果级质感):
 *  - 烟熏半透明玻璃底 + 白色发丝描边, 永不使用纯黑实底
 *  - 大圆角 (面板 22 / 卡片 18 / 芯片 12), 柔和不割裂
 *  - 所有状态切换走指数平滑插值, 禁止硬切
 *  - 悬停提亮、按压缩放、聚焦呼吸, 三层反馈逐级递进
 *  - 文字三级层次: 主 / 次 / 弱, 靠透明度而非灰色区分
 */
namespace BattleGlass
{
	// ---------------- 玻璃底色 ----------------

	/**
	 * 常规玻璃: 深夜烟熏。
	 * 注意 alpha: 竞技场地面曝光偏亮, 0.40 的稀释度会让白发丝文字对比度掉到 2:1,
	 * 因此压到 0.68 —— 仍能透出场景轮廓, 又保证 >= 4.5:1 的可读性。
	 */
	static const FLinearColor GlassFill(0.045f, 0.065f, 0.100f, 0.68f);
	/** 悬停提亮 */
	static const FLinearColor GlassFillHover(0.080f, 0.115f, 0.170f, 0.80f);
	/** 按下/激活再亮一档 */
	static const FLinearColor GlassFillActive(0.105f, 0.155f, 0.235f, 0.86f);
	/** 阵亡单位: 去饱和的灰玻璃 */
	static const FLinearColor GlassFillDead(0.055f, 0.060f, 0.070f, 0.58f);

	// ---------------- 描边 ----------------

	/** 白色发丝描边 (常态) */
	static const FLinearColor Hairline(0.90f, 0.95f, 1.00f, 0.20f);
	/** 悬停增强 */
	static const FLinearColor HairlineHover(0.90f, 0.95f, 1.00f, 0.50f);
	/** 顶部高光发丝线 (苹果式内顶光) */
	static const FLinearColor TopHighlight(1.00f, 1.00f, 1.00f, 0.10f);

	// ---------------- 文字 ----------------

	static const FLinearColor TextPrimary(0.96f, 0.98f, 1.00f, 0.96f);
	static const FLinearColor TextSecondary(0.78f, 0.85f, 0.95f, 0.58f);
	static const FLinearColor TextTertiary(0.70f, 0.78f, 0.90f, 0.36f);
	static const FLinearColor TextDead(0.55f, 0.58f, 0.62f, 0.45f);

	// ---------------- 主题色 ----------------

	/** 我方冰蓝 (与脚底光环底盘一致) */
	static const FLinearColor AccentPlayer(0.32f, 0.76f, 1.00f, 1.0f);
	/** 敌方赤红 */
	static const FLinearColor AccentEnemy(1.00f, 0.30f, 0.22f, 1.0f);
	/** 当前行动回合金 */
	static const FLinearColor AccentGold(1.00f, 0.82f, 0.28f, 1.0f);

	// ---------------- 生命条 ----------------

	static const FLinearColor HealthHigh(0.25f, 0.85f, 0.45f, 1.0f);
	static const FLinearColor HealthMid(0.98f, 0.76f, 0.24f, 1.0f);
	static const FLinearColor HealthLow(0.96f, 0.30f, 0.25f, 1.0f);
	static const FLinearColor HealthTrack(1.00f, 1.00f, 1.00f, 0.10f);
	static const FLinearColor BarrierFill(0.55f, 0.80f, 1.00f, 1.0f);

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
