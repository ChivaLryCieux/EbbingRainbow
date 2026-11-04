// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RouteSceneBuilder.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Run/RouteCameraPawn.h"
#include "Run/RunSubsystem.h"
#include "Run/RunTypes.h"

namespace
{
	// 与 SRouteMapOverlay 同一份布局常数, 保证 3D 平台落在按钮正下方
	constexpr float CanvasW = 2040.0f;
	constexpr float CanvasH = 920.0f;
	constexpr float MarginX = 150.0f;
	constexpr float MarginY = 84.0f;
	constexpr float PadTop = 88.0f;
	constexpr float PadBottom = 58.0f;
	constexpr float NodeW = 100.0f;
	constexpr float NodeH = 112.0f;
	constexpr float SceneDepth = 9000.0f;   // 平台平面到相机的垂直距离 (cm)
}

ARouteSceneBuilder::ARouteSceneBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

UMaterialInterface* ARouteSceneBuilder::LoadMat(const TCHAR* Path)
{
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!M)
	{
		M = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	return M;
}

UMaterialInstanceDynamic* ARouteSceneBuilder::MakeGlowMID(const FLinearColor& Color, float EmissiveMul)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MatGlowBase, this);
	MID->SetVectorParameterValue(TEXT("ColorA"), Color * EmissiveMul);
	MID->SetVectorParameterValue(TEXT("ColorB"), Color * EmissiveMul);
	// 材质自发光强度很高, 低亮度用途要显式压住
	MID->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveMul);
	GlowMIDs.Add(MID);
	return MID;
}

UInstancedStaticMeshComponent* ARouteSceneBuilder::MakeISM(UStaticMesh* Mesh, UMaterialInterface* Mat)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetupAttachment(GetRootComponent());
	C->RegisterComponent();
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(false);
	if (Mat) { C->SetMaterial(0, Mat); }
	ISMComps.Add(C);
	return C;
}

void ARouteSceneBuilder::ResetRouteCamera()
{
	for (TActorIterator<ARouteCameraPawn> It(GetWorld()); It; ++It)
	{
		It->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetControlRotation(FRotator::ZeroRotator);
	}
}

FVector2D ARouteSceneBuilder::CanvasToScreen(const FVector2D& CanvasPos, const FVector2D& ViewSize) const
{
	const float AvailW = ViewSize.X;
	const float AvailH = ViewSize.Y - PadTop - PadBottom;
	const float Scale = FMath::Min(AvailW / CanvasW, AvailH / CanvasH);
	const float DrawW = CanvasW * Scale, DrawH = CanvasH * Scale;
	const float OX = (AvailW - DrawW) * 0.5f;
	const float OY = PadTop + (AvailH - DrawH) * 0.5f;
	return FVector2D(OX + CanvasPos.X * Scale, OY + CanvasPos.Y * Scale);
}

bool ARouteSceneBuilder::ScreenToPlane(const FVector2D& ScreenPx, float Depth, FVector& OutPos) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) { return false; }
	FVector Origin, Dir;
	if (!UGameplayStatics::DeprojectScreenToWorld(PC, ScreenPx, Origin, Dir))
	{
		return false;
	}
	const FVector Fwd = PC->GetControlRotation().Vector();
	const float Denom = FVector::DotProduct(Dir, Fwd);
	if (FMath::Abs(Denom) < 1e-4f) { return false; }
	OutPos = Origin + Dir * (Depth / Denom);
	return true;
}

