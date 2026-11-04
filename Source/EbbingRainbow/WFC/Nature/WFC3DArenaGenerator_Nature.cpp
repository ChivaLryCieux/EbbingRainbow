// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/WFC3DArenaGenerator_Nature.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Misc/CommandLine.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "NavigationSystem.h"

TArray<uint16> AWFC3DArenaGenerator_Nature::CachedHeight;
bool AWFC3DArenaGenerator_Nature::bHeightLoadAttempted = false;

namespace
{
	/** 平滑阶跃: t 在 [a,b] 内从 0 到 1 (cubic) */
	float NatSmooth(float A, float B, float T)
	{
		const float X = FMath::Clamp((T - A) / FMath::Max(B - A, 1e-4f), 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}

	/** PerlinNoise2D 双参包装 (UE5.8 只收 FVector2D) */
	float P2(float U, float V)
	{
		return FMath::PerlinNoise2D(FVector2D(U, V));
	}
}

AWFC3DArenaGenerator_Nature::AWFC3DArenaGenerator_Nature()
{
	PrimaryActorTick.bCanEverTick = false;

	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	SetRootComponent(Terrain);
	// bUseComplexAsSimpleCollision=true 会让 nav 几何导出走 BodySetup 之外的路径,
	// ProceduralMeshComponent 会因此完全不进 navmesh; 必须保持 false,
	// 由 CreateMeshSection(bCreateCollision) 烘焙的三模 BodySetup 供 Recast 使用。
	Terrain->bUseComplexAsSimpleCollision = false;
	Terrain->bUseAsyncCooking = false;   // 同步烘焙碰撞, nav 重建必须立刻看到
	Terrain->SetCanEverAffectNavigation(true);
}

bool AWFC3DArenaGenerator_Nature::LoadHeightmap()
{
	if (bHeightLoadAttempted)
	{
		return HasHeight();
	}
	bHeightLoadAttempted = true;

	const FString Path = FPaths::ProjectContentDir() / TEXT("Nature/E-R-Terrain.u16");
	TArray<uint8> Raw;
	if (!FFileHelper::LoadFileToArray(Raw, *Path) || Raw.Num() < 4033 * 4033 * 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Nature] heightmap missing/short: %s -> pure Perlin fallback"), *Path);
		return false;
	}
	CachedHeight.SetNumUninitialized(4033 * 4033);
	FMemory::Memcpy(CachedHeight.GetData(), Raw.GetData(), 4033 * 4033 * 2);
	UE_LOG(LogTemp, Log, TEXT("[Nature] heightmap loaded: %d samples"), CachedHeight.Num());
	return true;
}

float AWFC3DArenaGenerator_Nature::OutlineRadius(float AngleRad, float Phase) const
{
	// 低频轮廓扰动: 圆环上取 Perlin2D -> 有机岛形, 幅度 ~30%
	const float N = P2(
		Phase + FMath::Cos(AngleRad) * 0.9f,
		Phase * 1.7f + FMath::Sin(AngleRad) * 0.9f);
	const float N2 = P2(
		Phase * 2.3f + FMath::Cos(AngleRad) * 2.4f,
		Phase + FMath::Sin(AngleRad) * 2.4f);
	return IslandR0 * (0.80f + 0.22f * N + 0.08f * N2);
}

