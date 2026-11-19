// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/ERUITheme.h"

class UTexture2D;
class UTextBlock;
class UUserWidget;
class UWidget;

/**
 * 平面风 UI 公共工具: 字体/画刷/运行时纹理/文本块工厂。
 * 纹理全部运行时程序化生成 (发丝环/压边渐变/纯白), 零资产依赖;
 * 想换成美术贴图时直接在 DA_ERUITheme 的扩展位指认即可。
 */
namespace ERUI
{
	/** 当前主题 (UERUITheme::Get 的简写) */
	const UERUITheme* Theme();

	/** 档位字体 (bLatin: 纯数字/拉丁串用 Inter) */
	FSlateFontInfo Font(EERTypeScale Scale, bool bLatin = false);

	/** 乘算透明度 */
	inline FLinearColor Fade(FLinearColor C, float AlphaMul)
	{
		C.A *= AlphaMul;
		return C;
	}

	// ---- 运行时纹理 (AddToRoot, 进程级单例) ----
	UTexture2D* GetWhiteTexture();     // 4x4 纯白: 进度条/发丝线填充源
	UTexture2D* GetRingTexture();      // 9x9 1px 发丝环: ghost 按钮/框体九切片
	UTexture2D* GetScrimTexture();     // 4x64 垂直渐压 (上实下透)

	/** 全透明画刷 (隐形按钮用) */
	FSlateBrush TransparentBrush();

	/** 发丝框九切片画刷 (1px 线, Tint 控制颜色) */
	FSlateBrush HairlineBoxBrush(FLinearColor Tint);

	/** 纯白色块画刷 (用于 Image, 颜色走 SetColorAndOpacity) */
	FSlateBrush WhiteBrush();

	/** 指定期望尺寸的纯白色块 (细线/方块)。
	 * 注意 UImage::SetDesiredSizeOverride 在 SImage 实例化前调用会被丢弃,
	 * 建树期一律用本重载把尺寸写进 Brush.ImageSize。 */
	FSlateBrush WhiteBrush(FVector2D Size);

	/** 渐压画刷 */
	FSlateBrush ScrimBrush();

	/** 圆角玻璃底画刷 (面板/胶囊/芯片共用同一套描边语义) */
	FSlateBrush RoundedBrush(float Radius, FLinearColor Fill,
		float OutlineWidth = 0.0f, FLinearColor Outline = FLinearColor::Transparent);

	/** 建一个已调好字体/颜色的文本块 (默认 SelfHitTestInvisible) */
	UTextBlock* MakeText(UUserWidget* Owner, const FText& Text, EERTypeScale Scale,
		FLinearColor Color, bool bLatin = false);
}