void ARouteSceneBuilder::BuildScene(URunSubsystem* Run)
{
	if (!Run || !Run->HasActiveRun()) { return; }

	// ---- 资产 ----
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	MatConcrete = LoadMat(TEXT("/Game/Materials/Architecture/MI_Concrete_1.MI_Concrete_1"));
	MatMarble = LoadMat(TEXT("/Game/Materials/Architecture/MI_Marble_1.MI_Marble_1"));
	MatGlowBase = LoadMat(TEXT("/Game/Voxel/Materials/M_Voxel_Glow.M_Voxel_Glow"));
	if (!CubeMesh) { return; }

	ResetRouteCamera();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	FVector2D ViewSize(1600.0f, 900.0f);
	if (PC) { int32 VX = 0, VY = 0; PC->GetViewportSize(VX, VY); ViewSize = FVector2D(VX, VY); }

	const FRouteGraph& Graph = Run->GetGraph();
	const int32 CurrentId = Run->GetCurrentNodeId();

	// ---- 每节点的画布中心与世界锚点 ----
	const float DeltaX = (CanvasW - 2.0f * MarginX) / FMath::Max(1, Graph.NumLayers - 1);
	const float DeltaY = (CanvasH - 2.0f * MarginY) / FMath::Max(1, Graph.NumSlots - 1);
	TArray<FVector2D> CanvasCenters;
	TArray<FVector> Anchors;
	CanvasCenters.SetNum(Graph.Nodes.Num());
	Anchors.SetNum(Graph.Nodes.Num());
	for (const FRouteNode& Node : Graph.Nodes)
	{
		const FVector2D Center(MarginX + Node.Layer * DeltaX, MarginY + Node.Slot * DeltaY);
		CanvasCenters[Node.Id] = Center;

		// 画布坐标取按钮底缘再往下 24px: 平台悬在按钮正下方
		const FVector2D CanvasPos = Center + FVector2D(0.0f, NodeH * 0.5f + 24.0f);
		const FVector2D ScreenPx = CanvasToScreen(CanvasPos, ViewSize);
		FVector P;
		if (!ScreenToPlane(ScreenPx, SceneDepth, P))
		{
			P = FVector(SceneDepth,
				(CanvasPos.X - CanvasW * 0.5f) * 8.0f,
				-(CanvasPos.Y - CanvasH * 0.5f) * 8.0f);
		}
		Anchors[Node.Id] = P;
	}

	// ---- 类型色辉光材质 (类型表 -> 同色 MID) ----
	// 信标用类型色: 强度压到不 clip 的程度, 保持色相可读 (野=绿 / 战=橙红 / 安=青)
	UMaterialInstanceDynamic* MIDSafe  = MakeGlowMID(GetRouteNodeTypeInfo(ERouteNodeType::Safe).Color, 0.75f);
	UMaterialInstanceDynamic* MIDVox   = MakeGlowMID(GetRouteNodeTypeInfo(ERouteNodeType::Battle).Color, 0.85f);
	UMaterialInstanceDynamic* MIDNat   = MakeGlowMID(GetRouteNodeTypeInfo(ERouteNodeType::BattleNature).Color, 0.9f);
	UMaterialInstanceDynamic* MIDCur   = MakeGlowMID(FLinearColor(1.0f, 0.92f, 0.68f), 1.35f);
	UMaterialInstanceDynamic* MIDBeam  = MakeGlowMID(FLinearColor(0.35f, 0.55f, 0.95f), 0.40f);
	UMaterialInstanceDynamic* MIDStar  = MakeGlowMID(FLinearColor(0.75f, 0.85f, 1.0f), 0.7f);
	UMaterialInstanceDynamic* MIDDark  = MakeGlowMID(FLinearColor(0.05f, 0.065f, 0.11f), 0.06f);

	UInstancedStaticMeshComponent* ConIS  = MakeISM(CubeMesh, MatConcrete);
	UInstancedStaticMeshComponent* MarIS  = MakeISM(CubeMesh, MatMarble);
	UInstancedStaticMeshComponent* GlowS  = MakeISM(CubeMesh, MIDSafe);
	UInstancedStaticMeshComponent* GlowV  = MakeISM(CubeMesh, MIDVox);
	UInstancedStaticMeshComponent* GlowN  = MakeISM(CubeMesh, MIDNat);
	UInstancedStaticMeshComponent* GlowC  = MakeISM(CubeMesh, MIDCur);
	UInstancedStaticMeshComponent* BeamIS = MakeISM(CubeMesh, MIDBeam);
	UInstancedStaticMeshComponent* StarIS = MakeISM(CubeMesh, MIDStar);
	UInstancedStaticMeshComponent* DarkIS = MakeISM(CubeMesh, MIDDark);
	// 剪影块走通道1: 主/补/天光都只打通道0 => 恒为无光剪影 (雾仍按像素生效)
	DarkIS->SetLightingChannels(false, true, false);

	// ---- 边: 沿 Slate 同一条三次贝塞尔布辉光小段 (画布空间采样 -> 世界) ----
	for (const FRouteNode& Node : Graph.Nodes)
	{
		for (const int32 NextId : Node.Next)
		{
			if (!CanvasCenters.IsValidIndex(NextId)) { continue; }
			const FVector2D S = CanvasCenters[Node.Id], E = CanvasCenters[NextId];
			const double DX2 = E.X - S.X;
			const FVector2D C1(S.X + DX2 * 0.45, S.Y);
			const FVector2D C2(E.X - DX2 * 0.45, E.Y);
			FVector Prev;
			for (int32 Seg = 0; Seg <= 14; ++Seg)
			{
				const double T = (double)Seg / 14.0, U = 1.0 - T;
				const double W0 = U * U * U, W1 = 3.0 * U * U * T,
					W2 = 3.0 * U * T * T, W3 = T * T * T;
				const FVector2D Pt(
					S.X * W0 + C1.X * W1 + C2.X * W2 + E.X * W3,
					S.Y * W0 + C1.Y * W1 + C2.Y * W2 + E.Y * W3);
				FVector W;
				if (!ScreenToPlane(CanvasToScreen(Pt, ViewSize), SceneDepth, W)) { break; }
				W.Z -= 55.0f;   // 梁悬在按钮线下方, 与平台齐平
				if (Seg > 0)
				{
					const FVector D = W - Prev;
					const float Len = D.Size();
					if (Len > 1.0f)
					{
						BeamIS->AddInstance(FTransform(
							FRotationMatrix::MakeFromX(D.GetSafeNormal()).Rotator(),
							(W + Prev) * 0.5f, FVector(Len / 100.0f, 0.16f, 0.16f)));
					}
				}
				Prev = W;
			}
		}
	}

	// ---- 平台 ----
	FRandomStream NRng(Graph.Seed ^ 0x5EED);
	for (const FRouteNode& Node : Graph.Nodes)
	{
		const bool bCurrent = (Node.Id == CurrentId);
		const FLinearColor TC = GetRouteNodeTypeInfo(Node.Type).Color;
		UInstancedStaticMeshComponent* GlowIS =
			bCurrent ? GlowC :
			(Node.Type == ERouteNodeType::BattleNature) ? GlowN :
			(Node.Type == ERouteNodeType::Battle) ? GlowV : GlowS;
		BuildNodePlatform(Anchors[Node.Id], Node, bCurrent, Node.bVisited, TC);
		const FVector P = Anchors[Node.Id];
		const float S = (Node.Layer == Graph.NumLayers - 1) ? 1.5f : 1.0f;
		const float Gy = bCurrent ? 1.5f : (Node.bVisited ? 0.7f : 1.0f);
		GlowIS->AddInstance(FTransform(FRotator(0, NRng.FRand() * 20.0f - 10.0f, 0),
			P + FVector(0, 0, 90.0f * S), FVector(1.5f * S * Gy)));
	}
	(void)ConIS; (void)MarIS; (void)StarIS;

	BuildStarsAndMonoliths(Graph.Seed, FVector(SceneDepth * 0.5f, 0, 0));
	BuildLighting(Graph.Seed);

	UE_LOG(LogTemp, Log, TEXT("[Route3D] scene built: nodes=%d edges drawn, stars ok"),
		Graph.Nodes.Num());
}