void AWFC3DArenaGenerator_Nature::BuildField(int32 Seed, FNatField& F) const
{
	const int32 N = GridN;
	const bool bForceFlat = FParse::Param(FCommandLine::Get(), TEXT("NatureFlat"));
	F.H.SetNumUninitialized(N * N);
	F.Valid.SetNumUninitialized(N * N);
	F.PlateauH = 0.0f;
	F.MaxRadius = 0.0f;

	FRandomStream Rng(Seed);

	// ---- 1. Gaea 截取窗口 + 旋转/镜像 -> 细节源 ----
	const int32 Pad = 20;                        // 模糊半径 + 边界余量
	const int32 Win = N + Pad * 2;               // 截取窗口 (px)
	const bool bHM = LoadHeightmap();
	const int32 MaxOff = 4033 - Win;
	const int32 Ox = Rng.RandRange(0, MaxOff);
	const int32 Oy = Rng.RandRange(0, MaxOff);
	const int32 Rot = Rng.RandRange(0, 3);
	const bool bMirror = Rng.RandRange(0, 1) == 1;
	UE_LOG(LogTemp, Log, TEXT("[Nature] crop %d,%d rot=%d mirror=%d"), Ox, Oy, Rot, bMirror);

	// field 顶点 (i,j) -> crop 像素 (px,py) 映射 (旋转/镜像作用于 N 网格内部,
	// 像素偏移 Pad 留给模糊边界)
	auto MapToCrop = [&](int32 I, int32 J, int32& PX, int32& PY)
	{
		int32 X = I, Y = J;
		switch (Rot)
		{
		case 1: X = J;          Y = N - 1 - I; break;
		case 2: X = N - 1 - I;  Y = N - 1 - J; break;
		case 3: X = N - 1 - J;  Y = I;         break;
		default: break;
		}
		if (bMirror) { X = N - 1 - X; }
		PX = X + Pad;
		PY = Y + Pad;
	};

	// ---- 2. 高通: crop - boxblur(crop) ----
	// 在 crop 上做可分离盒式模糊 (半径 16px ≈ 8m): 只保留中小尺度细节
	const int32 BR = 16;
	TArray<float> Detail;
	Detail.SetNumZeroed(N * N);
	if (bHM)
	{
		// 水平模糊 -> 垂直模糊, 直接对输出点计算 (N=241, 暴力也够快)
		const TArray<uint16>& HM = CachedHeight;
		auto RawAt = [&](int32 PX, int32 PY) -> float
		{
			PX = FMath::Clamp(PX, 0, Win - 1);
			PY = FMath::Clamp(PY, 0, Win - 1);
			return (float)HM[(Oy + PY) * 4033 + (Ox + PX)];
		};
		// 先算每行水平模糊存入临时, 再垂直模糊
		TArray<float> HB;
		HB.SetNumUninitialized(N * N);
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				int32 PX, PY; MapToCrop(I, J, PX, PY);
				float Acc = 0.0f;
				for (int32 K = -BR; K <= BR; ++K) { Acc += RawAt(PX + K, PY); }
				HB[J * N + I] = Acc / (float)(2 * BR + 1);
			}
		}
		// 垂直模糊需要同映射列 -> 再遍历一次 (crop 行方向)
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				int32 PX, PY; MapToCrop(I, J, PX, PY);
				float Acc = 0.0f;
				for (int32 K = -BR; K <= BR; ++K) { Acc += RawAt(PX, PY + K); }
				const float Blur = (HB[J * N + I] + Acc / (float)(2 * BR + 1)) * 0.5f;
				Detail[J * N + I] = RawAt(PX, PY) - Blur;
			}
		}
	}
	else
	{
		// 兜底: Perlin 细节 (中频 + 高频)
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const float U = I / 24.0f, V = J / 24.0f;
				Detail[J * N + I] =
					9000.0f * P2(U, V) +
					3500.0f * P2(U * 4.0f, V * 4.0f);
			}
		}
	}

	// 归一化细节 -> 目标 RMS ≈ 260cm, 并做边缘增幅 (崖带更破碎)
	double Rms = 0.0;
	for (float D : Detail) { Rms += (double)D * D; }
	Rms = FMath::Sqrt(Rms / Detail.Num());
	const float DetailScale = (Rms > 1.0) ? (260.0f / (float)Rms) : 0.01f;

	// ---- 3. 岛形 + 高度合成 ----
	const float Phase = Rng.FRand() * 37.0f;
	const float P0 = 420.0f + Rng.FRand() * 320.0f;   // 平台基准微扰相位
	const float CX = N * 0.5f, CY = N * 0.5f;

	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const float X = (I - CX) * Step;
			const float Y = (J - CY) * Step;
			const float R = FMath::Sqrt(X * X + Y * Y);
			const float Ang = FMath::Atan2(Y, X);
			const float Rout = OutlineRadius(Ang, Phase);
			F.MaxRadius = FMath::Max(F.MaxRadius, Rout);

			const int32 Idx = J * N + I;
			if (R > Rout * 1.10f)
			{
				F.Valid[Idx] = 0;
				F.H[Idx] = -4000.0f;
				continue;
			}
			F.Valid[Idx] = 1;

			// 平台近纯平: 只有 ±8cm 微观 + 极淡细节 (nav/站位完全可靠)
			float H = 8.0f * P2(P0 * 0.31f + X / 160.0f, P0 * 0.37f + Y / 160.0f)
				+ Detail[Idx] * DetailScale * 0.06f;

			// [诊断] -NatureFlat: 平台绝对归零, 隔离高度场因素
			if (bForceFlat && R < PlateauR) { H = 0.0f; }

			// 缓坡带 (平台缘 -> 岛缘): 丘陵渐起 + 整体下沉 ~1400cm
			const float T = NatSmooth(PlateauR + 100.0f, Rout * 0.92f, R);
			H += T * (240.0f * P2(P0 * 0.013f + X / 5200.0f, P0 * 0.017f + Y / 5200.0f)
				+ FMath::Clamp(Detail[Idx] * DetailScale * 0.55f, -120.0f, 120.0f)
				+ 24.0f * P2(P0 * 0.21f + X / 420.0f, P0 * 0.23f + Y / 420.0f)
				- 1400.0f);

			// 崖裙: 轮廓外侧急速下拉
			H -= NatSmooth(Rout * 0.96f, Rout * 1.08f, R) * 2400.0f;

			F.H[Idx] = H;
		}
	}

	// ---- 4. 坡度松弛: 平台+缓冲带 <=26cm (nav 步高 35 留余量),
	//    缓坡带 <=40cm (≈39°, nav 上限 44°); 24 轮保证收敛 ----
	for (int32 It = 0; It < 24; ++It)
	{
		bool bChanged = false;
		for (int32 J = 0; J < N - 1; ++J)
		{
			for (int32 I = 0; I < N - 1; ++I)
			{
				const float X = (I - CX) * Step;
				const float Y = (J - CY) * Step;
				const float R = FMath::Sqrt(X * X + Y * Y);
				const float Ang = FMath::Atan2(Y, X);
				const float Rout = OutlineRadius(Ang, Phase);
				float MaxD;
				if (R <= PlateauR + 800.0f) { MaxD = 26.0f; }
				else if (R <= Rout * 0.95f)  { MaxD = 40.0f; }
				else { continue; }
				const int32 A = J * N + I, B = J * N + I + 1, C = (J + 1) * N + I;
				auto ClampPair = [&](int32 P, int32 Q, float MD)
				{
					const float D = F.H[Q] - F.H[P];
					if (FMath::Abs(D) > MD)
					{
						const float Mid = (F.H[P] + F.H[Q]) * 0.5f;
						F.H[P] = Mid - FMath::Sign(D) * MD * 0.5f;
						F.H[Q] = Mid + FMath::Sign(D) * MD * 0.5f;
						bChanged = true;
					}
				};
				ClampPair(A, B, MaxD);
				ClampPair(A, C, MaxD);
			}
		}
		if (!bChanged) { break; }
	}
}

