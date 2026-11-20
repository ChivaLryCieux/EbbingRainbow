// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 虹不我与 — 高性能纯 C++ UI 动效内核 (零 UObject / 零 GC / 亚微秒级每帧开销):
 *  - 二阶弹簧阻尼求解器 (FERFloatSpring / FERVec2Spring): 物理平滑、可随时打断、无硬切
 *  - 建筑感缓动曲线 (ExpoOut / QuintOut / QuartInOut): 干脆利落，克制神性，拒绝卡通弹跳
 *  - 错峰级联时间轴 (FERRevealTrack): 驱动行/卡/刻度/分割线的序贯展开
 *  - 机能风遥测译码文字 (FERCipherReveal): 英文代号/编号克制解码动效 (Control / 明日方舟风)
 */
namespace ERMotion
{
	inline float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.0f, 1.0f);
	}

	/** 极速切入、建筑感收尾 (适合面板展开、分割线划出、选中锁定) */
	inline float ExpoOut(float T)
	{
		T = Clamp01(T);
		return (T >= 1.0f) ? 1.0f : (1.0f - FMath::Pow(2.0f, -10.0f * T));
	}

	/** 五次方缓出 (比 Cubic 更静谧庄严的长尾 decelerate) */
	inline float QuintOut(float T)
	{
		T = Clamp01(T);
		const float U = 1.0f - T;
		return 1.0f - U * U * U * U * U;
	}

	/** 三次方缓出 */
	inline float CubicOut(float T)
	{
		T = Clamp01(T);
		const float U = 1.0f - T;
		return 1.0f - U * U * U;
	}

	/** 四次方缓入缓出 (适合扫描光扫过、黑边收放) */
	inline float QuartInOut(float T)
	{
		T = Clamp01(T);
		if (T < 0.5f)
		{
			return 8.0f * T * T * T * T;
		}
		const float U = -2.0f * T + 2.0f;
		return 1.0f - (U * U * U * U) * 0.5f;
	}

	/** 极克制过冲 (仅 2% 轻微回弹, 适合高亮确认/按键回弹) */
	inline float CrispSettle(float T)
	{
		T = Clamp01(T);
		const float U = T - 1.0f;
		constexpr float C1 = 0.65f;
		constexpr float C3 = C1 + 1.0f;
		return 1.0f + C3 * U * U * U + C1 * U * U;
	}

	/** 错峰窗口采样: 将全局时间 Elapsed 映射到 [Delay, Delay + Duration] 的 [0, 1] */
	inline float Window(float Elapsed, float Delay, float Duration)
	{
		if (Duration <= KINDA_SMALL_NUMBER)
		{
			return Elapsed >= Delay ? 1.0f : 0.0f;
		}
		return Clamp01((Elapsed - Delay) / Duration);
	}

	/** 慢速庄严呼吸 (0..1, 周期约 4 秒) */
	inline float SolemnPulse(float TimeSeconds, float Speed = 1.55f)
	{
		const float S = 0.5f + 0.5f * FMath::Sin(TimeSeconds * Speed);
		return S * S * (3.0f - 2.0f * S);
	}
}

/**
 * 二阶阻尼弹簧 (单标量):
 * 使用子步积分 (max step 1/120s), 保证任意帧率下绝对稳定不发散。
 */
struct FERFloatSpring
{
	float Value = 0.0f;
	float Target = 0.0f;
	float Velocity = 0.0f;
	/** 刚度 (越大响应越利落) */
	float Stiffness = 320.0f;
	/** 阻尼比 (1.0 = 临界阻尼无过冲; 0.82 = 极轻手感回弹) */
	float DampingRatio = 0.96f;

	FERFloatSpring() = default;
	FERFloatSpring(float Initial, float InStiffness, float InDampingRatio)
		: Value(Initial), Target(Initial), Stiffness(InStiffness), DampingRatio(InDampingRatio)
	{}

	static FERFloatSpring Snappy(float Initial = 0.0f)     { return FERFloatSpring(Initial, 440.0f, 0.98f); }
	static FERFloatSpring Tactile(float Initial = 0.0f)    { return FERFloatSpring(Initial, 340.0f, 0.84f); }
	static FERFloatSpring Monumental(float Initial = 0.0f) { return FERFloatSpring(Initial, 190.0f, 1.00f); }
	static FERFloatSpring Silky(float Initial = 0.0f)      { return FERFloatSpring(Initial, 140.0f, 0.96f); }

	void SnapTo(float V)
	{
		Value = V;
		Target = V;
		Velocity = 0.0f;
	}

	void SetTarget(float InTarget)
	{
		Target = InTarget;
	}

	void Impulse(float DeltaVel)
	{
		Velocity += DeltaVel;
	}

	bool IsSettled(float PosEps = 0.0008f, float VelEps = 0.002f) const
	{
		return FMath::Abs(Target - Value) <= PosEps && FMath::Abs(Velocity) <= VelEps;
	}

	float Tick(float DeltaTime)
	{
		if (DeltaTime <= 0.0f)
		{
			return Value;
		}
		// 防止切后台或卡顿导致的超大 dt
		float Remaining = FMath::Min(DeltaTime, 0.08f);
		const float Omega = FMath::Sqrt(FMath::Max(1.0f, Stiffness));
		const float Damp = 2.0f * DampingRatio * Omega;

		constexpr float MaxSubStep = 1.0f / 120.0f;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Dt = FMath::Min(Remaining, MaxSubStep);
			const float Accel = Stiffness * (Target - Value) - Damp * Velocity;
			Velocity += Accel * Dt;
			Value += Velocity * Dt;
			Remaining -= Dt;
		}

