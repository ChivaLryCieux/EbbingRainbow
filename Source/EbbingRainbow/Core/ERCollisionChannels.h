// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"

/**
 * 战斗视线/掩体检测专用通道 (DefaultEngine.ini 注册名 "BattleLOS")。
 *
 * 约定:
 *  - 一切地形几何 (WFC 瓦片/体素块/自然岛/nav 代理) 按默认响应 Block;
 *  - 战斗单位在初始化时显式 SetCollisionResponseToChannel(ER_TRACE_BATTLELOS, Ignore)。
 * 这样视线射线天然不命中单位, 不需要每帧重建 "忽略所有单位" 列表。
 */
constexpr ECollisionChannel ER_TRACE_BATTLELOS = ECC_GameTraceChannel1;