bool AWFC3DArenaGenerator_Nature::SampleHeight(
	const FNatField& F, float X, float Y, float& OutH, float& OutSlopeDeg) const
{
	const int32 N = GridN;
	const float GX = X / Step + N * 0.5f;
	const float GY = Y / Step + N * 0.5f;
	const int32 I0 = FMath::FloorToInt(GX), J0 = FMath::FloorToInt(GY);
	if (I0 < 1 || J0 < 1 || I0 >= N - 1 || J0 >= N - 1) { return false; }
	const int32 Idx = J0 * N + I0;
	if (!F.Valid[Idx]) { return false; }
	const float FX = GX - I0, FY = GY - J0;
	const float H00 = F.H[Idx],            H10 = F.H[Idx + 1];
	const float H01 = F.H[Idx + N],        H11 = F.H[Idx + N + 1];
	OutH = FMath::Lerp(FMath::Lerp(H00, H10, FX), FMath::Lerp(H01, H11, FX), FY);
	const float DX = (F.H[Idx + 1] - F.H[Idx - 1]) / (2.0f * Step);
	const float DY = (F.H[Idx + N] - F.H[Idx - N]) / (2.0f * Step);
	OutSlopeDeg = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX * DX + DY * DY)));
	return true;
}

void AWFC3DArenaGenerator_Nature::BuildTerrainMesh(const FNatField& F)
{
	const int32 N = GridN;
	TArray<FVector> Verts;   Verts.SetNumUninitialized(N * N);
	TArray<FVector> Norms;   Norms.SetNumUninitialized(N * N);
	TArray<FVector2D> UVs;   UVs.SetNumUninitialized(N * N);
	TArray<FProcMeshTangent> Tans; Tans.SetNumUninitialized(N * N);
	TArray<int32> Tris;      Tris.Reserve(N * N * 6);

	// 顶点直接放在世界系 (actor 在原点): 岛心 = CenterX/CenterY,
	// 与撒点/探针/导航的 3800 坐标约定一致
	const float OX = CenterX - N * 0.5f * Step, OY = CenterY - N * 0.5f * Step;
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const int32 Idx = J * N + I;
			const float X = OX + I * Step, Y = OY + J * Step;
			Verts[Idx] = FVector(X, Y, F.H[Idx]);
			UVs[Idx] = FVector2D(X / 600.0f, Y / 600.0f);
			Tans[Idx] = FProcMeshTangent(1.0f, 0.0f, 0.0f);

			// 法线: 中心差分 (边缘单向)
			const int32 Im = FMath::Max(I - 1, 0), Ip = FMath::Min(I + 1, N - 1);
			const int32 Jm = FMath::Max(J - 1, 0), Jp = FMath::Min(J + 1, N - 1);
			const float DX = (F.H[J * N + Ip] - F.H[J * N + Im]) / ((Ip - Im) * Step);
			const float DY = (F.H[Jp * N + I] - F.H[Jm * N + I]) / ((Jp - Jm) * Step);
			Norms[Idx] = FVector(-DX, -DY, 1.0f).GetSafeNormal();
		}
	}
	for (int32 J = 0; J < N - 1; ++J)
	{
		for (int32 I = 0; I < N - 1; ++I)
		{
			const int32 A = J * N + I, B = A + 1, C = A + N, D = C + 1;
			if (!F.Valid[A] || !F.Valid[B] || !F.Valid[C] || !F.Valid[D]) { continue; }
			Tris.Add(A); Tris.Add(C); Tris.Add(B);
			Tris.Add(B); Tris.Add(C); Tris.Add(D);
		}
	}

	Terrain->ClearAllMeshSections();
	Terrain->CreateMeshSection(0, Verts, Tris, Norms, UVs,
		TArray<FColor>(), Tans, /*bCreateCollision=*/true);
	Terrain->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Terrain->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// 组件注册时还没有 mesh -> nav 八叉树按空包围盒+空 BodySetup 建档,
	// UpdateComponentInNavOctree 只刷包围盒不足以让它重新取 BodySetup.
	// 网格建好后重注册组件, 让 nav 元素重新初始化 (bounds + trimesh 都补齐)。
	Terrain->UpdateBounds();
	Terrain->UnregisterComponent();
	Terrain->RegisterComponent();
	if (MatGround)
	{
		Terrain->SetMaterial(0, MatGround);
	}
	UE_LOG(LogTemp, Log, TEXT("[Nature] terrain mesh: %d verts %d tris"),
		Verts.Num(), Tris.Num() / 3);
}

void AWFC3DArenaGenerator_Nature::BuildUnderside(const FNatField& F, FRandomStream& Rng)
{
	(void)F; (void)Rng;
	// 岛底观感由 ScatterUnderRocks 的漂浮岩块承担, 不加额外网格
}

UStaticMesh* AWFC3DArenaGenerator_Nature::LoadMesh(const TCHAR* Path)
{
	return LoadObject<UStaticMesh>(nullptr, Path);
}

UMaterialInterface* AWFC3DArenaGenerator_Nature::LoadMaterial(const TCHAR* Path)
{
	return LoadObject<UMaterialInterface>(nullptr, Path);
}

