// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WFC3DTypes.h"
#include "WFC3DModuleLibrary.h"
#include "WFC3DSolver.h"
#include "WFC3DGenerator.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * 3D 波函数坍缩 (WFC) 生成器 Actor
 * 可放置于场景中，通过指定静态网格体目录或模块资产库，程序化生成完整的 3D 空间。
 * 具备一键扫描、一键生成、单步演示教学、全量实例化渲染（HISM）等功能。
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (DisplayName = "WFC 3D Generator"))
class EBBINGRAINBOW_API AWFC3DGenerator : public AActor
{
	GENERATED_BODY()

public:
	AWFC3DGenerator();

protected:
	virtual void BeginPlay() override;

public:
	// ==========================================
	// 基础网格空间配置
	// ==========================================

	/** 3D 空间的网格尺寸 (X 轴格子数, Y 轴格子数, Z 轴层数) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Grid")
	FIntVector GridSize = FIntVector(6, 6, 3);

	/** 单个网格单元格的三维物理尺寸 (厘米，通常对应模块化资产的边界尺寸，如 400x400x400) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Grid")
	FVector CellSize = FVector(400.0f, 400.0f, 400.0f);

	/** 是否在游戏启动 (BeginPlay) 时自动生成空间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Grid")
	bool bAutoGenerateOnBeginPlay = false;

	// ==========================================
	// 模块与目录配置
	// ==========================================

	/** 模块资产库 (可选；若指定则优先使用资产库中的模块和定制规则) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Modules")
	TObjectPtr<UWFC3DModuleLibrary> ModuleLibrary = nullptr;

	/**
	 * 指定要扫描静态网格体的 Content 路径 (例如 "/Game/LevelPrototyping/Meshes")
	 * 当未指定 ModuleLibrary 或希望从目录一键导入时使用
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Directory Scan")
	FString ScanFolderPath = TEXT("/Game/LevelPrototyping/Meshes");

	/** 扫描时默认赋予的横向插槽名 (+X, -X, +Y, -Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Directory Scan")
	FName DefaultHorizontalSocket = FName(TEXT("Default"));

	/** 扫描时默认赋予的纵向插槽名 (+Z, -Z) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Directory Scan")
	FName DefaultVerticalSocket = FName(TEXT("Default"));

	/** 扫描时是否自动为网格体生成 4 个 Z 轴旋转变体 (0°, 90°, 180°, 270°) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Directory Scan")
	bool bAutoRotationsOnScan = true;

	/** 生成时是否包含空气/虚空模块 (空 Mesh)，以营造室内/通道等负空间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Air")
	bool bIncludeAir = true;

	/** 空气模块的采样权重 (权重越大，空间越通透开阔) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Air", meta = (EditCondition = "bIncludeAir", ClampMin = "0.01"))
	float AirWeight = 3.0f;

	// ==========================================
	// 算法控制参数
	// ==========================================

	/** 随机数种子 (0 代表每次点击生成不同的随机种子) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Solver")
	int32 RandomSeed = 0;

	/** 求解遇冲突时的最大重试轮次 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Solver", meta = (ClampMin = "1", ClampMax = "200"))
	int32 MaxRetryAttempts = 50;

	/** 是否使用分层实例化静态网格体 (HISM)，性能卓越，支持超低 Draw Call 与 Nanite/LOD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Rendering")
	bool bUseHISM = true;

public:
	/** 是否强制最底层 (Z = 0) 为实体地面，确保建筑具备扎实的地基且绝不悬空 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Constraints")
	bool bEnforceGroundFloor = true;

public:
	// ==========================================
	// 编辑器一键操作 (CallInEditor)
	// ==========================================

	/**
	 * [按钮] 一键装载预设的规整 3D 建筑/地牢规则系统 (自动修正 CellSize、配置地面、墙体、门洞、立柱与空气)
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void SetupDungeonPreset();

	/**
	 * [按钮] 一键扫描指定目录下的所有 StaticMesh 并加载为模块
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void ScanDirectoryAssets();

	/**
	 * [按钮] 执行 3D 波函数坍缩并生成整个三维空间
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void Generate3DSpace();

	/**
	 * [按钮] 清理已生成的场景实例
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void ClearGenerated();

	/**
	 * [按钮] 单步执行一次坍缩与传播，并在视口中实时更新已确定的格子（用于观察算法坍缩动态）
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void StepCollapse();

private:
	/** 准备并编译模块数据 */
	bool PrepareLibrary();

	/** 将求解结果在场景中完成实例化构建 */
	void RenderResult();

	/** 获取或创建指定 StaticMesh 对应的 InstancedStaticMeshComponent */
	UInstancedStaticMeshComponent* GetOrCreateInstancedComponent(UStaticMesh* Mesh);

private:
	UPROPERTY(Transient)
	TObjectPtr<UWFC3DModuleLibrary> RuntimeLibrary = nullptr;

	TArray<FCompiledWFC3DModule> CompiledModules;
	TArray<TArray<TBitArray<>>> CompatibilityTable;

	FWFC3DSolver Solver;
	bool bSolverInitialized = false;

	/** 按网格体分类缓存的实例化组件列表 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> InstancedMeshComponents;
};