		if (IsSettled())
		{
			Value = Target;
			Velocity = 0.0f;
		}
		return Value;
	}
};

/** 二阶阻尼弹簧 (2D 向量) */
struct FERVec2Spring
{
	FERFloatSpring X;
	FERFloatSpring Y;

	FERVec2Spring() = default;
	FERVec2Spring(FVector2D Initial, float Stiffness, float DampingRatio)
		: X(Initial.X, Stiffness, DampingRatio)
		, Y(Initial.Y, Stiffness, DampingRatio)
	{}

	static FERVec2Spring Snappy(FVector2D Initial = FVector2D::ZeroVector)  { return FERVec2Spring(Initial, 420.0f, 0.98f); }
	static FERVec2Spring Tactile(FVector2D Initial = FVector2D::ZeroVector) { return FERVec2Spring(Initial, 320.0f, 0.85f); }

	void SnapTo(FVector2D V)   { X.SnapTo(V.X); Y.SnapTo(V.Y); }
	void SetTarget(FVector2D V){ X.SetTarget(V.X); Y.SetTarget(V.Y); }
	FVector2D GetValue() const { return FVector2D(X.Value, Y.Value); }
	bool IsSettled() const     { return X.IsSettled() && Y.IsSettled(); }

	FVector2D Tick(float DeltaTime)
	{
		return FVector2D(X.Tick(DeltaTime), Y.Tick(DeltaTime));
	}
};

/**
 * 错峰入场/转场时钟:
 * 记录一次触发后的流逝秒数, 各子控件按 (Delay, Duration) 采样即可实现精准序贯编排。
 */
struct FERRevealTrack
{
	float Elapsed = 0.0f;
	float TotalDuration = 0.85f;
	bool bActive = false;

	void Trigger(float InTotalDuration = 0.85f)
	{
		Elapsed = 0.0f;
		TotalDuration = FMath::Max(0.05f, InTotalDuration);
		bActive = true;
	}

	void FinishImmediately()
	{
		Elapsed = TotalDuration;
		bActive = false;
	}

	bool Tick(float DeltaTime)
	{
		if (!bActive)
		{
			return false;
		}
		Elapsed += FMath::Min(FMath::Max(0.0f, DeltaTime), 0.05f);
		if (Elapsed >= TotalDuration)
		{
			Elapsed = TotalDuration;
			bActive = false;
		}
		return true;
	}

	bool IsRunning() const { return bActive; }

	float Expo(float Delay, float Duration) const
	{
		return ERMotion::ExpoOut(ERMotion::Window(Elapsed, Delay, Duration));
	}

	float Quint(float Delay, float Duration) const
	{
		return ERMotion::QuintOut(ERMotion::Window(Elapsed, Delay, Duration));
	}
};

/**
 * 机能风遥测译码文字 (英文大写编号/代号专用):
 * 左向右高速收敛, 仅在活跃期间做极克制的字符替换, 收敛后零计算、零抖动。
 */
struct FERCipherReveal
{
	FString Target;
	float Elapsed = 0.0f;
	float Delay = 0.0f;
	float Duration = 0.34f;
	bool bActive = false;

	void Start(const FString& InTarget, float InDuration = 0.34f, float InDelay = 0.0f)
	{
		Target = InTarget;
		Elapsed = 0.0f;
		Delay = FMath::Max(0.0f, InDelay);
		Duration = FMath::Max(0.06f, InDuration);
		bActive = !Target.IsEmpty();
	}

	void FinishImmediately()
	{
		Elapsed = Delay + Duration;
		bActive = false;
	}

	bool IsRunning() const { return bActive; }

	/** 推进一帧; 返回 true 表示文字内容有变化需要 SetText */
	bool Tick(float DeltaTime, FString& OutDisplay)
	{
		if (!bActive)
		{
			return false;
		}

		Elapsed += FMath::Min(FMath::Max(0.0f, DeltaTime), 0.05f);
		const float T = ERMotion::Window(Elapsed, Delay, Duration);
		if (T >= 1.0f)
		{
			bActive = false;
			OutDisplay = Target;
			return true;
		}

		static const TCHAR GlyphPool[] = TEXT("01/:_-+AX");
		constexpr int32 PoolLen = 9;

		const int32 Len = Target.Len();
		// 线性高速从左向右锁字 (在 90% 进度处全部收敛, 避免尾字长时间停留在乱码态)
		const float LockFrac = FMath::Clamp(T / 0.90f, 0.0f, 1.0f);
		const int32 LockedCount = FMath::FloorToInt(LockFrac * (float)Len + 0.0001f);
		const int32 StepBucket = FMath::FloorToInt(Elapsed * 28.0f);

		OutDisplay.Reset(Len);
		for (int32 i = 0; i < Len; ++i)
		{
			const TCHAR Ch = Target[i];
			// 保留空格与分隔符, 防止排版宽度抖动
			if (i < LockedCount || Ch == TEXT(' ') || Ch == TEXT('/') || Ch == TEXT('·') || Ch == TEXT('.') || Ch == TEXT('-'))
			{
				OutDisplay.AppendChar(Ch);
			}
			else if (i <= LockedCount + 2)
			{
				const uint32 Hash = (uint32)(i * 37 + StepBucket * 13 + (int32)Ch);
				OutDisplay.AppendChar(GlyphPool[Hash % PoolLen]);
			}
			else
			{
				OutDisplay.AppendChar(TEXT(' '));
			}
		}
		return true;
	}
};