UInstancedStaticMeshComponent* AWFC3DArenaGenerator_Nature::GetOrCreateComp(
	UStaticMesh* Mesh, bool bEnableCollision)
{
	if (!Mesh) { return nullptr; }
	for (UInstancedStaticMeshComponent* C : ScatterComps)
	{
		if (C && C->GetStaticMesh() == Mesh) { return C; }
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetupAttachment(GetRootComponent());
	C->RegisterComponent();
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(bEnableCollision
		? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(bEnableCollision);
	ScatterComps.Add(C);
	return C;
}

void AWFC3DArenaGenerator_Nature::AddInstance(
	UInstancedStaticMeshComponent* Comp, const FVector& Pos,
	const FRotator& Rot, float Scale, const FNatField* Field)
{
	if (!Comp) { return; }
	FVector P = Pos;
	if (Field)
	{
		float H, S;
		if (SampleHeight(*Field, P.X - CenterX, P.Y - CenterY, H, S))
		{
			P.Z = H;
		}
	}
	Comp->AddInstance(FTransform(Rot, P, FVector(Scale)));
}

void AWFC3DArenaGenerator_Nature::BuildNavProxy(const FNatField& F)
{
	UStaticMesh* Cube = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) { return; }
	// 独立组件: 不能走 GetOrCreateComp (它会复用同 mesh 的方碑组件,
	// 隐藏它会把可见方碑也藏起来)
	UHierarchicalInstancedStaticMeshComponent* Proxy =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("NavProxy"));
	Proxy->SetupAttachment(GetRootComponent());
	Proxy->RegisterComponent();
	Proxy->SetStaticMesh(Cube);
	Proxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Proxy->SetCanEverAffectNavigation(true);
	ScatterComps.Add(Proxy);
	if (!Proxy) { return; }
	Proxy->SetVisibility(false);
	Proxy->SetHiddenInGame(true);
	Proxy->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Proxy->SetCanEverAffectNavigation(true);

	const float T = 200.0f;                    // 板间距
	const float Half = PlateauR + 800.0f;      // 覆盖平台+边缘缓冲
	const int32 N = FMath::CeilToInt(Half * 2.0f / T);
	const float O0 = -Half + T * 0.5f;

	// 板顶高度: 场高 -10cm (贴地略低, 单位陷进草里比悬空好看)
	TArray<float> Top, Orig;
	Top.SetNumZeroed(N * N);
	Orig.SetNumZeroed(N * N);
	TArray<uint8> Ok; Ok.SetNumZeroed(N * N);
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const float X = O0 + I * T, Y = O0 + J * T;
			float H, S;
			if (FMath::Sqrt(X * X + Y * Y) <= Half
				&& SampleHeight(F, X, Y, H, S) && S <= 38.0f)
			{
				Ok[J * N + I] = 1;
				Top[J * N + I] = Orig[J * N + I] = H - 10.0f;
			}
		}
	}

	// 相邻板高差松弛到 <=30cm: 保证整个连通片 nav 不断;
	// 与地形偏离 >80cm 的板丢弃 (陡坡带自然断裂 = 诚实不可走)
	for (int32 It = 0; It < 80; ++It)
	{
		bool bChanged = false;
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const int32 A = J * N + I;
				if (!Ok[A]) { continue; }
				for (int32 D = 0; D < 2; ++D)
				{
					const int32 B = (D == 0) ? A + 1 : A + N;
					if (B < 0 || B >= N * N || !Ok[B]) { continue; }
					const float DD = Top[B] - Top[A];
					if (FMath::Abs(DD) > 30.0f)
					{
						const float Mid = (Top[A] + Top[B]) * 0.5f;
						Top[A] = Mid - FMath::Sign(DD) * 15.0f;
						Top[B] = Mid + FMath::Sign(DD) * 15.0f;
						bChanged = true;
					}
				}
			}
		}
		if (!bChanged) { break; }
	}

	int32 Placed = 0;
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const int32 Idx = J * N + I;
			if (!Ok[Idx] || FMath::Abs(Top[Idx] - Orig[Idx]) > 80.0f) { continue; }
			const float X = O0 + I * T + CenterX, Y = O0 + J * T + CenterY;
			Proxy->AddInstance(FTransform(FRotator::ZeroRotator,
				FVector(X, Y, Top[Idx] - 30.0f), FVector(2.0f, 2.0f, 0.6f)));
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Nature] nav proxy: %d/%d tiles"), Placed, N * N);
}

