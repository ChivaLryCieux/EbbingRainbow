// Copyright Epic Games, Inc. All Rights Reserved.

#include "Run/RouteSceneBuilder.h"
#include "Core/ERDebug.h"
#include "Core/ERLog.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Run/RouteCameraPawn.h"
#include "Run/RouteHUD.h"
#include "Run/RouteLayout.h"
#include "Run/RunSubsystem.h"
#include "Run/RunTypes.h"

namespace
{
	/** 节点/连线所在平面到相机的垂直距离 (cm) */
	constexpr float SceneDepth = 9000.0f;

	/** 平面实例朝 -X (面向相机); UDir 是平面局部 X(U) 轴在屏幕内 (Y,Z) 的方向 */
	FQuat PlaneQuat(const FVector2D& UDirYZ)
	{
		const FVector2D U = UDirYZ.GetSafeNormal();
		return FRotationMatrix::MakeFromZX(FVector(-1.0, 0.0, 0.0), FVector(0.0, U.X, U.Y)).ToQuat();
	}

	template <typename T>
	T* LoadAsset(const TCHAR* Path) { return LoadObject<T>(nullptr, Path); }
}

ARouteSceneBuilder::ARouteSceneBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

// ------------------------------------------------------------------ 基础设施

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

UInstancedStaticMeshComponent* ARouteSceneBuilder::MatISM(UStaticMesh* Mesh, UMaterialInterface* Mat,
	const TArray<FSP>& Scalars, const TArray<FVP>& Vectors, FName PulseParam, float PulseAmp)
{
	FString Key = FString::Printf(TEXT("%s|%s|%s|%.3f"), *GetNameSafe(Mesh), *GetNameSafe(Mat), *PulseParam.ToString(), PulseAmp);
	for (const FSP& S : Scalars) { Key += FString::Printf(TEXT("|%s=%.4f"), *S.Name.ToString(), S.Value); }
	for (const FVP& V : Vectors) { Key += FString::Printf(TEXT("|%s=%.3f,%.3f,%.3f"), *V.Name.ToString(), V.Value.R, V.Value.G, V.Value.B); }
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Found = ISMCache.Find(Key))
	{
		return *Found;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
	for (const FSP& S : Scalars) { MID->SetScalarParameterValue(S.Name, S.Value); }
	for (const FVP& V : Vectors) { MID->SetVectorParameterValue(V.Name, V.Value); }
	MIDs.Add(MID);

	if (!PulseParam.IsNone())
	{
		float Base = 1.0f;
		for (const FSP& S : Scalars) { if (S.Name == PulseParam) { Base = S.Value; } }
		FPulse P;
		P.MID = MID;
		P.Param = PulseParam;
		P.Base = Base;
		P.Amp = PulseAmp;
		P.Speed = 1.5f + 0.37f * (Pulses.Num() % 5);
		P.Phase = Pulses.Num() * 1.3f;
		Pulses.Add(P);
	}

	UInstancedStaticMeshComponent* IS = MakeISM(Mesh, MID);
	ISMCache.Add(Key, IS);
	return IS;
}

UInstancedStaticMeshComponent* ARouteSceneBuilder::GlowISM(const FLinearColor& Color, float Intensity, float Power,
	float ScaleU, float ScaleV, float EndFade, bool bPulse, float PulseAmp)
{
	return MatISM(PlaneMesh, MatGlow,
		{ {"Intensity", Intensity}, {"Power", Power}, {"ScaleU", ScaleU}, {"ScaleV", ScaleV}, {"EndFade", EndFade} },
		{ {"Color", Color} },
		bPulse ? FName("Intensity") : NAME_None, PulseAmp);
}

void ARouteSceneBuilder::AddPlane(UInstancedStaticMeshComponent* IS, const FVector& Pos, const FVector2D& UDir,
	float SizeU, float SizeV)
{
	if (!IS) { return; }
	IS->AddInstance(FTransform(PlaneQuat(UDir), Pos, FVector(SizeU / 100.0f, SizeV / 100.0f, 1.0f)), true);
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
		// 反投影读的是相机管理器缓存的视点 (上一帧 = PlayerStart 位置); 不立刻刷新, 所有屏幕对齐都会偏一个相机高度
		if (PC->PlayerCameraManager) { PC->PlayerCameraManager->UpdateCamera(0.0f); }
	}
}

FVector2D ARouteSceneBuilder::GetViewSize() const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		int32 VX = 0, VY = 0;
		PC->GetViewportSize(VX, VY);
		if (VX > 0 && VY > 0) { return FVector2D(VX, VY); }
	}
	return FVector2D(1600.0, 900.0);
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

