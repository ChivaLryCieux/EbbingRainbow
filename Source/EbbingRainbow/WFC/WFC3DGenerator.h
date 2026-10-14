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

	/** 是否开启多层立体空间楼梯强连通保证 (相邻楼层之间必定有至少一处可通行的楼梯井相连) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MultiStory")
	bool bEnsureStairConnectivity = true;

	/** 每两层之间最少保证生成的楼梯数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MultiStory", meta = (ClampMin = "1", ClampMax = "4"))
	int32 StairsPerFloorTransition = 1;

	/** 是否包含天花板/顶层封顶板 (构建 4 层平整地板/天花板结构) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MultiStory")
	bool bIncludeCeilings = true;

	// ==========================================
	// 超大型鸟巢/商场贯通中庭空间配置 (Mega Atrium / Stadium)
	// ==========================================

	/** 是否开启大型鸟巢/商场通高中庭预设模式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium")
	bool bMegaAtriumMode = false;

	/** 环形漫游回廊与建筑在墙体周边的进深 (单位：格子数，例如 4~6 格，中间区域留作通高中庭) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium", meta = (ClampMin = "2", ClampMax = "12"))
	int32 AtriumWalkwayDepth = 4;

	/** 中庭是否采用鸟巢式椭圆/圆角流线形态 (若为 false 则为矩形开敞中庭) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|MegaAtrium")
	bool bOvalAtriumShape = true;

	// ==========================================
	// 螺旋花园高塔预设配置 (Spiral Garden Tower) - 巨构尺度
	// 设计意象：一条 112m 高的巨型螺旋缎带盘旋 3.9 圈 —— 没有桶状墙体、没有中央立柱，
	// 只有联通的 24m 宽延伸地板 + 地板边缘的栏杆 + 地板上的立方体踏步。
	// 拓扑由 WFC 约束驱动：中央 36m 贯通天井 + 逐层旋转 25° 的 49° 扇形缎带 +
	// 缎带内外缘与端头栏杆 + 层间交错楼梯/坡道 + 局部外扩悬浮耳台。
	// ==========================================

	/** 开启螺旋花园高塔模式 (Generate3DSpace 将按螺旋拓扑构建初始约束) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden")
	bool bSpiralGardenMode = false;

	/** 每层缎带绕塔心旋转角 (度)。整塔累计旋转 = 圈数 x 360 (默认 25°/层，56 层盘旋约 3.9 圈，每圈上升 28.8m 保证圈层次分明) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float SpiralTwistPerLayer = 25.0f;

	/** 相邻层爬升段重叠角 (度)。缎带弧宽 = 每层旋转角 + 重叠角，重叠区即楼梯落脚处，保证动线连续 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "6.0", ClampMax = "80.0"))
	float SpiralArcOverlapDeg = 24.0f;

	/** 缎带内缘半径 (格)。此半径以内为贯通全塔的中央天井 (36m 宽，高空俯瞰的震撼核心) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "3", ClampMax = "16"))
	int32 SpiralRibbonInnerRadius = 9;

	/** 缎带外缘半径 (格)。缎带宽度 = 外缘 - 内缘 (默认 12 格 = 24m 宽步道)，越宽越气势恢宏 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "4", ClampMax = "30"))
	int32 SpiralRibbonOuterRadius = 21;

	/** 局部外扩耳台出现概率 (每层独立判定：缎带某一层向外多悬挑 5 格，形成错落轮廓) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralRibbonWidenChance = 0.35f;

	// ==========================================
	// 螺旋缎带点缀 (拱门/门 + 花坛) - 单一 Cube 拼装形态的概率化布置
	// 形态定义见 SetupSpiralGardenPreset，布置逻辑见 Generate3DSpace 螺旋约束第 4 阶段
	// ==========================================

	/** 是否在缎带路面上概率生成 "拱门(门)" 形态 (双立柱+横梁+顶石，中部门洞敞开可穿行) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden")
	bool bGenerateSpiralArchways = true;

	/** 拱门在缎带核心地面格上的单格生成概率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralArchwayChance = 0.07f;

	/** 拱门之间的最小间距 (格，切比雪夫距离，避免门挨门) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SpiralArchwayMinSpacing = 3;

	/** 是否在缎带路面上概率生成 "花坛" 形态 (抬升土床+围檐+花丛，约 71cm 高不遮挡动线) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden")
	bool bGenerateSpiralPlanters = true;

	/** 花坛在缎带边缘格 (紧邻栏杆/虚空的格) 的生成概率 —— 边缘高概率形成临栏绿带 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralPlanterEdgeChance = 0.22f;

	/** 花坛在缎带中央格的生成概率 (低概率保持步道通畅) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpiralPlanterChance = 0.04f;

	/** 花坛之间的最小间距 (格，切比雪夫距离) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|SpiralGarden", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SpiralPlanterMinSpacing = 2;

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
	 * [按钮] 一键装载 FBX_Export 模块化组件规则系统 (自动匹配 200cm 模数、地面/墙体/门洞/转角/立柱/天花板插槽方向)
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void SetupModularKitPreset();

	/**
	 * [按钮] 一键装载【三层空间四层地板】立体地牢预设 (配置 8x8x3 网格、层间必有楼梯连通、第4层顶层封顶)
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void SetupThreeStoryPreset();

	/**
	 * [按钮] 一键装载【超大型鸟巢/商场贯通中庭】预设 (44x44x10 超百倍体积、中间通高中庭、环形蔓延回廊与立体阶梯)
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void SetupMegaAtriumPreset();

	/**
	 * [按钮] 一键装载【螺旋花园高塔】预设：
	 * 40 米高 20 层悬浮巨塔，中央巨木树干，环形回廊以 18°/层 盘旋而上 (整塔盘旋一周)；
	 * 全程无天花板向天空敞开，每层弧心交错布设楼梯与坡道，形成真实可攀爬的螺旋动线；
	 * 约 4 成概率随机外扩宽阔露台，每 7 层出现 "花园口袋层" 交由 WFC 自由发挥；
	 * 底层为悬浮岛式基座广场，顶层观景平台点缀立柱、三角鳍饰与世界树尖顶；
	 * 塔身挂满真实植被 (冷杉/花丛/灌木)，每次生成轮廓与植被分布都不同。
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WFC Operations")
	void SetupSpiralGardenPreset();

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

	/** 最近一次 Generate3DSpace 是否成功求解并渲染 */
	UFUNCTION(BlueprintPure, Category = "WFC Operations")
	bool WasLastGenerationSuccessful() const { return bLastGenerationSucceeded; }

private:
	bool bLastGenerationSucceeded = false;


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