void AWFC3DArenaGenerator_Nature::ScatterVegetation(const FNatField& F, FRandomStream& Rng)
{
	// 草: 密度高的浅色草甸 (4 种混合, 无碰撞)
	static const TCHAR* GrassPaths[] = {
		TEXT("/Game/Foliage/Grass/SM_MWAM_GrassA.SM_MWAM_GrassA"),
		TEXT("/Game/Foliage/Grass/SM_MWAM_GrassB.SM_MWAM_GrassB"),
		TEXT("/Game/Foliage/Grass/SM_MWAM_GrassC.SM_MWAM_GrassC"),
		TEXT("/Game/Foliage/Grass/SM_MWAM_GrassD.SM_MWAM_GrassD"),
	};
	TArray<UInstancedStaticMeshComponent*> Grass;
	for (const TCHAR* P : GrassPaths) { Grass.Add(GetOrCreateComp(LoadMesh(P), false)); }

	const int32 GrassTries = 9000;
	for (int32 K = 0; K < GrassTries; ++K)
	{
		const float X = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float Y = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		float H, S;
		if (!SampleHeight(F, X, Y, H, S) || S > 30.0f) { continue; }
		const float R = FMath::Sqrt(X * X + Y * Y);
		if (R > F.MaxRadius * 0.98f) { continue; }
		// 团簇: Perlin 阈值 -> 草甸成丛而非均匀噪点
		const float Clump = P2(X / 1400.0f, Y / 1400.0f);
		if (Clump < -0.05f) { continue; }
		const int32 Mi = Rng.RandRange(0, Grass.Num() - 1);
		AddInstance(Grass[Mi], FVector(X + CenterX, Y + CenterY, 0),
			FRotator(0, Rng.FRand() * 360.0f, 0), Rng.FRandRange(0.7f, 1.4f), &F);
	}

	// 灌木 + 花: 点缀, 平台内为主
	static const TCHAR* BushPaths[] = {
		TEXT("/Game/Foliage/Foliage/Bush/SM_Bush_01.SM_Bush_01"),
		TEXT("/Game/Foliage/Foliage/Bush/SM_Bush_03.SM_Bush_03"),
		TEXT("/Game/Foliage/Foliage/Bush/SM_Bush_06.SM_Bush_06"),
		TEXT("/Game/Foliage/Foliage/Bush/SM_Bush_08.SM_Bush_08"),
	};
	static const TCHAR* FlowerPaths[] = {
		TEXT("/Game/Foliage/Foliage/Flower/SM_Flower_01.SM_Flower_01"),
		TEXT("/Game/Foliage/Foliage/Flower/SM_Flower_02.SM_Flower_02"),
		TEXT("/Game/Foliage/Foliage/Flower/SM_Flower_03.SM_Flower_03"),
	};
	TArray<UInstancedStaticMeshComponent*> Deco;
	for (const TCHAR* P : BushPaths)   { Deco.Add(GetOrCreateComp(LoadMesh(P), false)); }
	for (const TCHAR* P : FlowerPaths) { Deco.Add(GetOrCreateComp(LoadMesh(P), false)); }
	for (int32 K = 0; K < 400; ++K)
	{
		const float X = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float Y = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		float H, S;
		if (!SampleHeight(F, X, Y, H, S) || S > 32.0f) { continue; }
		const float R = FMath::Sqrt(X * X + Y * Y);
		if (R < PlateauR * 0.35f) { continue; }      // 广场中心留空
		const int32 Mi = Rng.RandRange(0, Deco.Num() - 1);
		AddInstance(Deco[Mi], FVector(X + CenterX, Y + CenterY, 0),
			FRotator(0, Rng.FRand() * 360.0f, 0), Rng.FRandRange(0.8f, 1.6f), &F);
	}

	// 树: 只种在缓坡带/岛缘 (不挡可玩区), 少量剪影
	static const TCHAR* TreePaths[] = {
		TEXT("/Game/Foliage/Tree/SM_Fir_Tree_01.SM_Fir_Tree_01"),
		TEXT("/Game/Foliage/Tree/SM_Fir_Tree_03.SM_Fir_Tree_03"),
		TEXT("/Game/Foliage/Tree/SM_Fir_Tree_05.SM_Fir_Tree_05"),
		TEXT("/Game/Foliage/Tree/SM_Fir_Tree_07.SM_Fir_Tree_07"),
	};
	TArray<UInstancedStaticMeshComponent*> Trees;
	for (const TCHAR* P : TreePaths) { Trees.Add(GetOrCreateComp(LoadMesh(P), false)); }
	TArray<FVector> Placed;
	for (int32 K = 0; K < 500 && Placed.Num() < 16; ++K)
	{
		const float X = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float Y = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float R = FMath::Sqrt(X * X + Y * Y);
		if (R < PlateauR + 700.0f || R > F.MaxRadius * 0.92f) { continue; }
		float H, S;
		if (!SampleHeight(F, X, Y, H, S) || S > 38.0f) { continue; }
		bool bFar = true;
		for (const FVector& P : Placed)
		{
			if (FVector::Dist2D(P, FVector(X, Y, 0)) < 1100.0f) { bFar = false; break; }
		}
		if (!bFar) { continue; }
		Placed.Add(FVector(X, Y, 0));
		const int32 Mi = Rng.RandRange(0, Trees.Num() - 1);
		AddInstance(Trees[Mi], FVector(X + CenterX, Y + CenterY, 0),
			FRotator(0, Rng.FRand() * 360.0f, 0), Rng.FRandRange(0.9f, 1.8f), &F);
	}
}

void AWFC3DArenaGenerator_Nature::ScatterCover(const FNatField& F, FRandomStream& Rng)
{
	// 掩体岩块: Jungle_Rock 系, 带碰撞 -> 战术掩体 + nav 自动绕开
	static const TCHAR* RockPaths[] = {
		TEXT("/Game/RockEnv_Pack/Meshes/Jungle_Rocks/SM_Jungle_Rock_1.SM_Jungle_Rock_1"),
		TEXT("/Game/RockEnv_Pack/Meshes/Jungle_Rocks/SM_Jungle_Rock_3.SM_Jungle_Rock_3"),
		TEXT("/Game/RockEnv_Pack/Meshes/Jungle_Rocks/SM_Jungle_Rock_4.SM_Jungle_Rock_4"),
	};
	TArray<UInstancedStaticMeshComponent*> Rocks;
	for (const TCHAR* P : RockPaths) { Rocks.Add(GetOrCreateComp(LoadMesh(P), true)); }

	TArray<FVector> Placed;
	for (int32 K = 0; K < 400 && Placed.Num() < 12; ++K)
	{
		const float X = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float Y = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float R = FMath::Sqrt(X * X + Y * Y);
		if (R < PlateauR * 0.3f || R > PlateauR + 1400.0f) { continue; }
		float H, S;
		if (!SampleHeight(F, X, Y, H, S) || S > 34.0f) { continue; }
		bool bFar = true;
		for (const FVector& P : Placed)
		{
			if (FVector::Dist2D(P, FVector(X, Y, 0)) < 1600.0f) { bFar = false; break; }
		}
		if (!bFar) { continue; }
		Placed.Add(FVector(X, Y, 0));
		const int32 Mi = Rng.RandRange(0, Rocks.Num() - 1);
		// 半埋: 沿法线略下沉由 Scale 与 Z 偏移近似
		AddInstance(Rocks[Mi], FVector(X + CenterX, Y + CenterY, -40.0f),
			FRotator(0, Rng.FRand() * 360.0f, 0), Rng.FRandRange(0.35f, 0.9f), &F);
	}
}

