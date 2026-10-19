// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WFCGenerationTypes.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DStyleTable.generated.h"

/** 一条风格映射：Style -> 具体生成器类 + 默认规模 */
USTRUCT(BlueprintType)
struct FWFCStyleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style")
	EWFCBuildingStyle Style = EWFCBuildingStyle::Style0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style")
	TSoftClassPtr<AWFC3DArenaGeneratorBase> GeneratorClass;

	/** 该风格的默认规模（请求中的 Scale 优先，这里只是编辑器/预览默认值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style")
	FIntVector DefaultScale = FIntVector(14, 14, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style")
	FText DisplayName;
};

/**
 * 风格表：把三参数中的 Style 映射到具体的 WFC 生成器类。
 * 新增风格只需要新增一个生成器类，并在这里加一条记录。
 */
UCLASS(BlueprintType)
class EBBINGRAINBOW_API UWFC3DStyleTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Style")
	TArray<FWFCStyleEntry> Styles;

	/** 解析风格对应的生成器类；找不到返回 nullptr */
	UFUNCTION(BlueprintCallable, Category = "WFC|Style")
	TSubclassOf<AWFC3DArenaGeneratorBase> Resolve(EWFCBuildingStyle InStyle) const;

	/** 获取风格的默认规模；找不到返回 14x14x1 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Style")
	FIntVector GetDefaultScale(EWFCBuildingStyle InStyle) const;
};
