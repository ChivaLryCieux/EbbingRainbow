// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RouteSceneBuilder.generated.h"

class URunSubsystem;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FRouteGraph;

/**
 * 航线图 3D 场景搭建器 (RouteGameMode 在 StartPlay 里生成并调用 BuildScene)。
 *
 * 交互/逻辑仍由 Slate 叠加层负责, 本层只出氛围 (全部自发光/无光照, 不依赖关卡灯光)。
 * 所有材质是 /Game/Run/Materials 下的程序化 Shader (Scripts/route_make_materials*.py 生成),
 * 刻意不放零星粒子 (试过 Niagara 星尘/火星, 观感廉价, 已移除):
 * - M_Route_Nebula   : fbm 噪声云 (缓慢漂移的弥散光池)
 * - M_Route_Rainbow  : 单平面光谱弧 + 微闪 (虹不我与, 呼应游戏名)
 * - M_Route_Rays     : 噪声条纹光柱 (神性圣光)
 * - M_Route_Halo     : 节点光环 (柔光 + 细环 + 旋转射线)
 * - 只有玩家当前所在节点发光 (金色光环+圣光柱), 其余节点/连线一律不发光; 不放零星粒子 (廉价)`n * - 一个仅在关卡没有自带无界后处理时才生成的 PostProcessVolume (固定曝光 + bloom + 暗角)
 *
 * 同一张 Run 图 -> 同一场景 (一切随机量都从 RunSeed 派生)。
 * 画布位置从 Slate 实际布局读取 (ARouteHUD::GetMapLayout), 视口/布局变化会自动重建;
 * 节点状态变化时调 BuildScene 刷新即可。
 */
UCLASS()
class EBBINGRAINBOW_API ARouteSceneBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARouteSceneBuilder();

	/** 按当前 Run 图搭建整个场景 (幂等: 重复调用先清空) */
	void BuildScene(URunSubsystem* Run);

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FSP { FName Name; float Value; };
	struct FVP { FName Name; FLinearColor Value; };

	void ClearScene();
	/** 相机归位: 路线相机拉到原点朝 +X, 场景全部在它的前方 */
	void ResetRouteCamera();
	FVector2D GetViewSize() const;
	/** 屏幕像素 -> 相机前方 Depth 处的世界点 (所有点共面, 与屏幕严格对齐) */
	bool ScreenToPlane(const FVector2D& ScreenPx, float Depth, FVector& OutPos) const;
	bool CanvasToWorld(const FVector2D& CanvasPos, float Depth, FVector& OutPos) const;
	bool PollMapLayout(FVector2D& OutOrigin, float& OutScale) const;
	/** Depth 处视锥的半宽/半高 (cm) */
	FVector2D FrustumHalf(float Depth) const;
	/** Depth 处 1 个屏幕像素对应多少 cm */
	float WorldPerPx(float Depth) const;
	/** 画布单位 -> 屏幕像素 */
	float CanvasScalePx() const;

	UInstancedStaticMeshComponent* MakeISM(UStaticMesh* Mesh, UMaterialInterface* Mat);
	/** 同参数的 (网格, 材质, 参数) 组合复用同一个 ISM; PulseParam 非空则该标量随时间呼吸 */
	UInstancedStaticMeshComponent* MatISM(UStaticMesh* Mesh, UMaterialInterface* Mat,
		const TArray<FSP>& Scalars, const TArray<FVP>& Vectors,
		FName PulseParam = NAME_None, float PulseAmp = 0.0f);
	/** M_Route_Glow 软光 (径向/条带遮罩) */
	UInstancedStaticMeshComponent* GlowISM(const FLinearColor& Color, float Intensity, float Power,
		float ScaleU, float ScaleV, float EndFade, bool bPulse, float PulseAmp = 0.18f);
	/** 朝向相机的平面实例: UDir 为平面 U 轴在屏幕内(Y,Z)的方向 */
	void AddPlane(UInstancedStaticMeshComponent* IS, const FVector& Pos, const FVector2D& UDir, float SizeU, float SizeV);

	void BuildBackdrop(int32 Seed);
	void BuildRainbow();
	void BuildBeacons(URunSubsystem* Run, const FRouteGraph& Graph);
	void BuildPostProcess();
	void BuildLighting(int32 RunSeed);

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> ISMComps;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> SpawnedActors;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> ISMCache;

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MatGlow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MatNebula;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MatRainbow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MatRays;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MatHalo;

	TWeakObjectPtr<URunSubsystem> CachedRun;
	FVector2D ViewSize = FVector2D(1920.0, 1080.0);
	FVector2D BuiltViewSize = FVector2D::ZeroVector;
	float ResizeStableTime = 0.0f;
	bool bHasMapLayout = false;
	FVector2D MapOriginPx = FVector2D::ZeroVector;
	float MapScalePx = 1.0f;
	float Time = 0.0f;

	TArray<FVector2D> NodeCenters;
	TArray<FVector> Anchors;

	struct FPulse
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		FName Param;
		float Base = 1.0f;
		float Amp = 0.2f;
		float Speed = 2.0f;
		float Phase = 0.0f;
	};
	TArray<FPulse> Pulses;
};