void AWFC3DArenaGenerator_Nature::ScatterRuins(const FNatField& F, FRandomStream& Rng)
{
	// 科幻遗迹: 混凝土巨石方碑 (半埋+微倾), 与体素岛同一材质语言
	UStaticMesh* Cube = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Con = LoadMaterial(
		TEXT("/Game/Materials/Architecture/MI_Concrete_1.MI_Concrete_1"));
	UInstancedStaticMeshComponent* Mono = GetOrCreateComp(Cube, true);
	if (Mono && Con) { Mono->SetMaterial(0, Con); }

	TArray<FVector> Placed;
	for (int32 K = 0; K < 300 && Placed.Num() < 7; ++K)
	{
		const float X = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float Y = Rng.FRandRange(-1.0f, 1.0f) * F.MaxRadius;
		const float R = FMath::Sqrt(X * X + Y * Y);
		if (R < PlateauR * 0.55f || R > F.MaxRadius * 0.9f) { continue; }
		float H, S;
		if (!SampleHeight(F, X, Y, H, S) || S > 36.0f) { continue; }
		bool bFar = true;
		for (const FVector& P : Placed)
		{
			if (FVector::Dist2D(P, FVector(X, Y, 0)) < 1500.0f) { bFar = false; break; }
		}
		if (!bFar) { continue; }
		Placed.Add(FVector(X, Y, 0));
		const float Hgt = Rng.FRandRange(2.5f, 7.0f);
		const FRotator Rot(Rng.FRandRange(-9.0f, 9.0f),
			Rng.FRand() * 360.0f, Rng.FRandRange(-9.0f, 9.0f));
		AddInstance(Mono, FVector(X + CenterX, Y + CenterY, -Hgt * 25.0f),
			Rot, Hgt, &F);
	}
}

void AWFC3DArenaGenerator_Nature::ScatterUnderRocks(const FNatField& F, FRandomStream& Rng)
{
	// 岛底漂浮岩块: Cliff_Rock 系倒挂在岛缘下方 + 中心龙骨巨石
	static const TCHAR* CliffPaths[] = {
		TEXT("/Game/RockEnv_Pack/Meshes/Cliff_Rocks/SM_Cliff_Rock_1.SM_Cliff_Rock_1"),
		TEXT("/Game/RockEnv_Pack/Meshes/Cliff_Rocks/SM_Cliff_Rock_4.SM_Cliff_Rock_4"),
		TEXT("/Game/RockEnv_Pack/Meshes/Cliff_Rocks/SM_Cliff_Rock_6.SM_Cliff_Rock_6"),
		TEXT("/Game/RockEnv_Pack/Meshes/Cliff_Rocks/SM_Cliff_Rock_8.SM_Cliff_Rock_8"),
	};
	TArray<UInstancedStaticMeshComponent*> Cliffs;
	for (const TCHAR* P : CliffPaths) { Cliffs.Add(GetOrCreateComp(LoadMesh(P), false)); }

	// 边缘环带: 轮廓外 -> 下方漂移
	for (int32 K = 0; K < 26; ++K)
	{
		const float Ang = Rng.FRand() * 2.0f * PI;
		const float R = F.MaxRadius * Rng.FRandRange(0.9f, 1.15f);
		const float X = FMath::Cos(Ang) * R + CenterX;
		const float Y = FMath::Sin(Ang) * R + CenterY;
		const float Z = -Rng.FRandRange(600.0f, 3200.0f);
		const FRotator Rot(Rng.FRandRange(-25.0f, 25.0f),
			Rng.FRand() * 360.0f, Rng.FRandRange(160.0f, 200.0f)); // 大致倒挂
		const int32 Mi = Rng.RandRange(0, Cliffs.Num() - 1);
		if (Cliffs[Mi])
		{
			Cliffs[Mi]->AddInstance(FTransform(Rot, FVector(X, Y, Z),
				FVector(Rng.FRandRange(0.25f, 0.7f))));
		}
	}
	// 龙骨: 中心 2-3 块巨石垂在岛底
	for (int32 K = 0; K < 3; ++K)
	{
		const float Ang = Rng.FRand() * 2.0f * PI;
		const float R = Rng.FRandRange(0.0f, F.MaxRadius * 0.4f);
		const int32 Mi = Rng.RandRange(0, Cliffs.Num() - 1);
		if (Cliffs[Mi])
		{
			Cliffs[Mi]->AddInstance(FTransform(
				FRotator(Rng.FRandRange(-15.0f, 15.0f), Rng.FRand() * 360.0f,
					Rng.FRandRange(170.0f, 190.0f)),
				FVector(FMath::Cos(Ang) * R + CenterX, FMath::Sin(Ang) * R + CenterY,
					-Rng.FRandRange(1800.0f, 4200.0f)),
				FVector(Rng.FRandRange(0.9f, 2.0f))));
		}
	}
}