bool ARouteSceneBuilder::PollMapLayout(FVector2D& OutOrigin, float& OutScale) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ARouteHUD* HUD = PC ? Cast<ARouteHUD>(PC->GetHUD()) : nullptr;
	return HUD && HUD->GetMapLayout(OutOrigin, OutScale);
}

bool ARouteSceneBuilder::CanvasToWorld(const FVector2D& CanvasPos, float Depth, FVector& OutPos) const
{
	// 优先用 Slate 实际布局结果对齐 (公式推导的位置会和 Slate 的 Overlay 对齐规则差十几像素)
	const FVector2D Screen = bHasMapLayout
		? MapOriginPx + CanvasPos * MapScalePx
		: ERRouteLayout::CanvasToScreen(CanvasPos, ViewSize);
	if (ScreenToPlane(Screen, Depth, OutPos))
	{
		return true;
	}
	OutPos = FVector(Depth,
		(CanvasPos.X - ERRouteLayout::MapW * 0.5f) * 8.0f,
		-(CanvasPos.Y - ERRouteLayout::MapH * 0.5f) * 8.0f);
	return false;
}

FVector2D ARouteSceneBuilder::FrustumHalf(float Depth) const
{
	FVector A, B;
	if (ScreenToPlane(FVector2D(0.0, 0.0), Depth, A) && ScreenToPlane(ViewSize, Depth, B))
	{
		return FVector2D(FMath::Abs(B.Y - A.Y) * 0.5f, FMath::Abs(B.Z - A.Z) * 0.5f);
	}
	return FVector2D(Depth, Depth * 0.5625f);
}

float ARouteSceneBuilder::WorldPerPx(float Depth) const
{
	return FrustumHalf(Depth).X * 2.0f / FMath::Max(1.0f, (float)ViewSize.X);
}

float ARouteSceneBuilder::CanvasScalePx() const
{
	return bHasMapLayout ? MapScalePx : ERRouteLayout::CanvasScale(ViewSize);
}

void ARouteSceneBuilder::ClearScene()
{
	for (UInstancedStaticMeshComponent* C : ISMComps)
	{
		if (C) { C->DestroyComponent(); }
	}
	for (AActor* A : SpawnedActors)
	{
		if (A) { A->Destroy(); }
	}
	ISMComps.Reset();
	SpawnedActors.Reset();
	MIDs.Reset();
	ISMCache.Reset();
	Pulses.Reset();
	NodeCenters.Reset();
	Anchors.Reset();
}

// ------------------------------------------------------------------ 总装

void ARouteSceneBuilder::BuildScene(URunSubsystem* Run)
{
	if (!Run || !Run->HasActiveRun()) { return; }
	CachedRun = Run;
	ClearScene();

	CubeMesh = LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PlaneMesh = LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane"));
	MatGlow = LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Glow.M_Route_Glow"));
	MatNebula = LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Nebula.M_Route_Nebula"));
	MatRainbow = LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Rainbow.M_Route_Rainbow"));
	MatRays = LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Rays.M_Route_Rays"));
	MatHalo = LoadAsset<UMaterialInterface>(TEXT("/Game/Run/Materials/M_Route_Halo.M_Route_Halo"));
	if (!CubeMesh || !PlaneMesh || !MatGlow || !MatNebula || !MatRainbow || !MatRays || !MatHalo)
	{
		UE_LOG(LogERRun, Warning, TEXT("[Route3D] 缺少 /Game/Run/Materials 资产 (Scripts/route_make_materials*.py 生成), 3D 航线场景跳过"));
		return;
	}

	ResetRouteCamera();
	ViewSize = GetViewSize();
	BuiltViewSize = ViewSize;
	ResizeStableTime = 0.0f;
	bHasMapLayout = PollMapLayout(MapOriginPx, MapScalePx);

	const FRouteGraph& Graph = Run->GetGraph();

	NodeCenters.SetNum(Graph.Nodes.Num());
	Anchors.SetNum(Graph.Nodes.Num());
	for (const FRouteNode& Node : Graph.Nodes)
	{
		NodeCenters[Node.Id] = ERRouteLayout::NodeCenter(Graph, Node);
		CanvasToWorld(NodeCenters[Node.Id], SceneDepth + 60.0f, Anchors[Node.Id]);
	}

	BuildBackdrop(Graph.Seed);
	BuildRainbow();
	BuildBeacons(Run, Graph);
	BuildPostProcess();
	BuildLighting(Graph.Seed);

	UE_LOG(LogERRun, Log, TEXT("[Route3D] scene built: nodes=%d ism=%d view=%.0fx%.0f layout=%s"),
		Graph.Nodes.Num(), ISMComps.Num(), ViewSize.X, ViewSize.Y, bHasMapLayout ? TEXT("slate") : TEXT("formula"));
}

void ARouteSceneBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ISMComps.Num() == 0) { return; }
	Time += DeltaSeconds;

	// 视口尺寸/Slate 画布位置变了 (PIE 窗口缩放/全屏切换/首帧布局完成): 屏幕反投影对齐会失效, 稳定 0.3s 后整场重建
	FVector2D PolledOrigin = FVector2D::ZeroVector;
	float PolledScale = 1.0f;
	const bool bPolled = PollMapLayout(PolledOrigin, PolledScale);
	const bool bLayoutChanged = bPolled != bHasMapLayout
		|| (bPolled && (!PolledOrigin.Equals(MapOriginPx, 0.5) || !FMath::IsNearlyEqual(PolledScale, MapScalePx, 0.0005f)));
	if (!GetViewSize().Equals(BuiltViewSize, 1.0) || bLayoutChanged)
	{
		ResizeStableTime += DeltaSeconds;
		if (ResizeStableTime > 0.3f && CachedRun.IsValid())
		{
			BuildScene(CachedRun.Get());
		}
		return;
	}
	ResizeStableTime = 0.0f;

	for (const FPulse& P : Pulses)
	{
		if (UMaterialInstanceDynamic* M = P.MID.Get())
		{
			M->SetScalarParameterValue(P.Param, P.Base * (1.0f + P.Amp * FMath::Sin(Time * P.Speed + P.Phase)));
		}
	}
}

// ------------------------------------------------------------------ 背景

void ARouteSceneBuilder::BuildBackdrop(int32 Seed)
{
	const float D = 26000.0f;
	const FVector2D H = FrustumHalf(D);
	FRandomStream Rng(Seed ^ 0x9E3B);

	auto Cloud = [&](float YF, float ZF, float SU, float SV, const FLinearColor& A, const FLinearColor& B,
		float I, float Pow, float NoiseScale)
	{
		UInstancedStaticMeshComponent* IS = MatISM(PlaneMesh, MatNebula,
			{ {"Intensity", I}, {"Power", Pow}, {"NoiseScale", NoiseScale}, {"Seed", (float)Rng.FRandRange(0.0f, 40.0f)},
			  {"SpeedX", 0.006f}, {"SpeedY", 0.003f}, {"Contrast", 2.0f}, {"CloudAmount", 0.9f} },
			{ {"ColorA", A}, {"ColorB", B} });
		AddPlane(IS, FVector(D + YF * 3.0f, YF * H.X, ZF * H.Y), FVector2D(1.0, 0.0), SU * H.X * 2.0f, SV * H.Y * 2.0f);
	};

	// 底: 冷蓝 -> 靛紫的地平云海
	Cloud(0.00f, -0.62f, 1.60f, 1.00f, FLinearColor(0.04f, 0.13f, 0.34f), FLinearColor(0.15f, 0.12f, 0.40f), 0.17f, 1.5f, 2.6f);
	// 左上: 青绿冷雾 (起点侧偏冷)
	Cloud(-0.70f, 0.55f, 1.10f, 0.95f, FLinearColor(0.04f, 0.26f, 0.38f), FLinearColor(0.12f, 0.18f, 0.48f), 0.09f, 1.8f, 3.2f);
	// 右侧: 金色暖云 (终点侧偏暖, 神性)
	Cloud(0.92f, 0.02f, 0.95f, 1.30f, FLinearColor(1.00f, 0.60f, 0.24f), FLinearColor(0.95f, 0.34f, 0.28f), 0.15f, 1.7f, 2.4f);
	// 画面中心极淡的冷白云, 让节点区域有"光源"
	Cloud(0.00f, 0.12f, 0.95f, 0.85f, FLinearColor(0.30f, 0.52f, 0.92f), FLinearColor(0.50f, 0.62f, 1.00f), 0.07f, 2.0f, 3.6f);
}

void ARouteSceneBuilder::BuildRainbow()
{
	const float D = 31000.0f;
	const FVector2D H = FrustumHalf(D);
	// 弧心在画面下方 1.35 个半高处; v 轴向下: v = 0.5 - Z/(2*Hz)
	const float CenterV = 0.5f + 1.35f * 0.5f;
	UInstancedStaticMeshComponent* IS = MatISM(PlaneMesh, MatRainbow,
		{ {"Intensity", 0.15f}, {"Aspect", (float)(H.X / H.Y)}, {"CenterV", CenterV}, {"Radius", 1.075f},
		  {"Width", 0.15f}, {"ArcHalf", 1.25f}, {"Pastel", 0.28f}, {"Softness", 1.3f} },
		{ {"Tint", FLinearColor(1.0f, 0.97f, 0.9f)} });
	AddPlane(IS, FVector(D, 0.0f, 0.0f), FVector2D(1.0, 0.0), H.X * 2.04f, H.Y * 2.04f);
}