void ARouteSceneBuilder::BuildNodePlatform(
	const FVector& Pos, const FRouteNode& Node, bool bCurrent, bool bVisited,
	const FLinearColor& TypeColor)
{
	FRandomStream Rng(Node.Id * 7919 + 13);
	const float S = (Node.Layer == 0) ? 1.35f : 1.0f;

	// 顶板 (当前节点用大理石, 其余混凝土) —— 组件在 BuildScene 持有,
	// 这里通过 GetComponentsByClass 取第一个具体/大理石 ISM 太绕;
	// 改成直接在 ISMComps 中按材质区分: 0=concrete 1=marble (MakeISM 顺序约定)
	UInstancedStaticMeshComponent* ConIS = ISMComps.Num() > 0 ? ISMComps[0] : nullptr;
	UInstancedStaticMeshComponent* MarIS = ISMComps.Num() > 1 ? ISMComps[1] : nullptr;
	UInstancedStaticMeshComponent* SlabIS = bCurrent ? MarIS : ConIS;
	if (!SlabIS) { return; }

	// 顶板: 沿视线方向薄、屏幕方向宽, 微仰角让顶面吃到主光
	SlabIS->AddInstance(FTransform(
		FRotator(10.0f + Rng.FRandRange(-3.0f, 3.0f), Rng.FRandRange(-5.0f, 5.0f), 0),
		Pos, FVector(2.2f, 7.2f * S, 0.8f)));

	// 收分两段, 向下悬出 "漂浮岛" 底 —— 无光剪影材质, 与远景方碑同一语言
	UInstancedStaticMeshComponent* DarkIS =
		ISMComps.IsValidIndex(8) ? ISMComps[8].Get() : ConIS;
	DarkIS->AddInstance(FTransform(
		FRotator(Rng.FRandRange(-4.0f, 4.0f), Rng.FRandRange(-7.0f, 7.0f), Rng.FRandRange(-3.0f, 3.0f)),
		Pos - FVector(-60.0f, 0, 140.0f), FVector(1.7f, 5.0f * S, 1.3f)));
	DarkIS->AddInstance(FTransform(
		FRotator(Rng.FRandRange(-6.0f, 6.0f), Rng.FRandRange(-10.0f, 10.0f), Rng.FRandRange(-4.0f, 4.0f)),
		Pos - FVector(-140.0f, 0, 330.0f), FVector(1.2f, 2.9f * S, 2.0f)));

	// 已访问节点额外压暗顶板 (叠一层混凝土灰板)
	if (bVisited)
	{
		ConIS->AddInstance(FTransform(FRotator(10.0f, 0, 0),
			Pos + FVector(0, 0, 16.0f), FVector(2.0f, 6.6f * S, 0.20f)));
	}
}