void AWFC3DArenaGenerator_Nature::SpawnLighting(FRandomStream& Rng)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const FVector Origin = GetActorLocation();

	// 调谐关卡自带氛围演员 (与体素场景同一套虚空基调)
	bool bFoundLevelFog = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A == this || A->GetOwner() == this) { continue; }
		if (AExponentialHeightFog* F = Cast<AExponentialHeightFog>(A))
		{
			bFoundLevelFog = true;
			if (UExponentialHeightFogComponent* FC = F->GetComponent())
			{
				FC->SetFogDensity(0.008f);
				FC->SetFogHeightFalloff(0.45f);
				FC->SetFogInscatteringColor(
					FLinearColor::FromSRGBColor(FColor(10, 13, 22)));
				FC->SetDirectionalInscatteringColor(FLinearColor(0.001f, 0.002f, 0.004f));
				FC->SetDirectionalInscatteringExponent(14.0f);
				FC->SetStartDistance(1200.0f);
				FC->SetVolumetricFog(true);
				FC->SetVolumetricFogScatteringDistribution(0.55f);
				FC->SetVolumetricFogAlbedo(FColor(18, 26, 48));
				FC->SetVolumetricFogEmissive(FLinearColor(0.002f, 0.003f, 0.006f));
			}
		}
		else if (ASkyLight* S = Cast<ASkyLight>(A))
		{
			if (USkyLightComponent* SC = S->GetLightComponent())
			{
				SC->SetIntensity(0.3f);
				SC->SetLightColor(FLinearColor::FromSRGBColor(FColor(70, 82, 110)));
				SC->bLowerHemisphereIsBlack = true;
			}
		}
		else if (A->GetClass()->GetName().Contains(TEXT("SkyAtmosphere")))
		{
			A->SetActorHiddenInGame(true);
		}
	}

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	P.Owner = this;

	const float YawDeg = 20.0f + Rng.FRand() * 320.0f;
	const float PitchDeg = -(46.0f + Rng.FRand() * 14.0f);
	ADirectionalLight* Key = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), Origin, FRotator(PitchDeg, YawDeg, 0.0f), P);
	if (Key)
	{
		UDirectionalLightComponent* LC = Key->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(7.5f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(255, 246, 232)));
		LC->SetCastShadows(true);
		LC->LightSourceAngle = 0.8f;
		LC->ForwardShadingPriority = 1;
		Key->SetActorRotation(FRotator(PitchDeg, YawDeg, 0.0f));
		AtmosphereActors.Add(Key);
	}

	ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), Origin,
		FRotator(-25.0f, YawDeg + 160.0f, 0.0f), P);
	if (Fill)
	{
		UDirectionalLightComponent* LC = Fill->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(0.9f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(120, 140, 175)));
		LC->SetCastShadows(false);
		LC->bAffectTranslucentLighting = true;
		AtmosphereActors.Add(Fill);
	}

	ADirectionalLight* Bounce = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), Origin,
		FRotator(65.0f, YawDeg + 180.0f, 0.0f), P);
	if (Bounce)
	{
		UDirectionalLightComponent* LC = Bounce->GetComponent();
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(1.4f);
		LC->SetLightColor(FLinearColor::FromSRGBColor(FColor(90, 105, 140)));
		LC->SetCastShadows(false);
		AtmosphereActors.Add(Bounce);
	}

	if (!bFoundLevelFog)
	{
		if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(
			AExponentialHeightFog::StaticClass(), Origin + FVector(0, 0, -1400.0f),
			FRotator::ZeroRotator, P))
		{
			UExponentialHeightFogComponent* FC = Fog->GetComponent();
			FC->SetFogDensity(0.008f);
			FC->SetFogHeightFalloff(0.45f);
			FC->SetFogInscatteringColor(
				FLinearColor::FromSRGBColor(FColor(10, 13, 22)));
			FC->SetDirectionalInscatteringColor(FLinearColor(0.001f, 0.002f, 0.004f));
			FC->SetDirectionalInscatteringExponent(14.0f);
			FC->SetStartDistance(1200.0f);
			FC->SetVolumetricFog(true);
			FC->SetVolumetricFogScatteringDistribution(0.55f);
			FC->SetVolumetricFogAlbedo(FColor(18, 26, 48));
			FC->SetVolumetricFogEmissive(FLinearColor(0.002f, 0.003f, 0.006f));
			AtmosphereActors.Add(Fog);
		}
	}
}

