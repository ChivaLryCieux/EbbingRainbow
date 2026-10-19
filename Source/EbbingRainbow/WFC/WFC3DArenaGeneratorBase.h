// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WFC3DTypes.h"
#include "WFC3DSolver.h"
#include "WFCGenerationTypes.h"
#include "WFC3DArenaGeneratorBase.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UWFC3DModuleLibrary;

/**
 * 通用 WFC 生成器基类。
 * 只负责所有风格共享的流程：编译模块 -> 初始化求解器 -> 施加约束 -> 求解 -> 渲染。
 * 具体风格（地牢/中庭/螺旋…）由子类实现 BuildModules / BuildConstraints / PostProcess。
 *
 * 注意：顶层调用请使用 AWFC3DArenaHost，不要直接依赖具体子类。
 */
UCLASS(Abstract)
class EBBINGRAINBOW_API AWFC3DArenaGeneratorBase : public AActor
{
	GENERATED_BODY()

public:
	AWFC3DArenaGeneratorBase();

	/** 三参数生成入口，子类一般不需要重写 */
	virtual bool Generate(const FWFCGenerationRequest& Request);

	/** 清理当前生成的实例（保留 HISM 组件，便于同风格复用） */
	UFUNCTION(BlueprintCallable, Category = "WFC")
	virtual void ClearGenerated();

	UFUNCTION(BlueprintPure, Category = "WFC")
	const FWFCGenerationResult& GetLastResult() const { return LastResult; }

protected:
	/**
	 * 构建本风格的模块集与邻接相容表。
	 * 这里保留默认实现而不是纯虚函数：UE 的 DEFINE_ABSTRACT_DEFAULT_CONSTRUCTOR_CALL
	 * 在普通构建下仍会实例化 TClass，纯虚函数会导致 UCLASS 编译失败。
	 */
	virtual bool BuildModules(
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility)
	{
		return false;
	}

	/** 构建本风格的初始固定约束（房间/楼梯/中庭/螺旋…） */
	virtual bool BuildConstraints(
		const FIntVector& GridSize,
		int32 Seed,
		TMap<int32, int32>& OutClamps)
	{
		return true;
	}

	/** 求解完成后的装饰、清理与修正；可重写 */
	virtual void PostProcess(int32 Seed) {}

	/** 本风格单个模块/格子的世界尺寸（cm） */
	virtual FVector GetCellWorldSize() const { return FVector(200.0f); }

	/** 便捷函数：把模块库编译成求解器数据 */
	bool CompileModuleLibrary(
		UWFC3DModuleLibrary* Library,
		TArray<FCompiledWFC3DModule>& OutModules,
		TArray<TArray<TBitArray<>>>& OutCompatibility);

	/** 按 SourceModuleId 查找模块索引（优先返回 Rot0 变体） */
	int32 FindModuleIndex(FName SourceModuleId) const;

	/** 按 SourceModuleId + Yaw 角度查找旋转变体索引 */
	int32 FindModuleVariantIndex(FName SourceModuleId, int32 YawDegrees) const;

	/** 便捷函数：写入一个固定格（越界或 INDEX_NONE 自动忽略） */
	FORCEINLINE void AddClamp(
		TMap<int32, int32>& Clamps,
		int32 X, int32 Y, int32 Z,
		int32 ModuleIndex) const
	{
		if (ModuleIndex == INDEX_NONE)
		{
			return;
		}

		const FIntVector Coord(X, Y, Z);
		if (Solver.IsValidCoord(Coord))
		{
			Clamps.Add(Solver.CoordToIndex(Coord), ModuleIndex);
		}
	}

	/** 将求解结果实例化为 HISM */
	void RenderResult();

	/** 获取或创建指定 StaticMesh 对应的实例化组件 */
	UInstancedStaticMeshComponent* GetOrCreateInstancedComponent(UStaticMesh* Mesh);

protected:
	FWFC3DSolver Solver;
	TArray<FCompiledWFC3DModule> CompiledModules;
	TArray<TArray<TBitArray<>>> CompatibilityTable;

	/** 是否使用 HISM（默认开启） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Rendering")
	bool bUseHISM = true;

	/** 冲突重试次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Solver", meta = (ClampMin = "1", ClampMax = "200"))
	int32 MaxRetryAttempts = 50;

	FWFCGenerationResult LastResult;

private:
	/** 按网格体分类缓存的实例化组件列表 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> InstancedMeshComponents;
};