void ARouteSceneBuilder::BuildEdge(const FVector& A, const FVector& B)
{
	(void)A; (void)B;   // 集中放置逻辑已内联到 BuildScene, 此函数保留签名备用
}

void ARouteSceneBuilder::BuildStarsAndMonoliths(int32 RunSeed, const FVector& ViewCenter)
{
	FRandomStream Rng(RunSeed ^ 0x57A2);

	UInstancedStaticMeshComponent* StarIS =
		ISMComps.IsValidIndex(7) ? ISMComps[7].Get() : nullptr;
	UInstancedStaticMeshComponent* DarkIS =
		ISMComps.IsValidIndex(8) ? ISMComps[8].Get() : nullptr;
	UInstancedStaticMeshComponent* ConIS =
		ISMComps.IsValidIndex(0) ? ISMComps[0].Get() : nullptr;

	// 星点: 大范围壳层 (避开平台平面附近 ±3m)
	if (StarIS)
	{
		for (int32 K = 0; K < 320; ++K)
		{
			const float X = SceneDepth + Rng.FRandRange(-3500.0f, 9000.0f);
			const float Y = Rng.FRandRange(-16000.0f, 16000.0f);
			const float Z = Rng.FRandRange(-9000.0f, 9000.0f);
			StarIS->AddInstance(FTransform(FRotator::ZeroRotator,
				FVector(X, Y, Z), FVector(Rng.FRandRange(0.05f, 0.22f))));
		}
	}

	// 远景剪影方碑: 暗色无光照材质, 不受灯光/雾影响 = 稳定的虚空剪影
	if (DarkIS)
	{
		for (int32 K = 0; K < 12; ++K)
		{
			const float X = SceneDepth + Rng.FRandRange(4800.0f, 13000.0f);
			const float Y = Rng.FRandRange(-14000.0f, 14000.0f);
			const float Z = Rng.FRandRange(-5500.0f, 7000.0f);
			const FRotator Rot(Rng.FRandRange(-6.0f, 6.0f),
				Rng.FRandRange(-14.0f, 14.0f), Rng.FRandRange(-6.0f, 6.0f));
			DarkIS->AddInstance(FTransform(Rot, FVector(X, Y, Z),
				FVector(Rng.FRandRange(6.0f, 26.0f),
					Rng.FRandRange(3.0f, 10.0f), Rng.FRandRange(4.0f, 24.0f))));
		}
	}
	(void)ViewCenter;
}

