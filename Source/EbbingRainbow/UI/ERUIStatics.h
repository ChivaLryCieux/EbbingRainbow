// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/ERUITheme.h"

class UERTechFrame;
class UOverlay;
class UTexture2D;
class UTextBlock;
class UUserWidget;
class UVerticalBox;
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

	/** 档位字体 (Linux Biolinum 拉丁/数字 + Noto 中文) */
	FSlateFontInfo Font(EERTypeScale Scale, bool bLatin = false);

	/** 显式字号/粗细/字距 (用于《控制:共振》大占比粗体与极小微标对比) */
	FSlateFontInfo FontBold(int32 Size, int32 Tracking = 0);
	FSlateFontInfo FontRegular(int32 Size, int32 Tracking = 0);

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

	// =====================================================================
	// 「白底墨字彩条」统一样式 (源自大厅「银河委托」主按钮, 全部按钮与栏目共用):
	//   反白石膏底 + 极墨黑粗体字 + 左侧 3~4px 窄光谱/点睛彩条 + 45° 切角 + 四角对准刻度。
	// 排印铁律: 同一控件内禁止中英文并存, 按钮与栏目只写中文, 中文字号加大。
	// =====================================================================

	/** 骨白底板上的墨字颜色 (Alpha 1 = 主文字, 0.62 = 次级, 0.42 = 微标) */
	FLinearColor InkText(float Alpha = 1.0f);

	/** 给 UERTechFrame 套用白底墨字彩条配色。
	 *  Hover01: 悬停强度; bEnabled=false 时整体褪为灰白; bEmphasis=true 时强化描边 (选中/紧急);
	 *  bPrism=true 用《虹不我与》三段光谱条, 否则用 Accent 单色条。 */
	void StyleBoneFrame(UERTechFrame* Frame, float Hover01, bool bEnabled, bool bEmphasis,
		const FLinearColor& Accent, bool bPrism);

	/** 骨白栏目卡: 底板 + 内容纵向容器。Content 里的文字请用 InkText 配色。 */
	struct FBoneCard
	{
		UOverlay* Root = nullptr;
		UERTechFrame* Frame = nullptr;
		UVerticalBox* Content = nullptr;
	};
	FBoneCard MakeBoneCard(UUserWidget* Owner, const FLinearColor& Accent, bool bPrism,
		const FMargin& Padding = FMargin(20.0f, 14.0f), bool bDrawBrackets = true);

	/** 栏目标题条 (章节抬头): 骨白底 + 墨字 + 左侧点睛条, 只含中文 */
	UWidget* MakeSectionBar(UUserWidget* Owner, const FText& Title, int32 FontSize = 20,
		const FLinearColor& Accent = FLinearColor(0.96f, 0.56f, 0.04f, 1.0f), bool bPrism = true);
}
