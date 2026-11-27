// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformTime.h"

// 骨骼兵性能标尺的轻量手工计时: 仅累积毫秒数, 由 -LegionPerf 采样结束时统一取平均并清零。
// 开销为每次作用域两次 QPC 读取 (数十 ns), 只在军团 Tick 路径每帧每团出现几次, 可忽略。
namespace LegionPerf
{
	struct FAccum
	{
		double MicroAIMs = 0.0;
		double SkeletalRepMs = 0.0;
		double BulletsMs = 0.0;
		double UnitTickMs = 0.0;
		double GameModeTickMs = 0.0;
		double DirectiveMs = 0.0;
		double TerrainConformMs = 0.0;
		double DeathLoopMs = 0.0;
		double ActorMoveMs = 0.0;
		int32 Frames = 0;
	};

	inline FAccum& Get()
	{
		static FAccum A;
		return A;
	}

	inline void Reset()
	{
		Get() = FAccum();
	}

	struct FScope
	{
		double& Target;
		double Start;
		explicit FScope(double& InTarget) : Target(InTarget), Start(FPlatformTime::Seconds()) {}
		~FScope() { Target += (FPlatformTime::Seconds() - Start) * 1000.0; }
	};
}
