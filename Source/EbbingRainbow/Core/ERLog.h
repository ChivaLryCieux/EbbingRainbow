// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EbbingRainbow.h"

/**
 * 工程统一日志分类。
 *
 * 历史上 150+ 处 UE_LOG(LogTemp, ...) 的归处: 新日志一律走这些分类,
 * 便于按子系统过滤 (`Log LogERBattle Verbose` / `Log LogERWFC Log`)。
 * LogEbbingRainbow (见 EbbingRainbow.h) 仍是兜底通用分类。
 */
DECLARE_LOG_CATEGORY_EXTERN(LogERBattle,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogERBattleUI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogERWFC,      Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogERRun,      Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogERHall,     Log, All);
