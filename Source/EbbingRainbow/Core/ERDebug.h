// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 统一命令行调试开关。
 *
 * 原先 FParse::Param(FCommandLine::Get(), "...") 散落在生成器/GameMode/
 * 竞技场里 (10+ 处, 每次调用都重新解析整条命令行)。这里一次性解析并缓存;
 * CVar (ER.*) 仍留在各子系统本地。
 *
 * 纪律: 正式玩法不得依赖这些开关; Shipping 下调用点建议 #if !UE_BUILD_SHIPPING。
 */
namespace ER::Debug
{
	/** -Atmo: 生成器接管关卡氛围 (种子化三点光+虚空雾; 默认关) */
	bool IsAtmoEnabled();

	/** -ArenaSeed=N: 固定竞技场种子 (未指定返回 0) */
	int32 GetArenaSeed();

	/** -ArenaGenerator=N: 强制生成器覆盖 (未指定返回 -1) */
	int32 GetArenaGeneratorOverride();

	/** -ArenaForceReady: 跳过 nav 校验直接就绪 (仅截图) */
	bool IsArenaForceReady();

	/** -AutoShot / -AutoShotTop: 竞技场就绪后自动截图 (正俯机位) */
	bool IsAutoShot();
	bool IsAutoShotTop();

	/** -ShowNav: 截图时显示导航 */
	bool IsShowNav();

	/** -RouteShot: 航线图 3D 场景搭好后自动截图 */
	bool IsRouteShot();

	/** 自然岛诊断: -NatureCube / -NatureNavBox / -NatureFlat */
	bool IsNatureCube();
	bool IsNatureNavBox();
	bool IsNatureFlat();

	/** -HallShowcase: 大厅布局陈列模式 */
	bool IsHallShowcase();

	/** -ArchDump: 旧 Atrium ASCII 平面图输出 (Legacy) */
	bool IsArchDump();
}
