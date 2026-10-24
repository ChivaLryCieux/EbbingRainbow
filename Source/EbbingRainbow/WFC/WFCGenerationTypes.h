// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFCGenerationTypes.generated.h"

/**
 * 建筑风格。
 * 风格与进度解耦：进度层负责把 Act 映射到 Style，WFC 只认 0/1/2。
 * 每个 Style 由 UWFC3DStyleTable 映射到一套独立的生成器类与模型/材质套件。
 */
UENUM(BlueprintType)
enum class EWFCBuildingStyle : uint8
{
	Style0 = 0 UMETA(DisplayName = "Style 0"),
	Style1 = 1 UMETA(DisplayName = "Style 1"),
	Style2 = 2 UMETA(DisplayName = "Style 2")
};

/**
 * 三参数生成请求，这是上层唯一需要关心的 WFC 输入契约。
 * - Style: 风格（决定生成器、模型、材质、规则）
 * - Scale: 规模（X/Y/Z 格数；世界尺寸 = 格数 x 风格模块模数）
 * - Seed: 随机种子（0 = 随机，仅调试用；正式流程请传入非 0 种子以便复现）
 */
USTRUCT(BlueprintType)
struct FWFCGenerationRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Request")
	EWFCBuildingStyle Style = EWFCBuildingStyle::Style0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Request")
	FIntVector Scale = FIntVector(14, 14, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Request")
	int32 Seed = 0;

	FString ToString() const
	{
		return FString::Printf(TEXT("Style=%d Scale=%dx%dx%d Seed=%d"),
			static_cast<int32>(Style), Scale.X, Scale.Y, Scale.Z, Seed);
	}
};

/** 一次生成的结果，供上层做日志、复现、包围盒与出生点计算。 */
USTRUCT(BlueprintType)
struct FWFCGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	int32 UsedSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	FIntVector UsedScale = FIntVector::ZeroValue;

	/** 该风格单个模块/格子的世界尺寸（cm），由具体生成器提供 */
	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	FVector CellWorldSize = FVector::ZeroVector;

	/** World-space probe on the walkable upper level (used by arena nav checks). */
	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	FVector UpperNavProbe = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	int32 CellCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WFC|Result")
	double DurationMs = 0.0;
};
