// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFC3DArenaGeneratorBase.h"
#include "WFC3DArenaGenerator_Nature.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * 多尺度自然漂浮岛竞技场生成器 (Style 2 / ER.ArenaGenerator=2)。
 *
 * 思路:
 * - 从 Gaea 高度图 (Content/Nature/E-R-Terrain.u16, 4033^2 uint16) 随机截取
 *   一块窗口做高通滤波得到中小尺度细节; 宏观形态由本生成器的
 *   Perlin + 岛形遮罩决定 (平顶可玩区 -> 缓坡丘陵 -> 边缘悬崖 -> 虚空)。
 * - 地形用 UProceduralMeshComponent 运行时构建, 带碰撞 -> Recast 自动烘焙
 *   可行走面 (坡度限制内)。出生点仍由 BattleArena 在 nav 面上自动选。
 * - 植被/岩石/废墟全部 HISM 种子化撒点; 岛底挂漂浮岩块, 与体素岛同一
 *   视觉语言 (漂浮沙盘)。
 * - 同 seed 同岛: 一切随机性走 FRandomStream(Seed + k)。
 */
UCLASS()
class EBBINGRAINBOW_API AWFC3DArenaGenerator_Nature : public AWFC3DArenaGeneratorBase
{
	GENERATED_BODY()

public:
	AWFC3DArenaGenerator_Nature();

	virtual bool Generate(const FWFCGenerationRequest& Request) override;
	virtual void ClearGenerated() override;

protected:
	virtual FVector GetCellWorldSize() const override { return FVector(50.0f); }

private:
	// ---- 地形场 ----
	static constexpr int32 GridN = 241;          // 顶点数/边 (0.5m 步长 -> 120m)
	static constexpr float Step = 50.0f;         // cm
	static constexpr float CenterX = 3800.0f;
	static constexpr float CenterY = 3800.0f;
	static constexpr float PlateauR = 3600.0f;   // 平整可玩区半径 (cm)
	static constexpr float IslandR0 = 5600.0f;   // 岛轮廓基准半径 (cm)
	static constexpr float RimDrop = 2600.0f;    // 边缘崖壁下落深度 (cm)

	struct FNatField
	{
		TArray<float> H;                         // GridN*GridN 世界高度 (cm)
		TArray<uint8> Valid;                     // 该顶点是否在岛轮廓内
		float PlateauH = 0.0f;                   // 可玩区基准高度
		float MaxRadius = 0.0f;                  // 实际最大轮廓半径
	};

	/** 载入(并缓存) Gaea 高度图; 失败返回 false 走纯 Perlin 兜底 */
	static bool LoadHeightmap();
	static const TArray<uint16>& HeightData() { return CachedHeight; }
	static bool HasHeight() { return CachedHeight.Num() == 4033 * 4033; }

	/** 构建高度场: 截取+高通细节 + 岛形 + 可玩区坡度松弛 */
	void BuildField(int32 Seed, FNatField& OutField) const;
	/** 世界坐标 -> 高度场采样 (双线性); 不在岛内返回 false */
	bool SampleHeight(const FNatField& Field, float X, float Y, float& OutH, float& OutSlopeDeg) const;
	/** 岛轮廓半径 (含 Perlin 扰动) */
	float OutlineRadius(float AngleRad, float Phase) const;

	// ---- 网格 ----
	void BuildTerrainMesh(const FNatField& Field);
	void BuildUnderside(const FNatField& Field, FRandomStream& Rng);

	// ---- 行走层 ----
	// UE5.8 中运行时构建的 ProceduralMeshComponent 不向 nav 八叉树供几何时
	// (元素初始化时无 BodySetup, 之后不重建), 已用微型方块实验证实。
	// 因此用隐形 HISM 阶梯板按高度场铺一层 nav 地面: 板间距松弛到 <=30cm
	// (Recast 步高 35), 走不过去的陡沿自然断开。
	void BuildNavProxy(const FNatField& Field);

	// ---- 撒点 ----
	void ScatterVegetation(const FNatField& Field, FRandomStream& Rng);
	void ScatterCover(const FNatField& Field, FRandomStream& Rng);
	void ScatterRuins(const FNatField& Field, FRandomStream& Rng);
	void ScatterUnderRocks(const FNatField& Field, FRandomStream& Rng);
	UInstancedStaticMeshComponent* GetOrCreateComp(UStaticMesh* Mesh, bool bEnableCollision);
	void AddInstance(UInstancedStaticMeshComponent* Comp, const FVector& Pos,
		const FRotator& Rot, float Scale, const FNatField* Field = nullptr);

	// ---- 氛围 (与体素生成器同一套路: 接管关卡演员 + 三点光) ----
	void SpawnLighting(FRandomStream& Rng);

	// ---- 资产 ----
	UStaticMesh* LoadMesh(const TCHAR* Path);
	UMaterialInterface* LoadMaterial(const TCHAR* Path);

	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> ScatterProbe;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MatGround;
	TArray<TObjectPtr<AActor>> AtmosphereActors;
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ScatterComps;

	static TArray<uint16> CachedHeight;
	static bool bHeightLoadAttempted;
};
