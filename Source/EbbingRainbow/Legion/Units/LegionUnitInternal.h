// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimationAsset.h"
#include "Engine/StaticMesh.h"

/** LegionUnit 各实现文件共享的内部数学/资产工具；不是跨模块公共 API。 */
namespace LegionUnitInternal
{
	/** 确定性 2D Perlin 梯度噪声 ([-1, 1], 用于自然漫步与蛇行微偏移) */
	inline float SmoothFade(float T) { return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f); }

	inline float Grad2D(int32 IX, int32 IY, float FX, float FY)
	{
		uint32 H = (uint32)(IX * 374761393u + IY * 668265263u);
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		const float Angle = (float)(H & 0xFFFF) * (6.2831853f / 65535.0f);
		return FMath::Cos(Angle) * FX + FMath::Sin(Angle) * FY;
	}

	inline float Noise2D(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float FX = X - (float)X0;
		const float FY = Y - (float)Y0;
		const float U = SmoothFade(FX);
		const float V = SmoothFade(FY);
		const float N00 = Grad2D(X0,     Y0,     FX,        FY);
		const float N10 = Grad2D(X0 + 1, Y0,     FX - 1.0f, FY);
		const float N01 = Grad2D(X0,     Y0 + 1, FX,        FY - 1.0f);
		const float N11 = Grad2D(X0 + 1, Y0 + 1, FX - 1.0f, FY - 1.0f);
		return FMath::Lerp(FMath::Lerp(N00, N10, U), FMath::Lerp(N01, N11, U), V);
	}

	/** 计算 2D 连续噪声走位向量 */
	inline FVector2D SampleWanderNoise(float TimeSec, float SeedX, float SeedY, float Freq = 0.28f)
	{
		return FVector2D(
			Noise2D(SeedX + TimeSec * Freq, SeedY * 0.71f),
			Noise2D(SeedY + TimeSec * Freq + 17.3f, SeedX * 0.67f));
	}

	/** 仅允许非叠加动画进入 SingleNodeInstance, 防止 bind-pose 告警 */
	inline bool IsSafeSingleNodeAsset(const UAnimationAsset* Asset)
	{
		return Asset && !Asset->IsValidAdditive();
	}

	/** 目标身高 -> 网格缩放/抬升 (让任意网格底盘贴 z=0 且等高) */
	inline float NormalizeMesh(const UStaticMesh* Mesh, float TargetHeight, float& OutLift)
	{
		OutLift = 0.0f;
		if (!Mesh) { return 1.0f; }
		const FBox B = Mesh->GetBoundingBox();
		const float Z = B.GetSize().Z;
		if (Z < KINDA_SMALL_NUMBER) { return 1.0f; }
		const float S = TargetHeight / Z;
		OutLift = -B.Min.Z * S;
		return S;
	}
}