void ARouteSceneBuilder::BuildLighting(int32 RunSeed)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	FRandomStream Rng(RunSeed ^ 0x11C4);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	P.Owner = this;

	// 主光: 优先调谐关卡已有的平行光 (避免多主光告警), 没有再生成
	ADirectionalLight* KeyLight = nullptr;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (!KeyLight) { KeyLight = *It; }
		else
		{
			It->GetComponent()->SetIntensity(0.0f);
			It->GetComponent()->SetCastShadows(false);
		}
	}
	if (!KeyLight)
	{
		KeyLight = World->SpawnActor<ADirectionalLight>(
			ADirectionalLight::StaticClass(), FVector::ZeroVector,
			FRotator::ZeroRotator, P);
		if (KeyLight) { SpawnedActors.Add(KeyLight); }
	}
	if (KeyLight)
	{
		KeyLight->SetActorRotation(FRotator(-42.0f, 18.0f, 0.0f));
		UDirectionalLightComponent* LC = KeyLight->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(4.5f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(235, 242, 255)));
		LC->SetCastShadows(true);
		LC->ForwardShadingPriority = 1;
		LC->SetLightingChannels(true, false, false);
	}
	// 补光: 从左后下方打, 阴影不死黑
	if (ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), FVector::ZeroVector,
		FRotator(-8.0f, 205.0f, 0.0f), P))
	{
		UDirectionalLightComponent* LC = Fill->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(0.7f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(105, 125, 165)));
		LC->SetCastShadows(false);
		LC->SetLightingChannels(true, false, false);
		SpawnedActors.Add(Fill);
	}
	// 环境: 冷天光压暗
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P))
	{
		USkyLightComponent* SC = Sky->GetLightComponent();
		SC->SetMobility(EComponentMobility::Movable);
		SC->SetIntensity(0.35f);
		SC->SetLightColor(FLinearColor::FromSRGBColor(FColor(70, 82, 110)));
		SC->bLowerHemisphereIsBlack = true;
		SpawnedActors.Add(Sky);
	}
	// 虚空雾: 近黑蓝, 远处融进虚空
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(
		AExponentialHeightFog::StaticClass(), FVector::ZeroVector,
		FRotator::ZeroRotator, P))
	{
		UExponentialHeightFogComponent* FC = Fog->GetComponent();
		FC->SetFogDensity(0.012f);
		FC->SetFogHeightFalloff(0.6f);
		FC->SetFogInscatteringColor(FLinearColor::FromSRGBColor(FColor(8, 11, 20)));
		FC->SetStartDistance(2200.0f);
		FC->SetVolumetricFog(false);
		SpawnedActors.Add(Fog);
	}
}
