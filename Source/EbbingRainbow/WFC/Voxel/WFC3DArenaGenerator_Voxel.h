// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_Voxel.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * 多尺度体素竞技场生成器 (默认, ER.ArenaGenerator=0)。
 *
 * 不用 FWFC3DSolver/模块库: 自写一个轻量 2D WFC 高度场 (标签 Void/0..3,
 * |ΔL|<=1, 外部允许破碎 Void 边缘), 再按区域连通性放楼梯,
 * 最后把地形/崖面/钟乳/破碎外壳全部实例化成轴对齐立方体
 * (同一个 /Engine/BasicShapes/Cube, 按材质分 HISM)。
 *
 * 坐标契约: 19x19, CellSize=400, 格 (x,y) 占局部 [x*400,(x+1)*400]^2,
 * 高度层 L 顶面 z = L*200。所有随机只从 FRandomStream(Seed) 派生。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_Voxel : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_Voxel();

	virtual bool Generate(const FWFCGenerationRequest& Request) override;
	virtual void ClearGenerated() override;

protected:
	virtual FVector GetCellWorldSize() const override { return FVector(400.0f, 400.0f, 400.0f); }

private:
	/** 体素材质槽 (HISM 按槽分组) */
	enum class EVoxMat : uint8 { Concrete = 0, Marble, Glow, Glass, Count };

	struct FVoxStair
	{
		int32 LowX = 0;
		int32 LowY = 0;
		int32 Dir = 0; // 0=+Y 1=+X 2=-Y 3=-X, 指向高侧格
	};

	struct FVoxBlock
	{
		int32 X = 0;
		int32 Y = 0;
		int32 W = 1;
		int32 H = 1;
		uint8 Lev = 0;
		float TopOff = 0.0f;
		float Bottom = -100.0f;
		EVoxMat Mat = EVoxMat::Concrete;
	};

	struct FVoxPlan
	{
		uint8 L[19 * 19];            // 高度层, 255 = Void
		TArray<FVoxStair> Stairs;
		TArray<int32> RegionOf;      // 每格区域号 (Void = -1)
		TSet<int32> RoleCells;       // 楼梯角色格(入口/低/高), 一格一角
		TMap<int32, int32> LowEntry; // 低格 -> 入口格
		TMap<int32, int32> LowHigh;  // 低格 -> 高格
		TArray<FVoxBlock> Blocks;    // 共享块切分 (碰撞+可见共用)
		int16 BlockOf[19 * 19];      // 每格所属块, -1 = Void
		int32 Motif = 0;             // 0=Bowl 1=Ziggurat
		float ShellR = 0.0f;
	};

	// ---- 高度场 ----
	bool BuildPlan(int32 Seed, FRandomStream& Rng, FVoxPlan& OutPlan);
	bool CollapseHeightField(FRandomStream& Rng, int32 Motif,
		float Cx, float Cy, uint8* OutL);
	bool BuildBowlField(FRandomStream& Rng, float Cx, float Cy, uint8* OutL);
	float PriorWeight(int32 Motif, int32 MaxL, float Cx, float Cy,
		int32 X, int32 Y, int32 Label) const;
	void FixupSmallRegions(uint8* L);
	bool MergeSmallestUnreached(FVoxPlan& Plan) const;
	bool PlaceStairs(FRandomStream& Rng, FVoxPlan& Plan);
	bool CheckConnectivity(const FVoxPlan& Plan, int32* OutReached = nullptr) const;

	// ---- 实例化 ----
	void PartitionBlocks(FVoxPlan& Plan, int32 Seed, int32 ProbeCell) const;
	void BuildCollision(const FVoxPlan& Plan, FRandomStream& Rng, int32 ProbeCell);
	void BuildVisual(const FVoxPlan& Plan, FRandomStream& Rng);
	UInstancedStaticMeshComponent* GetMatComponent(EVoxMat Slot);
	void AddBox(EVoxMat Slot, const FVector& Center, const FVector& Size);
	void BuildShell(const FVoxPlan& Plan, FRandomStream& Rng,
		TArray<FTransform>& OutConcrete, TArray<FTransform>& OutMarble,
		TArray<FTransform>& OutGlow, TArray<FTransform>& OutGlass);

	UMaterialInterface* LoadMat(const TCHAR* Path);
	float ValueNoise3(float X, float Y, float Z) const;

	/** 生成灯光/雾氛围 (单一主光 + 低强度补光 + 下方雾池) */
	void SpawnLighting(FRandomStream& Rng);

	static constexpr int32 GridW = 19;
	static constexpr int32 GridH = 19;
	static constexpr float CellSize = 400.0f;
	static constexpr float LayerH = 200.0f;
	static constexpr uint8 LVoid = 255;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MatConcrete = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MatMarble = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MatGlowBase = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MatGlassBase = nullptr;

	/** 每次生成按种子创建的渐变动态材质 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynMats;

	/** 按 EVoxMat 分组的可见体素 HISM */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> MatComponents;

	UPROPERTY(Transient)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CollisionHISM = nullptr;

	/** 本次生成产生的氛围演员 (灯光/雾), ClearGenerated 时销毁 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> AtmoActors;
};