bool AWFC3DArenaGenerator_Nature::Generate(const FWFCGenerationRequest& Request)
{
	const double T0 = FPlatformTime::Seconds();
	ClearGenerated();

	// 每次重试换一块截取窗: attempt 计数混入种子
	static int32 AttemptCtr = 0;
	const int32 Seed = (Request.Seed != 0 ? Request.Seed : 1337) + (++AttemptCtr * 7919);
	FRandomStream Rng(Seed);

	FNatField Field;
	BuildField(Seed, Field);

	// 地面材质: 岛例自动材质 (坡度/高度混草-土-岩, 世界空间投影);
	// 兜底 Basic_Gray -> 混凝土
	MatGround = LoadMaterial(
		TEXT("/Game/MWLandscapeAutoMaterial/Materials/Landscape/MTL_MWAM_Landscape_IslandExample.MTL_MWAM_Landscape_IslandExample"));
	if (!MatGround)
	{
		MatGround = LoadMaterial(
			TEXT("/Game/MWLandscapeAutoMaterial/Materials/MASTER/Basic/MTL_MW_Basic_Gray.MTL_MW_Basic_Gray"));
	}
	if (!MatGround)
	{
		MatGround = LoadMaterial(
			TEXT("/Game/Materials/Architecture/MI_Concrete_1.MI_Concrete_1"));
	}

	BuildTerrainMesh(Field);

	// [诊断] 微型 PMC 方块: 若它的顶面有 nav -> PMC 本身没问题
	if (FParse::Param(FCommandLine::Get(), TEXT("NatureCube")))
	{
		UProceduralMeshComponent* Probe = NewObject<UProceduralMeshComponent>(this, TEXT("NavProbe"));
		Probe->SetupAttachment(GetRootComponent());
		Probe->RegisterComponent();
		Probe->bUseComplexAsSimpleCollision = false;
		Probe->bUseAsyncCooking = false;
		Probe->SetCanEverAffectNavigation(true);
		const FVector C(3800.0f, 2000.0f, 400.0f);   // 悬在岛上方, 独立观察
		const float S = 300.0f;
		TArray<FVector> V = {
			C + FVector(-S, -S, 0), C + FVector(S, -S, 0), C + FVector(-S, S, 0), C + FVector(S, S, 0),
			C + FVector(-S, -S, -S), C + FVector(S, -S, -S), C + FVector(-S, S, -S), C + FVector(S, S, -S)};
		TArray<int32> T = {0, 1, 2, 1, 3, 2, 4, 6, 5, 5, 6, 7,
			0, 4, 1, 1, 4, 5, 2, 3, 6, 3, 7, 6, 0, 2, 4, 2, 6, 4, 1, 5, 3, 3, 5, 7};
		TArray<FVector> N; N.SetNumZeroed(8);
		TArray<FVector2D> UV; UV.SetNumZeroed(8);
		TArray<FProcMeshTangent> Ta; Ta.SetNumZeroed(8);
		Probe->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), Ta, true);
		Probe->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Probe->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Probe->UpdateBounds();
		if (UNavigationSystemV1* NS = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			NS->UpdateComponentInNavOctree(*Probe);
		}
		ScatterProbe = Probe;
	}

	// [诊断] 大平板碰撞 (HISM cube): 若 nav 可达面积暴增 -> 只有 HISM 进 navmesh
	if (FParse::Param(FCommandLine::Get(), TEXT("NatureNavBox")))
	{
		UStaticMesh* Cube = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
		UHierarchicalInstancedStaticMeshComponent* Aid =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("NavAid"));
		Aid->SetupAttachment(GetRootComponent());
		Aid->SetStaticMesh(Cube);
		Aid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Aid->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Aid->SetCanEverAffectNavigation(true);
		Aid->SetVisibility(false);
		Aid->RegisterComponent();
		Aid->AddInstance(FTransform(FRotator::ZeroRotator,
			FVector(CenterX, CenterY, -50.0f), FVector(120.0f, 120.0f, 0.4f)));
	}
	BuildNavProxy(Field);
	ScatterVegetation(Field, Rng);
	ScatterCover(Field, Rng);
	ScatterRuins(Field, Rng);
	ScatterUnderRocks(Field, Rng);
	SpawnLighting(Rng);

	// 与 Terrain 同理: 撒点组件注册时也是空包围盒, 统一刷新进 nav 八叉树
	if (UNavigationSystemV1* NavSys =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (UInstancedStaticMeshComponent* C : ScatterComps)
		{
			if (C && C->IsCollisionEnabled() && C->CanEverAffectNavigation())
			{
				C->UpdateBounds();
				NavSys->UpdateComponentInNavOctree(*C);
			}
		}
	}

	// ---- 结果契约 ----
	LastResult = FWFCGenerationResult();
	float CenH, CenS;
	SampleHeight(Field, 0.0f, 0.0f, CenH, CenS);
	LastResult.GroundNavProbe = FVector(CenterX, CenterY, CenH + 10.0f);
	// 平台环带 4 探针 (r=2600) + 缓坡带 2 探针 (r=4300 找可走点)
	for (int32 K = 0; K < 4; ++K)
	{
		const float Ang = K * PI * 0.5f;
		float H, S;
		const float X = FMath::Cos(Ang) * 2600.0f, Y = FMath::Sin(Ang) * 2600.0f;
		if (SampleHeight(Field, X, Y, H, S))
		{
			LastResult.LandingProbes.Add(FVector(X + CenterX, Y + CenterY, H + 10.0f));
		}
	}
	for (int32 K = 0, Found = 0; K < 360 && Found < 2; ++K)
	{
		const float Ang = (float)K * PI / 180.0f;
		float H, S;
		const float X = FMath::Cos(Ang) * 4300.0f, Y = FMath::Sin(Ang) * 4300.0f;
		if (SampleHeight(Field, X, Y, H, S) && S < 36.0f)
		{
			LastResult.LandingProbes.Add(FVector(X + CenterX, Y + CenterY, H + 10.0f));
			++Found;
		}
	}
	LastResult.bSuccess = true;
	LastResult.UsedSeed = Request.Seed;
	// UsedScale*CellSize 会成为校验/导航的 3D 包围盒 (从原点起算):
	// Z 给足 2000cm 覆盖丘顶; X/Y 120m 覆盖整个岛 (悬崖/岛底在盒外, 本就不可走)
	LastResult.UsedScale = FIntVector(GridN, GridN, 40);
	LastResult.CellWorldSize = GetCellWorldSize();
	LastResult.ModuleCount = 1;
	LastResult.CellCount = GridN * GridN;
	LastResult.DurationMs = (FPlatformTime::Seconds() - T0) * 1000.0;

	UE_LOG(LogTemp, Log, TEXT("[Nature] Generated seed=%d islandR=%.0f in %.1f ms"),
		Request.Seed, Field.MaxRadius, LastResult.DurationMs);
	return true;
}

void AWFC3DArenaGenerator_Nature::ClearGenerated()
{
	if (Terrain)
	{
		Terrain->ClearAllMeshSections();
	}
	for (UInstancedStaticMeshComponent* C : ScatterComps)
	{
		if (C) { C->ClearInstances(); }
	}
	for (AActor* A : AtmosphereActors)
	{
		if (A) { A->Destroy(); }
	}
	AtmosphereActors.Reset();
}