// ------------------------------------------------------------------ 当前节点圣光 (全场唯一的发光源)

void ARouteSceneBuilder::BuildBeacons(URunSubsystem* Run, const FRouteGraph& Graph)
{
	const FRouteNode* Current = Run->GetCurrentNode();
	if (!Current || !Anchors.IsValidIndex(Current->Id)) { return; }

	const float Px = WorldPerPx(SceneDepth);
	const FLinearColor Gold = ERRouteLayout::GoldColor();
	const FVector P = Anchors[Current->Id];
	const bool bFinal = Current->Layer == Graph.NumLayers - 1;
	const float SigilPx = 34.0f * (bFinal ? 1.22f : 1.0f) * CanvasScalePx();   // 屏幕像素里的徽章半径

	// 神性光环: 柔光 + 细环 + 缓慢旋转的射线 (射线避开中心, 不压 Slate 徽章)
	const float SizePx = 430.0f;
	const float Half = SizePx * 0.5f;
	// 旋转与呼吸都在 Shader 里用 GPU Time 驱动 (不再逐帧改 MID 参数), 射线宽而软, 避免细射线转动时的闪烁/爬行
	UInstancedStaticMeshComponent* Halo = MatISM(PlaneMesh, MatHalo,
		{ {"Intensity", 0.85f}, {"GlowPower", 3.4f}, {"RingRadius", SigilPx * 1.65f / Half}, {"RingWidth", 0.010f},
		  {"RingIntensity", 0.45f}, {"RayCount", 10.0f}, {"RayIntensity", 0.50f},
		  {"RayPower", 3.0f}, {"Spin", 0.10f}, {"BreathSpeed", 0.9f}, {"BreathAmount", 0.10f},
		  {"HoleInner", SigilPx * 1.15f / Half}, {"HoleOuter", SigilPx * 2.0f / Half} },
		{ {"Color", Gold} });	AddPlane(Halo, P, FVector2D(1, 0), SizePx * Px, SizePx * Px);

	// 向上的一道金色圣光柱 (噪声条纹, 淡出两端)
	const float Len = 900.0f * Px;
	UInstancedStaticMeshComponent* Pillar = MatISM(PlaneMesh, MatRays,
		{ {"Intensity", 0.10f}, {"Power", 1.6f}, {"StreakFreq", 0.8f}, {"Speed", 0.0f} },
		{ {"Color", Gold} });
	AddPlane(Pillar, P + FVector(10.0f, 0, Len * 0.48f), FVector2D(1, 0), 110.0f * Px, Len);
}
// ------------------------------------------------------------------ 后处理 / 灯光

void ARouteSceneBuilder::BuildPostProcess()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// 关卡自己摆了无界后处理就尊重它 (Painterly 风格体积是 Transient 的, 不算)
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (!It->HasAnyFlags(RF_Transient) && It->bUnbound) { return; }
	}

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	P.Owner = this;
	APostProcessVolume* V = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, P);
	if (!V) { return; }
	SpawnedActors.Add(V);

	V->bUnbound = true;
	V->bEnabled = true;
	V->Priority = 0.0f;
	V->BlendWeight = 1.0f;

	FPostProcessSettings& S = V->Settings;
	// 固定曝光: 场景几乎全是自发光, 自动曝光会把虚空提亮成灰
	S.bOverride_AutoExposureMinBrightness = true;  S.AutoExposureMinBrightness = 0.0f;
	S.bOverride_AutoExposureMaxBrightness = true;  S.AutoExposureMaxBrightness = 0.0f;
	S.bOverride_AutoExposureBias = true;           S.AutoExposureBias = 0.0f;
	S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = 1.0f;
	S.bOverride_LocalExposureShadowContrastScale = true;    S.LocalExposureShadowContrastScale = 1.0f;
	S.bOverride_BloomIntensity = true;             S.BloomIntensity = 1.1f;
	S.bOverride_BloomThreshold = true;             S.BloomThreshold = 0.4f;
	S.bOverride_VignetteIntensity = true;          S.VignetteIntensity = 0.55f;
}

void ARouteSceneBuilder::BuildLighting(int32 RunSeed)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	// 航线场景全部自发光, 默认不需要灯光 (与竞技场一致: 关卡自摆光照归用户手调);
	// -Atmo 显式开启程序氛围
	if (!ER::Debug::IsAtmoEnabled())
	{
		return;
	}
	FRandomStream Rng(RunSeed ^ 0x11C4);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	P.Owner = this;

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



