// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WFC3DTypes.h"
#include "WFC3DModuleLibrary.generated.h"

/**
 * 3D WFC 模块资产库 (UDataAsset)
 * 存储模块定义、相邻约束规则，并提供从指定目录一键扫描网格体与规则预编译功能。
 */
UCLASS(BlueprintType)
class EBBINGRAINBOW_API UWFC3DModuleLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UWFC3DModuleLibrary();

	/** 用户配置的模块列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Library")
	TArray<FWFC3DModuleDefinition> Modules;

	/** 是否自动添加空气/空单元格模块 (空 Mesh)，以产生走廊/房间/开阔空间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Library|Air")
	bool bIncludeAirModule = true;

	/** 空气模块的采样权重 (权重越大，留白和通道越多) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Library|Air", meta = (EditCondition = "bIncludeAirModule", ClampMin = "0.01"))
	float AirModuleWeight = 3.0f;

	/** 空气模块各个面的插槽名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Library|Air", meta = (EditCondition = "bIncludeAirModule"))
	FName AirSocketName = FName(TEXT("Air"));

	/** 自定义显式相容插槽对 (若为空，则默认按插槽名相同即相容的对称规则匹配) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC Library|Rules")
	TArray<FWFC3DSocketCompatibilityPair> CustomCompatibilityPairs;

public:
	/**
	 * 从指定 Content 路径（例如 "/Game/LevelPrototyping/Meshes"）扫描所有 StaticMesh 并添加到模块库
	 * @param DirectoryPath Content 路径
	 * @param DefaultHorizontalSocket X/Y 轴默认插槽名
	 * @param DefaultVerticalSocket Z 轴默认插槽名
	 * @param bAutoRotations 是否自动开启四向旋转变体
	 * @param bClearExisting 是否清空现有列表
	 * @return 扫描添加的网格体数量
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WFC Library")
	int32 ScanDirectoryForStaticMeshes(
		const FString& DirectoryPath = TEXT("/Game/LevelPrototyping/Meshes"),
		FName DefaultHorizontalSocket = FName(TEXT("Default")),
		FName DefaultVerticalSocket = FName(TEXT("Default")),
		bool bAutoRotations = true,
		bool bClearExisting = false
	);

	/**
	 * 将模块配置编译展开（旋转变体衍生 + 兼容性查找矩阵构建）
	 * @param OutCompiledModules 输出编译后的变体模块列表
	 * @param OutCompatibilityTable 输出预计算相容性表: [ModuleA][Direction] -> BitArray(合法ModuleB集合)
	 * @return 编译是否成功
	 */
	bool CompileLibrary(
		TArray<FCompiledWFC3DModule>& OutCompiledModules,
		TArray<TArray<TBitArray<>>>& OutCompatibilityTable
	) const;

	/** 检查插槽 A 与相邻侧插槽 B 是否兼容 */
	bool AreSocketsCompatible(FName SocketA, FName SocketB) const;
};
