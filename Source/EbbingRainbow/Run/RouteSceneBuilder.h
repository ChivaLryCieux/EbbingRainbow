// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RouteSceneBuilder.generated.h"

class URunSubsystem;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 航线图 3D 场景搭建器 (RouteGameMode 在 StartPlay 里生成并调用 BuildScene)。
 *
 * 交互/逻辑仍由 Slate 叠加层负责, 本层只做视觉:
 * - 把每个节点的 Slate 画布坐标反投影到相机前方的世界平面,
 *   在节点按钮正下方生成一座漂浮体素平台 (顶板+收分岩芯+类型色辉光核)。
 * - 节点连线用纤细辉光梁。
 * - 远处撒星点与巨型剪影方碑, 配同一套虚空雾/三点光, 与竞技场语言一致。
 *
 * 同一张 Run 图 -> 同一场景 (一切随机量都从 RunSeed 派生)。
 */
UCLASS()
class EBBINGRAINBOW_API ARouteSceneBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARouteSceneBuilder();

	/** 按当前 Run 图搭建整个场景 (幂等: 重复调用先清空) */
	void BuildScene(URunSubsystem* Run);

private:
	/** 相机归位: 路线相机拉到原点朝 +X, 场景全部在它的前方 */
	void ResetRouteCamera();
	/** 节点画布坐标(2040x920 虚拟像素) -> 屏幕像素 */
	FVector2D CanvasToScreen(const FVector2D& CanvasPos, const FVector2D& ViewSize) const;
	/** 屏幕像素 -> 相机前方 Depth 处的世界点 (所有点共面, 与屏幕严格对齐) */
	bool ScreenToPlane(const FVector2D& ScreenPx, float Depth, FVector& OutPos) const;

	UInstancedStaticMeshComponent* MakeISM(UStaticMesh* Mesh, UMaterialInterface* Mat);
	UMaterialInterface* LoadMat(const TCHAR* Path);
	UMaterialInstanceDynamic* MakeGlowMID(const FLinearColor& Color, float EmissiveMul);

	/** 单节点平台簇 (顶板/收分/辉光核) */
	void BuildNodePlatform(const FVector& Pos, const struct FRouteNode& Node,
		bool bCurrent, bool bVisited, const FLinearColor& TypeColor);
	void BuildEdge(const FVector& A, const FVector& B);
	void BuildStarsAndMonoliths(int32 RunSeed, const FVector& ViewCenter);
	void BuildLighting(int32 RunSeed);

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> ISMComps;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> SpawnedActors;

	TObjectPtr<UStaticMesh> CubeMesh;
	TObjectPtr<UMaterialInterface> MatConcrete;
	TObjectPtr<UMaterialInterface> MatMarble;
	TObjectPtr<UMaterialInterface> MatGlowBase;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;
};
