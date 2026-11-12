// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC/Voxel/WFC3DArenaGenerator_Voxel.h"
#include "WFC/Shared/ERArenaAtmosphere.h"
#include "Core/ERDebug.h"
#include "Core/ERLog.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

namespace
{
	const int32 VDX[4] = { 0, 1, 0, -1 };
	const int32 VDY[4] = { 1, 0, -1, 0 };

	FORCEINLINE int32 VoxCell(int32 X, int32 Y) { return Y * 19 + X; }

	// 标签位: bit0 = Void, bit1..4 = L0..L3
	constexpr uint8 DomAll = 0x1F;
	constexpr uint8 DomVoid = 0x01;

	FORCEINLINE bool LabelsCompatible(int32 A, int32 B)
	{
		return A == 0 || B == 0 || FMath::Abs(A - B) <= 1;
	}

	uint32 VoxHash(int32 X, int32 Y, int32 Z, int32 S)
	{
		uint32 H = static_cast<uint32>(X * 374761393) ^ static_cast<uint32>(Y * 668265263)
			^ static_cast<uint32>(Z * 2246822519) ^ static_cast<uint32>(S * 2654435761);
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}
}

AWFC3DArenaGenerator_Voxel::AWFC3DArenaGenerator_Voxel()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWFC3DArenaGenerator_Voxel::ClearGenerated()
{
	Super::ClearGenerated();
	for (UInstancedStaticMeshComponent* Comp : MatComponents)
	{
		if (Comp)
		{
			Comp->ClearInstances();
		}
	}
	if (CollisionHISM)
	{
		CollisionHISM->ClearInstances();
	}
	DynMats.Reset();
	for (AActor* A : AtmoActors)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	AtmoActors.Reset();
}

UMaterialInterface* AWFC3DArenaGenerator_Voxel::LoadMat(const TCHAR* Path)
{
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!M)
	{
		M = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UE_LOG(LogERWFC, Warning,
			TEXT("[Voxel] Missing material %s; falling back to BasicShapeMaterial "
				"(run Scripts/voxel_build_materials.py)."), Path);
	}
	return M;
}

// ---------------------------------------------------------------------------
// 高度场
// ---------------------------------------------------------------------------

float AWFC3DArenaGenerator_Voxel::PriorWeight(
	int32 Motif, int32 MaxL, float Cx, float Cy, int32 X, int32 Y, int32 Label) const
{
	const float Dx = X - Cx;
	const float Dy = Y - Cy;
	const float R = FMath::Sqrt(Dx * Dx + Dy * Dy) / 9.0f;

	if (Label == 0)
	{
		// Void: 只允许外圈, 边界格额外放开 -> 破碎漂浮边缘
		float W = (R > 0.8f) ? 0.25f : 0.0f;
		if (X == 0 || Y == 0 || X == GridW - 1 || Y == GridH - 1)
		{
			W = FMath::Max(W, 0.5f);
		}
		return W;
	}

	const int32 L = Label - 1;
	int32 E;
	if (Motif == 0)
	{
		// Bowl: 中心低外圈高 (圆形剧场)
		E = (R < 0.45f) ? 0 : (R < 0.68f) ? 1 : (R < 0.88f) ? 2 : 3;
	}
	else
	{
		// Ziggurat: 中心祭坛向外下降
		E = (R < 0.30f) ? MaxL : (R < 0.55f) ? MaxL - 1 : (R < 0.80f) ? MaxL - 2 : 0;
		E = FMath::Clamp(E, 0, 3);
	}

	const int32 D = FMath::Abs(L - E);
	return (D == 0) ? 1.0f : (D == 1) ? 0.35f : (D == 2) ? 0.05f : 0.0f;
}

bool AWFC3DArenaGenerator_Voxel::CollapseHeightField(
	FRandomStream& Rng, int32 Motif, float Cx, float Cy, uint8* OutL)
{
	// 在 10x10 宏格 (2x2 子格, 末行/末列 1 宽) 上坍缩: 平台至少 800cm 厚,
	// 楼梯低格背后必有同层入口格, 细环结构性不可达的问题从根上消失。
	constexpr int32 MW = 10;
	constexpr int32 MH = 10;
	auto MIdx = [](int32 X, int32 Y) { return X + Y * MW; };

	const int32 MaxL = (Motif == 1) ? Rng.RandRange(2, 3) : 3;

	uint8 Dom[MW * MH];
	FMemory::Memset(Dom, DomAll, sizeof(Dom));

	auto PopCount = [](uint8 M) { return FMath::CountBits(static_cast<uint32>(M)); };

	// 弧相容: 从 Cell 域裁剪出发, 把约束沿 4 邻传播
	auto Propagate = [&](int32 StartIdx) -> bool
	{
		TArray<int32> Queue;
		Queue.Add(StartIdx);
		TBitArray<> InQ;
		InQ.Init(false, MW * MH);
		InQ[StartIdx] = true;
		while (Queue.Num() > 0)
		{
			const int32 A = Queue.Pop();
			InQ[A] = false;
			const int32 AX = A % MW;
			const int32 AY = A / MW;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + VDX[S];
				const int32 BY = AY + VDY[S];
				if (BX < 0 || BY < 0 || BX >= MW || BY >= MH)
				{
					continue;
				}
				const int32 B = MIdx(BX, BY);
				uint8 NewDom = 0;
				for (int32 Lb = 0; Lb < 5; ++Lb)
				{
					if ((Dom[B] & (1 << Lb)) == 0)
					{
						continue;
					}
					for (int32 La = 0; La < 5; ++La)
					{
						if ((Dom[A] & (1 << La)) && LabelsCompatible(La, Lb))
						{
							NewDom |= (1 << Lb);
							break;
						}
					}
				}
				if (NewDom != Dom[B])
				{
					Dom[B] = NewDom;
					if (NewDom == 0)
					{
						return false;
					}
					if (!InQ[B])
					{
						InQ[B] = true;
						Queue.Add(B);
					}
				}
			}
		}
		return true;
	};

	for (int32 Settled = 0; Settled < MW * MH; ++Settled)
	{
		// 最小熵未定格 + 小随机扰动
		int32 Best = INDEX_NONE;
		float BestScore = FLT_MAX;
		for (int32 I = 0; I < MW * MH; ++I)
		{
			const int32 C = PopCount(Dom[I]);
			if (C <= 1)
			{
				continue;
			}
			const float Score = C + Rng.FRand();
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = I;
			}
		}
		if (Best == INDEX_NONE)
		{
			break;
		}

		const int32 X = Best % MW;
		const int32 Y = Best / MW;

		// 已坍缩同标签邻居数 -> 权重 x3/个, 促成大平台
		int32 SameN[5] = { 0, 0, 0, 0, 0 };
		for (int32 S = 0; S < 4; ++S)
		{
			const int32 NX = X + VDX[S];
			const int32 NY = Y + VDY[S];
			if (NX < 0 || NY < 0 || NX >= MW || NY >= MH)
			{
				continue;
			}
			const uint8 ND = Dom[MIdx(NX, NY)];
			if (PopCount(ND) == 1)
			{
				const int32 NL = FMath::FloorLog2(ND);
				++SameN[NL];
			}
		}

		// 先验取宏格的代表子格坐标 (末列/末行只有 1 宽)
		const int32 RX = FMath::Min(X * 2, GridW - 1);
		const int32 RY = FMath::Min(Y * 2, GridH - 1);
		float Weights[5];
		float Total = 0.0f;
		for (int32 Lb = 0; Lb < 5; ++Lb)
		{
			Weights[Lb] = ((Dom[Best] & (1 << Lb)) == 0) ? 0.0f
				: PriorWeight(Motif, MaxL, Cx, Cy, RX, RY, Lb)
					* FMath::Pow(3.0f, SameN[Lb]);
			Total += Weights[Lb];
		}
		if (Total <= 0.0f)
		{
			return false;
		}

		float Pick = Rng.FRand() * Total;
		int32 Chosen = 4;
		for (int32 Lb = 0; Lb < 5; ++Lb)
		{
			Pick -= Weights[Lb];
			if (Pick <= 0.0f)
			{
				Chosen = Lb;
				break;
			}
		}
		Dom[Best] = static_cast<uint8>(1 << Chosen);
		if (!Propagate(Best))
		{
			return false;
		}
	}

	// 宏格展开回 19x19: 末行/末列宏格只覆盖 1 个子格
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const int32 Lb = FMath::FloorLog2(Dom[MIdx(X / 2, Y / 2)]);
			OutL[VoxCell(X, Y)] = (Lb == 0) ? LVoid : static_cast<uint8>(Lb - 1);
		}
	}
	return true;
}

bool AWFC3DArenaGenerator_Voxel::BuildBowlField(
	FRandomStream& Rng, float Cx, float Cy, uint8* OutL)
{
	// 确定性同心台地: 环形分层 + 低频噪声弯折带边界 + 外圈随机缺口。
	// Bowl 走 WFC 先验时环带被逐格坍缩切成 20+ 碎片区域, 楼梯连通性
	// 结构性凑不齐; 公式化生成反而更像"圆形剧场", 台地也更干净。
	constexpr int32 MW = 10;
	constexpr int32 MH = 10;
	auto MIdx = [](int32 X, int32 Y) { return X + Y * MW; };
	const float SaltV = Rng.RandRange(0, 999) + 0.71f;

	uint8 M[MW * MH];
	for (int32 MY = 0; MY < MH; ++MY)
	{
		for (int32 MX = 0; MX < MW; ++MX)
		{
			// 代表子格坐标 (末行/末列宏格宽 1)
			const float RX = FMath::Min(MX * 2, GridW - 1) + 0.5f;
			const float RY = FMath::Min(MY * 2, GridH - 1) + 0.5f;
			const float R = FMath::Sqrt(
				FMath::Square(RX - Cx) + FMath::Square(RY - Cy)) / 9.0f;

			// 不抖动: 逐宏格 ±1 会埋进大量异层孤岛/碎片区域, 楼梯邻接
			// 结构性凑不齐。同心台地本身就够 (宏格量化边缘已呈阶梯状),
			// 蜿蜒感交给外圈缺口与焦点偏移。
			int32 E = (R < 0.45f) ? 0 : (R < 0.68f) ? 1 : (R < 0.88f) ? 2 : 3;
			M[MIdx(MX, MY)] = static_cast<uint8>(FMath::Clamp(E, 0, 3));

			// 虚空缺口只啃最外圈 (R>0.88): 内部台地保持实心环带,
			// 每个高度层 1~2 个连通区域, 楼梯邻接才有冗余边可用;
			// 边缘残破的"漂浮碎壳"轮廓由外圈缺口承担。
			const float Bite = ValueNoise3(MX * 0.7f + 40.0f, MY * 0.7f, SaltV);
			const float V = (VoxHash(MX, MY, 97, 0) & 0xFFFF) / 65535.0f;
			const float VoidP = (R > 0.92f) ? 0.5f : 0.0f;
			if (V < VoidP || (R > 0.88f && Bite < 0.30f))
			{
				M[MIdx(MX, MY)] = LVoid;
			}
		}
	}

	// |ΔL|<=1 松弛 (边界弯折可能拉出 2 级落差)
	for (int32 Guard = 0; Guard < 8; ++Guard)
	{
		bool bAny = false;
		for (int32 MY = 0; MY < MH; ++MY)
		{
			for (int32 MX = 0; MX < MW; ++MX)
			{
				const int32 A = MIdx(MX, MY);
				if (M[A] == LVoid) { continue; }
				for (int32 S = 1; S <= 2; ++S)
				{
					const int32 BX = MX + VDX[S];
					const int32 BY = MY + VDY[S];
					if (BX >= MW || BY >= MH) { continue; }
					const int32 B = MIdx(BX, BY);
					if (M[B] == LVoid) { continue; }
					if (M[A] > M[B] + 1) { M[A] = M[B] + 1; bAny = true; }
					else if (M[B] > M[A] + 1) { M[B] = M[A] + 1; bAny = true; }
				}
			}
		}
		if (!bAny) { break; }
	}

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			OutL[VoxCell(X, Y)] = M[MIdx(X / 2, Y / 2)];
		}
	}
	return true;
}

void AWFC3DArenaGenerator_Voxel::FixupSmallRegions(uint8* L)
{
	// <14 格的同高连通块并入接壤最多的邻层 (平票取最低);
	// 只向下并会留下被高层包围的孤岛, 区域数一多楼梯邻接必断。
	bool bChanged = true;
	for (int32 Guard = 0; Guard < 12 && bChanged; ++Guard)
	{
		bChanged = false;
		int32 Region[GridW * GridH];
		FMemory::Memset(Region, 0xFF, sizeof(Region));
		TArray<int32> RegionSize;
		for (int32 I = 0; I < GridW * GridH; ++I)
		{
			if (L[I] == LVoid || Region[I] != -1)
			{
				continue;
			}
			const int32 Rid = RegionSize.Num();
			RegionSize.Add(0);
			TArray<int32> Stack;
			Stack.Add(I);
			Region[I] = Rid;
			while (Stack.Num() > 0)
			{
				const int32 A = Stack.Pop();
				++RegionSize[Rid];
				const int32 AX = A % GridW;
				const int32 AY = A / GridW;
				for (int32 S = 0; S < 4; ++S)
				{
					const int32 BX = AX + VDX[S];
					const int32 BY = AY + VDY[S];
					if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
					{
						continue;
					}
					const int32 B = VoxCell(BX, BY);
					if (Region[B] == -1 && L[B] == L[A])
					{
						Region[B] = Rid;
						Stack.Add(B);
					}
				}
			}
		}

		for (int32 I = 0; I < GridW * GridH; ++I)
		{
			if (L[I] == LVoid || RegionSize[Region[I]] >= 14)
			{
				continue;
			}
			// 并入接壤边数最多的非 Void 层 (平票取最低层)
			const int32 AX = I % GridW;
			const int32 AY = I / GridW;
			int32 Touch[4] = { 0, 0, 0, 0 };
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + VDX[S];
				const int32 BY = AY + VDY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const uint8 NL = L[VoxCell(BX, BY)];
				if (NL != LVoid)
				{
					++Touch[NL];
				}
			}
			int32 BestL = -1;
			int32 BestT = 0;
			for (int32 T = 0; T <= 3; ++T)
			{
				if (Touch[T] > BestT)
				{
					BestT = Touch[T];
					BestL = T;
				}
			}
			if (BestL >= 0 && BestL != L[I])
			{
				L[I] = static_cast<uint8>(BestL);
				bChanged = true;
			}
		}
	}

	// 兜底: 松弛 |ΔL|<=1 (并入可能制造新的落差)
	for (int32 Guard = 0; Guard < 64; ++Guard)
	{
		bool bAny = false;
		for (int32 Y = 0; Y < GridH; ++Y)
		{
			for (int32 X = 0; X < GridW; ++X)
			{
				const int32 A = VoxCell(X, Y);
				if (L[A] == LVoid)
				{
					continue;
				}
				for (int32 S = 1; S <= 2; ++S)
				{
					const int32 BX = X + VDX[S];
					const int32 BY = Y + VDY[S];
					if (BX >= GridW || BY >= GridH)
					{
						continue;
					}
					const int32 B = VoxCell(BX, BY);
					if (L[B] == LVoid)
					{
						continue;
					}
					if (L[A] > L[B] + 1) { L[A] = L[B] + 1; bAny = true; }
					else if (L[B] > L[A] + 1) { L[B] = L[A] + 1; bAny = true; }
				}
			}
		}
		if (!bAny)
		{
			break;
		}
	}

	// 删除孤岛区域: 全部邻居都是 Void 或 |ΔL|>1 的同高块永远放不了
	// 楼梯, 留着必然连通失败 (外圈漂浮碎壳靠 Void 缺口表现, 不靠孤岛)。
	for (int32 Guard = 0; Guard < 8; ++Guard)
	{
		int32 Region[GridW * GridH];
		FMemory::Memset(Region, 0xFF, sizeof(Region));
		TArray<int32> RegionSize;
		for (int32 I = 0; I < GridW * GridH; ++I)
		{
			if (L[I] == LVoid || Region[I] != -1)
			{
				continue;
			}
			const int32 Rid = RegionSize.Num();
			RegionSize.Add(0);
			TArray<int32> Stack;
			Stack.Add(I);
			Region[I] = Rid;
			while (Stack.Num() > 0)
			{
				const int32 A = Stack.Pop();
				++RegionSize[Rid];
				const int32 AX = A % GridW;
				const int32 AY = A / GridW;
				for (int32 S = 0; S < 4; ++S)
				{
					const int32 BX = AX + VDX[S];
					const int32 BY = AY + VDY[S];
					if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
					{
						continue;
					}
					const int32 B = VoxCell(BX, BY);
					if (Region[B] == -1 && L[B] == L[A])
					{
						Region[B] = Rid;
						Stack.Add(B);
					}
				}
			}
		}
		TArray<bool> bIsland;
		bIsland.Init(true, RegionSize.Num());
		for (int32 I = 0; I < GridW * GridH; ++I)
		{
			if (L[I] == LVoid) { continue; }
			const int32 AX = I % GridW;
			const int32 AY = I / GridW;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + VDX[S];
				const int32 BY = AY + VDY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = VoxCell(BX, BY);
				if (L[B] == LVoid || Region[B] == Region[I])
				{
					continue;
				}
				const int32 D = FMath::Abs(
					static_cast<int32>(L[B]) - static_cast<int32>(L[I]));
				if (D <= 1 && Region[I] < bIsland.Num())
				{
					bIsland[Region[I]] = false;
				}
			}
		}
		bool bAny = false;
		for (int32 I = 0; I < GridW * GridH; ++I)
		{
			if (Region[I] >= 0 && Region[I] < bIsland.Num() && bIsland[Region[I]])
			{
				L[I] = LVoid;
				bAny = true;
			}
		}
		if (!bAny)
		{
			break;
		}
	}
}

bool AWFC3DArenaGenerator_Voxel::PlaceStairs(FRandomStream& Rng, FVoxPlan& Plan)
{
	const int32 N = GridW * GridH;
	Plan.Stairs.Reset();
	Plan.RegionOf.Init(-1, N);
	if (Plan.RegionOf.Num() != N)
	{
		return false;
	}

	// 同高连通区域编号
	TArray<int32> RegionSize;
	for (int32 I = 0; I < N; ++I)
	{
		if (Plan.L[I] == LVoid || Plan.RegionOf[I] != -1)
		{
			continue;
		}
		const int32 Rid = RegionSize.Num();
		RegionSize.Add(0);
		TArray<int32> Stack;
		Stack.Add(I);
		Plan.RegionOf[I] = Rid;
		while (Stack.Num() > 0)
		{
			const int32 A = Stack.Pop();
			++RegionSize[Rid];
			const int32 AX = A % GridW;
			const int32 AY = A / GridW;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + VDX[S];
				const int32 BY = AY + VDY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = VoxCell(BX, BY);
				if (Plan.RegionOf[B] == -1 && Plan.L[B] == Plan.L[A])
				{
					Plan.RegionOf[B] = Rid;
					Stack.Add(B);
				}
			}
		}
	}


	// 区域间 ΔL=1 的邻接: (Ra,Rb) -> 共享边列表 (低侧格, 方向)
	struct FAdj
	{
		int32 Ra = 0;
		int32 Rb = 0;
		TArray<FVoxStair> Edges;
	};
	TArray<FAdj> Adjs;
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			for (int32 S = 1; S <= 2; ++S)
			{
				const int32 BX = X + VDX[S];
				const int32 BY = Y + VDY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 A = VoxCell(X, Y);
				const int32 B = VoxCell(BX, BY);
				if (Plan.L[A] == LVoid || Plan.L[B] == LVoid
					|| Plan.RegionOf[A] < 0 || Plan.RegionOf[B] < 0)
				{
					continue;
				}
				const int32 La = Plan.L[A];
				const int32 Lb = Plan.L[B];
				if (FMath::Abs(La - Lb) != 1 || Plan.RegionOf[A] == Plan.RegionOf[B])
				{
					continue;
				}
				const int32 Ra = FMath::Min(Plan.RegionOf[A], Plan.RegionOf[B]);
				const int32 Rb = FMath::Max(Plan.RegionOf[A], Plan.RegionOf[B]);
				FAdj* Found = nullptr;
				for (FAdj& E : Adjs)
				{
					if (E.Ra == Ra && E.Rb == Rb)
					{
						Found = &E;
						break;
					}
				}
				if (!Found)
				{
					FAdj NewAdj;
					NewAdj.Ra = Ra;
					NewAdj.Rb = Rb;
					Found = &Adjs[Adjs.Add(NewAdj)];
				}
				FVoxStair St;
				if (La < Lb) { St.LowX = X; St.LowY = Y; St.Dir = S; }
				else { St.LowX = BX; St.LowY = BY; St.Dir = (S + 2) % 4; }
				Found->Edges.Add(St);
			}
		}
	}


	// 丢弃过小区域参与的邻接: <4 格的落点在 nav 上烘不出可靠可站面
	// (实测 1~2 格孤岛落点探针必 UNREACHABLE)。小区域此后必然不可达,
	// 由连通修复环把它并进邻层, 比放一个注定走不通的楼梯干净。
	for (int32 I = Adjs.Num() - 1; I >= 0; --I)
	{
		if (FMath::Min(RegionSize[Adjs[I].Ra], RegionSize[Adjs[I].Rb]) < 4)
		{
			Adjs.RemoveAt(I);
		}
	}

	// Kruskal: 先保证全连通 (生成树), 再额外加 ~30% 环路
	TArray<int32> Edges;
	for (int32 I = 0; I < Adjs.Num(); ++I)
	{
		Edges.Add(I);
	}
	for (int32 I = Edges.Num() - 1; I > 0; --I)
	{
		Edges.Swap(I, Rng.RandRange(0, I));
	}

	TArray<int32> Parent;
	Parent.Init(-1, RegionSize.Num());
	auto Find = [&Parent](int32 R) -> int32
	{
		while (Parent[R] >= 0) { R = Parent[R]; }
		return R;
	};

	TArray<int32> Chosen;
	TArray<int32> Leftover;
	for (int32 E : Edges)
	{
		const int32 Pa = Find(Adjs[E].Ra);
		const int32 Pb = Find(Adjs[E].Rb);
		if (Pa != Pb)
		{
			Parent[Pa] = Pb;
			Chosen.Add(E);
		}
		else
		{
			Leftover.Add(E);
		}
	}
	const int32 Extra = FMath::Min(Leftover.Num(),
		FMath::CeilToInt(Leftover.Num() * 0.3f));
	for (int32 I = 0; I < Extra; ++I)
	{
		Chosen.Add(Leftover[I]);
	}


	// 每个邻接在共享边中随机选一条; 楼梯角色格(入口/低/高)互斥, 入口必须同层
	Plan.RoleCells.Reset();
	Plan.LowEntry.Reset();
	Plan.LowHigh.Reset();
	for (int32 E : Chosen)
	{
		TArray<FVoxStair> Cands = Adjs[E].Edges;
		for (int32 I = Cands.Num() - 1; I > 0; --I)
		{
			Cands.Swap(I, Rng.RandRange(0, I));
		}
		bool bPlaced = false;
		int32 SkipNoApp = 0, SkipRole = 0;
		for (const FVoxStair& St : Cands)
		{
			const int32 LowIdx = VoxCell(St.LowX, St.LowY);
			const int32 HighIdx = VoxCell(St.LowX + VDX[St.Dir],
				St.LowY + VDY[St.Dir]);
			const uint8 LowL = Plan.L[LowIdx];
			// 实测: 侧向登梯 nav 烘不出 (台阶侧边 25cm 唇沿不被当成可跨步),
			// 只认正对入口 (低格反方向同层格)。同层侧邻居仍打角色格,
			// 防止掩体把梯脚围死。
			const int32 EX = St.LowX - VDX[St.Dir];
			const int32 EY = St.LowY - VDY[St.Dir];
			if (EX < 0 || EY < 0 || EX >= GridW || EY >= GridH)
			{
				++SkipNoApp;
				continue;
			}
			const int32 EntryIdx = VoxCell(EX, EY);
			if (Plan.L[EntryIdx] != LowL || Plan.RoleCells.Contains(EntryIdx))
			{
				++SkipNoApp;
				continue;
			}
			// 一格一角
			if (Plan.RoleCells.Contains(LowIdx) || Plan.RoleCells.Contains(HighIdx))
			{
				++SkipRole;
				continue;
			}
			Plan.RoleCells.Add(LowIdx);
			Plan.RoleCells.Add(HighIdx);
			Plan.RoleCells.Add(EntryIdx);
			for (int32 S2 = 0; S2 < 4; ++S2)
			{
				if (S2 == St.Dir) { continue; }
				const int32 NX = St.LowX + VDX[S2];
				const int32 NY = St.LowY + VDY[S2];
				if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH)
				{
					continue;
				}
				const int32 NI = VoxCell(NX, NY);
				if (Plan.L[NI] == LowL)
				{
					Plan.RoleCells.Add(NI);
				}
			}
			Plan.LowEntry.Add(LowIdx, EntryIdx);
			Plan.LowHigh.Add(LowIdx, HighIdx);
			Plan.Stairs.Add(St);
			bPlaced = true;
			break;
		}
		// 修复兜底: 全候选失败时, 找能把正对入口格整平为 LowL 的边放
		// 楼梯。入口格必须非角色格, 且整平后与自身其他非 Void 邻居
		// |ΔL|<=1; 入口已在网格外 (低格贴图边) 则该邻接确实无救。
		if (!bPlaced)
		{
			for (const FVoxStair& St : Cands)
			{
				const int32 LowIdx = VoxCell(St.LowX, St.LowY);
				const int32 EX = St.LowX - VDX[St.Dir];
				const int32 EY = St.LowY - VDY[St.Dir];
				if (EX < 0 || EY < 0 || EX >= GridW || EY >= GridH)
				{
					continue;
				}
				const int32 EntryIdx = VoxCell(EX, EY);
				const int32 HighIdx = VoxCell(St.LowX + VDX[St.Dir],
					St.LowY + VDY[St.Dir]);
				if (Plan.RoleCells.Contains(LowIdx) || Plan.RoleCells.Contains(EntryIdx)
					|| Plan.RoleCells.Contains(HighIdx))
				{
					continue;
				}
				const uint8 LowL = Plan.L[LowIdx];
				bool bFlatOk = true;
				for (int32 S2 = 0; S2 < 4 && bFlatOk; ++S2)
				{
					const int32 NX = EX + VDX[S2];
					const int32 NY = EY + VDY[S2];
					if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH)
					{
						continue;
					}
					const int32 NI = VoxCell(NX, NY);
					if (NI == LowIdx)
					{
						continue;
					}
					const uint8 NL = Plan.L[NI];
					if (NL != LVoid
						&& FMath::Abs(static_cast<int32>(NL) - static_cast<int32>(LowL)) > 1)
					{
						bFlatOk = false;
					}
				}
				if (!bFlatOk)
				{
					continue;
				}
				Plan.L[EntryIdx] = LowL;
				Plan.RoleCells.Add(LowIdx);
				Plan.RoleCells.Add(EntryIdx);
				Plan.RoleCells.Add(HighIdx);
				Plan.LowEntry.Add(LowIdx, EntryIdx);
				Plan.LowHigh.Add(LowIdx, HighIdx);
				Plan.Stairs.Add(St);
				bPlaced = true;
				break;
			}
		}
		// 找不到合法边也修不了才跳过: BFS 校验兜住连通性
		if (!bPlaced)
		{
			UE_LOG(LogERWFC, Warning,
				TEXT("[VoxDiag] adj r%d<->r%d SKIPPED cands=%d noapp=%d role=%d"),
				Adjs[E].Ra, Adjs[E].Rb, Cands.Num(), SkipNoApp, SkipRole);
		}
	}
	return Plan.Stairs.Num() > 0;
}

bool AWFC3DArenaGenerator_Voxel::CheckConnectivity(
	const FVoxPlan& Plan, int32* OutReached) const
{
	const int32 N = GridW * GridH;
	TArray<uint8> Visited;
	Visited.Init(0, N);

	// 楼梯低格只与入口/高格相连 (侧向登梯 nav 烘不出, 实测);
	// 其他格与低格之间仅允许 "入口->低格"

	int32 Start = INDEX_NONE;
	int32 NonVoid = 0;
	for (int32 I = 0; I < N; ++I)
	{
		if (Plan.L[I] != LVoid)
		{
			++NonVoid;
			if (Start == INDEX_NONE) { Start = I; }
		}
	}
	if (Start == INDEX_NONE)
	{
		return false;
	}

	TArray<int32> Stack;
	Stack.Add(Start);
	Visited[Start] = 1;
	int32 Count = 0;
	while (Stack.Num() > 0)
	{
		const int32 A = Stack.Pop();
		++Count;
		const int32 AX = A % GridW;
		const int32 AY = A / GridW;
		for (int32 S = 0; S < 4; ++S)
		{
			const int32 BX = AX + VDX[S];
			const int32 BY = AY + VDY[S];
			if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
			{
				continue;
			}
			const int32 B = VoxCell(BX, BY);
			if (Visited[B] || Plan.L[B] == LVoid)
			{
				continue;
			}
			const int32* ALow = Plan.LowEntry.Find(A);
			const int32* BLow = Plan.LowEntry.Find(B);
			if (ALow)
			{
				// A 是低格: 只允许去入口或高格
				if (B != *ALow && B != Plan.LowHigh[A])
				{
					continue;
				}
			}
			else if (BLow)
			{
				// B 是低格: 只有它的入口(或高格)能进
				if (A != *BLow && A != Plan.LowHigh[B])
				{
					continue;
				}
			}
			else if (Plan.L[A] != Plan.L[B])
			{
				continue;
			}
			Visited[B] = 1;
			Stack.Add(B);
		}
	}
	if (OutReached) { *OutReached = Count; }
	return Count == NonVoid;
}

bool AWFC3DArenaGenerator_Voxel::MergeSmallestUnreached(FVoxPlan& Plan) const
{
	const int32 N = GridW * GridH;
	TArray<uint8> Visited;
	Visited.Init(0, N);
	int32 Start = INDEX_NONE;
	for (int32 I = 0; I < N; ++I)
	{
		if (Plan.L[I] != LVoid) { Start = I; break; }
	}
	if (Start == INDEX_NONE)
	{
		return false;
	}
	TArray<int32> Stack;
	Stack.Add(Start);
	Visited[Start] = 1;
	while (Stack.Num() > 0)
	{
		const int32 A = Stack.Pop();
		const int32 AX = A % GridW;
		const int32 AY = A / GridW;
		for (int32 S = 0; S < 4; ++S)
		{
			const int32 BX = AX + VDX[S];
			const int32 BY = AY + VDY[S];
			if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
			{
				continue;
			}
			const int32 B = VoxCell(BX, BY);
			if (Visited[B] || Plan.L[B] == LVoid)
			{
				continue;
			}
			const int32* ALow = Plan.LowEntry.Find(A);
			const int32* BLow = Plan.LowEntry.Find(B);
			if (ALow)
			{
				if (B != *ALow && B != Plan.LowHigh[A]) { continue; }
			}
			else if (BLow)
			{
				if (A != *BLow && A != Plan.LowHigh[B]) { continue; }
			}
			else if (Plan.L[A] != Plan.L[B])
			{
				continue;
			}
			Visited[B] = 1;
			Stack.Add(B);
		}
	}

	// 不可达格分成连通小组 (按 BFS 规则分组, 不按区域号——
	// 挂在楼梯低格非入口侧的同层格和主区同号但不可达)。
	// 取最小一组: 有异层邻居则并入接壤最多的层; 全是同层/虚空邻居
	// (被楼梯拓扑困死) 则整组变 Void——反正本来就到不了。
	TArray<int32> Unreached;
	for (int32 I = 0; I < N; ++I)
	{
		if (Plan.L[I] != LVoid && !Visited[I])
		{
			Unreached.Add(I);
		}
	}
	if (Unreached.Num() == 0)
	{
		return false;
	}
	TSet<int32> InComp;
	TArray<int32> BestComp;
	for (const int32 SeedI : Unreached)
	{
		if (InComp.Contains(SeedI))
		{
			continue;
		}
		TArray<int32> Comp;
		TArray<int32> S;
		S.Add(SeedI);
		InComp.Add(SeedI);
		while (S.Num() > 0)
		{
			const int32 A = S.Pop();
			Comp.Add(A);
			const int32 AX = A % GridW;
			const int32 AY = A / GridW;
			for (int32 D = 0; D < 4; ++D)
			{
				const int32 BX = AX + VDX[D];
				const int32 BY = AY + VDY[D];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = VoxCell(BX, BY);
				if (!InComp.Contains(B) && Plan.L[B] != LVoid && !Visited[B])
				{
					InComp.Add(B);
					S.Add(B);
				}
			}
		}
		if (BestComp.Num() == 0 || Comp.Num() < BestComp.Num())
		{
			BestComp = Comp;
		}
	}
	TSet<int32> CompSet(BestComp);
	int32 Touch[4] = { 0, 0, 0, 0 };
	for (const int32 I : BestComp)
	{
		const int32 AX = I % GridW;
		const int32 AY = I / GridW;
		for (int32 D = 0; D < 4; ++D)
		{
			const int32 BX = AX + VDX[D];
			const int32 BY = AY + VDY[D];
			if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
			{
				continue;
			}
			const int32 B = VoxCell(BX, BY);
			if (Plan.L[B] != LVoid && !CompSet.Contains(B))
			{
				++Touch[Plan.L[B]];
			}
		}
	}
	const uint8 OwnL = Plan.L[BestComp[0]];
	int32 BestL = -1, BestT = 0;
	for (int32 T = 0; T <= 3; ++T)
	{
		if (T != OwnL && Touch[T] > BestT) { BestT = Touch[T]; BestL = T; }
	}
	const uint8 NewL = (BestL >= 0) ? static_cast<uint8>(BestL) : LVoid;
	for (const int32 I : BestComp)
	{
		Plan.L[I] = NewL;
	}
	return true;
}

bool AWFC3DArenaGenerator_Voxel::BuildPlan(
	int32 Seed, FRandomStream& Rng, FVoxPlan& OutPlan)
{
	OutPlan.Motif = Rng.RandRange(0, 1);
	const float Cx = 9.0f + Rng.RandRange(-2, 2);
	const float Cy = 9.0f + Rng.RandRange(-2, 2);

	const bool bFieldOk = (OutPlan.Motif == 0)
		? BuildBowlField(Rng, Cx, Cy, OutPlan.L)
		: CollapseHeightField(Rng, OutPlan.Motif, Cx, Cy, OutPlan.L);
	if (!bFieldOk)
	{
		UE_LOG(LogERWFC, Warning, TEXT("[VoxDiag] motif=%d fail: collapse"), OutPlan.Motif);
		return false;
	}
	FixupSmallRegions(OutPlan.L);

	// 最大同层连通区域 >= 非 Void 格数 20% (保证大块平地, 两种母题都能过)
	int32 NonVoid = 0;
	int32 Region[GridW * GridH];
	FMemory::Memset(Region, 0xFF, sizeof(Region));
	int32 BestRegion = 0;
	for (int32 I = 0; I < GridW * GridH; ++I)
	{
		if (OutPlan.L[I] == LVoid)
		{
			continue;
		}
		++NonVoid;
		if (Region[I] != -1)
		{
			continue;
		}
		int32 Size = 0;
		TArray<int32> Stack;
		Stack.Add(I);
		Region[I] = 1;
		while (Stack.Num() > 0)
		{
			const int32 A = Stack.Pop();
			++Size;
			const int32 AX = A % GridW;
			const int32 AY = A / GridW;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + VDX[S];
				const int32 BY = AY + VDY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = VoxCell(BX, BY);
				if (Region[B] == -1 && OutPlan.L[B] == OutPlan.L[A])
				{
					Region[B] = 1;
					Stack.Add(B);
				}
			}
		}
		BestRegion = FMath::Max(BestRegion, Size);
	}
	if (NonVoid == 0 || BestRegion * 5 < NonVoid)
	{
		UE_LOG(LogERWFC, Warning,
			TEXT("[VoxDiag] motif=%d fail: region nonvoid=%d best=%d"),
			OutPlan.Motif, NonVoid, BestRegion);
		return false;
	}

	// 出生侧: x<=4 与 x>=14 各 >=8 个非 Void 格
	int32 West = 0;
	int32 East = 0;
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			if (OutPlan.L[VoxCell(X, Y)] == LVoid)
			{
				continue;
			}
			if (X <= 4) { ++West; }
			if (X >= 14) { ++East; }
		}
	}
	if (West < 8 || East < 8)
	{
		UE_LOG(LogERWFC, Warning,
			TEXT("[VoxDiag] motif=%d fail: spawn west=%d east=%d"),
			OutPlan.Motif, West, East);
		return false;
	}

	// 连通修复环: 摆楼梯 -> BFS -> 把不可达的最小区域并入接壤最多的
	// 邻层 -> 重摆。每轮区域数 -1, 最多 4 轮必然收敛; 比直接换母题
	// 便宜, 也保住 Bowl 的出场率。
	int32 Reached = 0;
	for (int32 Rep = 0; Rep < 4; ++Rep)
	{
		if (!PlaceStairs(Rng, OutPlan))
		{
			UE_LOG(LogERWFC, Warning, TEXT("[VoxDiag] motif=%d fail: stairs"), OutPlan.Motif);
			return false;
		}
		if (CheckConnectivity(OutPlan, &Reached))
		{
			return true;
		}
		if (Rep == 3)
		{
			break;
		}
		if (!MergeSmallestUnreached(OutPlan))
		{
			break;
		}
		FixupSmallRegions(OutPlan.L);
	}

	{
		int32 NumRegions = 0;
		for (int32 I = 0; I < GridW * GridH; ++I)
		{
			NumRegions = FMath::Max(NumRegions, OutPlan.RegionOf[I] + 1);
		}
		UE_LOG(LogERWFC, Warning,
			TEXT("[VoxDiag] motif=%d fail: conn stairs=%d regions=%d reached=%d"),
			OutPlan.Motif, OutPlan.Stairs.Num(), NumRegions, Reached);
		static int32 Dumped = 0;
		if (Dumped < 2 && OutPlan.Motif == 0)
		{
			++Dumped;
			for (int32 DY_ = 0; DY_ < GridH; ++DY_)
			{
				FString Row;
				for (int32 DX_ = 0; DX_ < GridW; ++DX_)
				{
					const int32 I = VoxCell(DX_, DY_);
					if (OutPlan.L[I] == LVoid) { Row += TEXT("~"); }
					else if (OutPlan.LowEntry.Contains(I)) { Row += TEXT("l"); }
					else { Row += FString::Printf(TEXT("%d"), OutPlan.L[I]); }
				}
				UE_LOG(LogERWFC, Warning, TEXT("[VoxDiag] %s"), *Row);
			}
		}
		return false;
	}
}

// ---------------------------------------------------------------------------
// 实例化
// ---------------------------------------------------------------------------

UInstancedStaticMeshComponent* AWFC3DArenaGenerator_Voxel::GetMatComponent(EVoxMat Slot)
{
	const int32 Idx = static_cast<int32>(Slot);
	while (MatComponents.Num() <= Idx)
	{
		MatComponents.Add(nullptr);
	}
	if (MatComponents[Idx] && IsValid(MatComponents[Idx]))
	{
		return MatComponents[Idx];
	}

	UInstancedStaticMeshComponent* Comp = NewObject<UInstancedStaticMeshComponent>(
		this, *FString::Printf(TEXT("Vox_%d"), Idx));
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetStaticMesh(CubeMesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetMobility(EComponentMobility::Movable);

	switch (Slot)
	{
	case EVoxMat::Concrete: Comp->SetMaterial(0, MatConcrete); break;
	case EVoxMat::Marble: Comp->SetMaterial(0, MatMarble); break;
	case EVoxMat::Glow: if (DynMats.Num() > 0) { Comp->SetMaterial(0, DynMats[0]); } break;
	case EVoxMat::Glass: if (DynMats.Num() > 1) { Comp->SetMaterial(0, DynMats[1]); } break;
	default: break;
	}
	Comp->RegisterComponent();
	MatComponents[Idx] = Comp;
	return Comp;
}

void AWFC3DArenaGenerator_Voxel::AddBox(EVoxMat Slot, const FVector& Center, const FVector& Size)
{
	if (UInstancedStaticMeshComponent* Comp = GetMatComponent(Slot))
	{
		Comp->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.0f), false);
	}
}

float AWFC3DArenaGenerator_Voxel::ValueNoise3(float X, float Y, float Z) const
{
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const int32 Z0 = FMath::FloorToInt(Z);
	const float Fx = X - X0;
	const float Fy = Y - Y0;
	const float Fz = Z - Z0;
	auto Smooth = [](float T) { return T * T * (3.0f - 2.0f * T); };
	const float U = Smooth(Fx);
	const float V = Smooth(Fy);
	const float W = Smooth(Fz);
	auto Lat = [&](int32 A, int32 B, int32 C)
	{
		return (VoxHash(A, B, C, 613) & 0xFFFF) / 65535.0f;
	};
	const float C00 = FMath::Lerp(Lat(X0, Y0, Z0), Lat(X0 + 1, Y0, Z0), U);
	const float C10 = FMath::Lerp(Lat(X0, Y0 + 1, Z0), Lat(X0 + 1, Y0 + 1, Z0), U);
	const float C01 = FMath::Lerp(Lat(X0, Y0, Z0 + 1), Lat(X0 + 1, Y0, Z0 + 1), U);
	const float C11 = FMath::Lerp(Lat(X0, Y0 + 1, Z0 + 1), Lat(X0 + 1, Y0 + 1, Z0 + 1), U);
	return FMath::Lerp(FMath::Lerp(C00, C10, V), FMath::Lerp(C01, C11, V), W);
}

void AWFC3DArenaGenerator_Voxel::PartitionBlocks(
	FVoxPlan& Plan, int32 Seed, int32 ProbeCell) const
{
	// 中块切分: 1x1 / 2x1 / 1x2 / 2x2 拼合, 平台不再是大板+碎粒而是
	// 中等块拼缝; 独立随机流 (Seed+7919) 不影响地形/楼梯序列。
	FRandomStream BR(Seed + 7919);
	// Perlin 噪声原点偏移 (种子化)
	const float NoOX = (Seed % 1013) * 0.731f + 11.3f;
	const float NoOY = (Seed % 797) * 0.417f + 3.7f;
	Plan.Blocks.Reset();
	FMemory::Memset(Plan.BlockOf, 0xFF, sizeof(Plan.BlockOf));

	auto Fits = [&](int32 X, int32 Y, int32 W, int32 H, uint8 Lev) -> bool
	{
		if (X + W > GridW || Y + H > GridH)
		{
			return false;
		}
		for (int32 DY_ = 0; DY_ < H; ++DY_)
		{
			for (int32 DX_ = 0; DX_ < W; ++DX_)
			{
				const int32 I = VoxCell(X + DX_, Y + DY_);
				if (Plan.BlockOf[I] != -1 || Plan.L[I] != Lev)
				{
					return false;
				}
			}
		}
		return true;
	};

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const int32 Idx = VoxCell(X, Y);
			const uint8 Lev = Plan.L[Idx];
			if (Lev == LVoid || Plan.BlockOf[Idx] != -1)
			{
				continue;
			}
			const float R = BR.FRand();
			int32 W = 1;
			int32 H = 1;
			const bool bAlt = BR.FRand() < 0.5f;
			if (R < 0.45f)
			{
				// 2x2 -> 2x1/1x2 -> 1x1
				if (Fits(X, Y, 2, 2, Lev)) { W = 2; H = 2; }
				else if (!bAlt && Fits(X, Y, 2, 1, Lev)) { W = 2; H = 1; }
				else if (Fits(X, Y, 1, 2, Lev)) { W = 1; H = 2; }
				else if (bAlt && Fits(X, Y, 2, 1, Lev)) { W = 2; H = 1; }
			}
			else if (R < 0.80f)
			{
				const int32 AW = bAlt ? 2 : 1;
				const int32 AH = bAlt ? 1 : 2;
				const int32 BW = bAlt ? 1 : 2;
				const int32 BH = bAlt ? 2 : 1;
				if (Fits(X, Y, AW, AH, Lev)) { W = AW; H = AH; }
				else if (Fits(X, Y, BW, BH, Lev)) { W = BW; H = BH; }
			}

			FVoxBlock Bl;
			Bl.X = X;
			Bl.Y = Y;
			Bl.W = W;
			Bl.H = H;
			Bl.Lev = Lev;
			const float Cx = X + W * 0.5f;
			const float Cy = Y + H * 0.5f;

			bool bRole = (ProbeCell == Idx);
			for (int32 DY_ = 0; DY_ < H && !bRole; ++DY_)
			{
				for (int32 DX_ = 0; DX_ < W && !bRole; ++DX_)
				{
					const int32 I = VoxCell(X + DX_, Y + DY_);
					if (Plan.RoleCells.Contains(I) || I == ProbeCell)
					{
						bRole = true;
					}
				}
			}
			Bl.bPinned = bRole ? 1 : 0;
			// Perlin 双频起伏: 宏观 45cm + 微观 8cm+抖动; pinned 块锁 0
			const float Macro = 45.0f * FMath::PerlinNoise2D(
				FVector2D(Cx * 0.12f + NoOX, Cy * 0.12f + NoOY));
			const float Micro = 8.0f * FMath::PerlinNoise2D(
				FVector2D(Cx * 0.83f + NoOY, Cy * 0.83f + NoOX))
				+ BR.FRandRange(-4.0f, 4.0f); // 每块必抽, 固定流序
			Bl.TopOff = bRole ? 0.0f
				: FMath::Clamp(Macro + Micro, -55.0f, 55.0f);
			static const float BottomSteps[4] = { 0.0f, 200.0f, 400.0f, 600.0f };
			Bl.Bottom = -100.0f - BottomSteps[BR.RandRange(0, 3)];
			// 平台基底先全混凝土, 大理石以后只做点缀
			Bl.Mat = EVoxMat::Concrete;

			const int32 BIdx = Plan.Blocks.Add(Bl);
			for (int32 DY_ = 0; DY_ < H; ++DY_)
			{
				for (int32 DX_ = 0; DX_ < W; ++DX_)
				{
					Plan.BlockOf[VoxCell(X + DX_, Y + DY_)] =
						static_cast<int16>(BIdx);
				}
			}
		}
	}

	// 自检修复: 任何非空格必须落在合法块里 (异常值补 1x1 兜底块 + 诊断日志)
	for (int32 I = 0; I < GridW * GridH; ++I)
	{
		if (Plan.L[I] == LVoid)
		{
			continue;
		}
		const int32 B = Plan.BlockOf[I];
		if (B >= 0 && B < Plan.Blocks.Num())
		{
			continue;
		}
		UE_LOG(LogERWFC, Warning,
			TEXT("[Voxel] bad BlockOf[%d]=%d (L=%d, blocks=%d) -> 1x1 fallback block"),
			I, B, Plan.L[I], Plan.Blocks.Num());
		FVoxBlock Bl;
		Bl.X = I % GridW; Bl.Y = I / GridW; Bl.W = 1; Bl.H = 1;
		Bl.Lev = Plan.L[I]; Bl.Bottom = -100.0f; Bl.Mat = EVoxMat::Concrete;
		Plan.BlockOf[I] = static_cast<int16>(Plan.Blocks.Add(Bl));
	}

	// ---- nav 安全松弛: 同层相邻块 |Δh|<=30 (step 35 留余量) ----
	// 块邻接 = 同层且共享至少一条格边
	TSet<uint32> AdjPairs;
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const int32 A = Plan.BlockOf[VoxCell(X, Y)];
			if (A < 0 || A >= Plan.Blocks.Num())
			{
				continue;
			}
			for (int32 S = 1; S <= 2; ++S)
			{
				const int32 BX = X + VDX[S];
				const int32 BY = Y + VDY[S];
				if (BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = Plan.BlockOf[VoxCell(BX, BY)];
				if (B < 0 || B >= Plan.Blocks.Num() || B == A
					|| Plan.Blocks[B].Lev != Plan.Blocks[A].Lev)
				{
					continue;
				}
				AdjPairs.Add((A < B) ? ((A << 16) | B) : ((B << 16) | A));
			}
		}
	}
	TArray<TPair<int32, int32>> Adj;
	for (uint32 P : AdjPairs)
	{
		Adj.Add(TPair<int32, int32>(P >> 16, P & 0xFFFF));
	}
	for (int32 Pass = 0; Pass < 64; ++Pass)
	{
		bool bAny = false;
		for (TPair<int32, int32>& P : Adj)
		{
			for (int32 T = 0; T < 2; ++T)
			{
				FVoxBlock& B0 = Plan.Blocks[T ? P.Value : P.Key];
				const FVoxBlock& B1 = Plan.Blocks[T ? P.Key : P.Value];
				if (B0.bPinned)
				{
					continue;
				}
				const float NewH = FMath::Clamp(B0.TopOff,
					B1.TopOff - 30.0f, B1.TopOff + 30.0f);
				if (FMath::Abs(NewH - B0.TopOff) > 0.01f)
				{
					B0.TopOff = NewH;
					bAny = true;
				}
			}
		}
		if (!bAny)
		{
			break;
		}
	}
	// 整厘米化 + 统计
	float HMin = FLT_MAX;
	float HMax = -FLT_MAX;
	float MaxAdj = 0.0f;
	int32 Pinned = 0;
	for (FVoxBlock& B : Plan.Blocks)
	{
		B.TopOff = FMath::RoundToFloat(B.TopOff);
		HMin = FMath::Min(HMin, B.TopOff);
		HMax = FMath::Max(HMax, B.TopOff);
		Pinned += B.bPinned;
	}
	for (TPair<int32, int32>& P : Adj)
	{
		MaxAdj = FMath::Max(MaxAdj,
			FMath::Abs(Plan.Blocks[P.Key].TopOff - Plan.Blocks[P.Value].TopOff));
	}
	if (Plan.Blocks.Num() > 0)
	{
		if (MaxAdj > 30.0f)
		{
			UE_LOG(LogERWFC, Warning,
				TEXT("[Voxel] Relief: maxAdj %.0f exceeds nav-safe 30"), MaxAdj);
		}
		UE_LOG(LogERWFC, Log,
			TEXT("[Voxel] Relief: min=%.0f max=%.0f maxAdj=%.0f pinned=%d blocks=%d"),
			HMin, HMax, MaxAdj, Pinned, Plan.Blocks.Num());
	}
}

void AWFC3DArenaGenerator_Voxel::BuildCollision(
	const FVoxPlan& Plan, FRandomStream& Rng, int32 ProbeCell)
{
	if (!CollisionHISM)
	{
		CollisionHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			this, TEXT("VoxCollision"));
		CollisionHISM->SetupAttachment(GetRootComponent());
		CollisionHISM->SetStaticMesh(CubeMesh);
		CollisionHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionHISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		CollisionHISM->SetCanEverAffectNavigation(true);
		CollisionHISM->SetVisibility(false);
		CollisionHISM->RegisterComponent();
	}
	CollisionHISM->ClearInstances();

	auto AddCol = [&](const FVector& Center, const FVector& Size)
	{
		CollisionHISM->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.0f), false);
	};
	auto AddColRot = [&](const FRotator& Rot, const FVector& Center, const FVector& Size)
	{
		CollisionHISM->AddInstance(FTransform(Rot, Center, Size / 100.0f), false);
	};
	(void)AddColRot;

	TSet<int64> StairCells;
	for (const FVoxStair& St : Plan.Stairs)
	{
		StairCells.Add((static_cast<int64>(St.LowX) << 32) | St.LowY);
		StairCells.Add((static_cast<int64>(St.LowX + VDX[St.Dir]) << 32) | (St.LowY + VDY[St.Dir]));
	}
	(void)StairCells;

	// 平台实心柱: -100 .. L*200
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const uint8 L = Plan.L[VoxCell(X, Y)];
			if (L == LVoid)
			{
				continue;
			}
			// 顶面含共享块起伏 TopOff (25 < step 35, nav 连续)
			const float Top = L * LayerH
				+ Plan.Blocks[Plan.BlockOf[VoxCell(X, Y)]].TopOff;
			AddCol(FVector(X * CellSize + 200.0f, Y * CellSize + 200.0f,
				(Top - 100.0f) * 0.5f),
				FVector(400.0f, 400.0f, Top + 100.0f));
		}
	}

	// 楼梯碰撞: 8 级轴对齐台阶盒 (与可见台阶同足迹同顶面)。
	// 旋转斜坡盒 navmesh 烘不成连通面 (Atrium 已踩过坑), 台阶盒最稳:
	// 步高 25 < AgentMaxStepHeight, 最后一级顶面与高侧柱顶齐平。
	for (const FVoxStair& St : Plan.Stairs)
	{
		const float LowTop = Plan.L[VoxCell(St.LowX, St.LowY)] * LayerH;
		const float LX = St.LowX * CellSize + 200.0f;
		const float LY = St.LowY * CellSize + 200.0f;
		const float DxF = VDX[St.Dir];
		const float DyF = VDY[St.Dir];
		for (int32 I = 0; I < 8; ++I)
		{
			const float Along = -200.0f + (I + 0.5f) * 50.0f;
			const float Top = LowTop + (I + 1) * 25.0f;
			const FVector C(LX + DxF * Along, LY + DyF * Along, (Top - 100.0f) * 0.5f);
			const FVector Size = (St.Dir == 1 || St.Dir == 3)
				? FVector(50.0f, 400.0f, Top + 100.0f)
				: FVector(400.0f, 50.0f, Top + 100.0f);
			AddCol(C, Size);
		}
	}

	// 掩体: 每 ~12 个非楼梯格 1 个, 避开楼梯角色格 + 地面探针格
	TArray<int32> CoverCand;
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const int32 Idx = VoxCell(X, Y);
			if (Plan.L[Idx] == LVoid || Plan.RoleCells.Contains(Idx)
				|| Idx == ProbeCell)
			{
				continue;
			}
			CoverCand.Add(Idx);
		}
	}
	const int32 WantCover = CoverCand.Num() / 12;
	for (int32 I = CoverCand.Num() - 1; I > 0; --I)
	{
		CoverCand.Swap(I, Rng.RandRange(0, I));
	}
	for (int32 I = 0; I < WantCover && I < CoverCand.Num(); ++I)
	{
		const int32 X = CoverCand[I] % GridW;
		const int32 Y = CoverCand[I] / GridW;
		// 半格对齐掩体: 400x200 贴格半边, 底面含块起伏
		const float Top = Plan.L[VoxCell(X, Y)] * LayerH
			+ Plan.Blocks[Plan.BlockOf[VoxCell(X, Y)]].TopOff;
		const float H = (Rng.FRand() < 0.5f) ? 100.0f : 150.0f;
		const bool bXLong = Rng.FRand() < 0.5f;
		const float Half = (Rng.FRand() < 0.5f) ? -100.0f : 100.0f;
		const FVector C(X * CellSize + 200.0f + (bXLong ? 0.0f : Half),
			Y * CellSize + 200.0f + (bXLong ? Half : 0.0f),
			Top + H * 0.5f);
		const FVector Size = bXLong ? FVector(400.0f, 200.0f, H)
			: FVector(200.0f, 400.0f, H);
		AddCol(C, Size);
		// 掩体可见部分混凝土 (不可见碰撞已挡导航)
		AddBox(EVoxMat::Concrete, C, Size);
	}
}

void AWFC3DArenaGenerator_Voxel::BuildVisual(const FVoxPlan& Plan, FRandomStream& Rng)
{
	// ---- 1) 共享块切分: 每块一个盒, XY 缩 6cm 出拼缝, z Bottom..顶 ----
	for (const FVoxBlock& Bl : Plan.Blocks)
	{
		const float Top = Bl.Lev * LayerH + Bl.TopOff;
		const FVector Center((Bl.X + Bl.W * 0.5f) * CellSize,
			(Bl.Y + Bl.H * 0.5f) * CellSize,
			(Bl.Bottom + Top) * 0.5f);
		AddBox(Bl.Mat, Center,
			FVector(Bl.W * CellSize - 6.0f, Bl.H * CellSize - 6.0f,
				Top - Bl.Bottom));
	}

	// ---- 2) 崖面中板: 每面 30% 概率一块 200/400 宽板, 凸出 50-100 ----
	auto FaceBlock = [&](int32 X, int32 Y, int32 S, float LoZ, float HiZ)
	{
		if (Rng.FRand() >= 0.3f || HiZ - LoZ < 60.0f)
		{
			return;
		}
		const float Along = (Rng.FRand() < 0.5f) ? 200.0f : 400.0f;
		const float Thick = Rng.FRandRange(50.0f, 100.0f);
		const float Top = HiZ - Rng.FRandRange(0.0f, 50.0f);
		const float Height = FMath::Min(Top - LoZ,
			Rng.FRandRange(0.5f, 1.0f) * (HiZ - LoZ));
		const FVector Pos(
			X * CellSize + 200.0f + VDX[S] * (200.0f + Thick * 0.5f - 10.0f),
			Y * CellSize + 200.0f + VDY[S] * (200.0f + Thick * 0.5f - 10.0f),
			Top - Height * 0.5f);
		const float TOff = (Along < 400.0f)
			? ((Rng.FRand() < 0.5f) ? -100.0f : 100.0f) : 0.0f;
		FVector P2 = Pos;
		const int32 T = (S + 1) % 4;
		P2.X += VDX[T] * TOff;
		P2.Y += VDY[T] * TOff;
		const EVoxMat M = EVoxMat::Concrete;
		const FVector Size = (S == 1 || S == 3)
			? FVector(Thick, Along, Height)
			: FVector(Along, Thick, Height);
		AddBox(M, P2, Size);
	};

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const uint8 L = Plan.L[VoxCell(X, Y)];
			if (L == LVoid)
			{
				continue;
			}
			const float Top = L * LayerH
				+ Plan.Blocks[Plan.BlockOf[VoxCell(X, Y)]].TopOff;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 NX = X + VDX[S];
				const int32 NY = Y + VDY[S];
				float LoTop = -100.0f;
				bool bFace = false;
				if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH)
				{
					bFace = true;
				}
				else
				{
					const uint8 NL = Plan.L[VoxCell(NX, NY)];
					if (NL == LVoid)
					{
						bFace = true;
					}
					else if (NL < L)
					{
						bFace = true;
						LoTop = NL * LayerH
							+ Plan.Blocks[Plan.BlockOf[VoxCell(NX, NY)]].TopOff;
					}
				}
				if (bFace && Top > LoTop + 20.0f)
				{
					FaceBlock(X, Y, S, LoTop, Top);
				}
			}
		}
	}

	// ---- 3) 悬浮岛底部钟乳 ----
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			if (Plan.L[VoxCell(X, Y)] == LVoid)
			{
				continue;
			}
			bool bEdge = (X <= 1 || Y <= 1 || X >= GridW - 2 || Y >= GridH - 2);
			if (!bEdge)
			{
				for (int32 S = 0; S < 4 && !bEdge; ++S)
				{
					const int32 NX = X + VDX[S];
					const int32 NY = Y + VDY[S];
					if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH
						|| Plan.L[VoxCell(NX, NY)] == LVoid)
					{
						bEdge = true;
					}
				}
			}
			if (!bEdge)
			{
				continue;
			}
			const float R = FMath::Sqrt(FMath::Square(X - 9.0f) + FMath::Square(Y - 9.0f)) / 9.0f;
			const float Total = 200.0f + FMath::Clamp(1.0f - R, 0.0f, 1.0f) * 800.0f;
			// 从共享块 Bottom 起挂, 最小块 100 (400->200->100)
			float Z = Plan.Blocks[Plan.BlockOf[VoxCell(X, Y)]].Bottom;
			float Sz = 400.0f;
			float OX = X * CellSize + 200.0f;
			float OY = Y * CellSize + 200.0f;
			float Acc = 0.0f;
			while (Acc < Total && Sz >= 100.0f)
			{
				const float H = Sz * Rng.FRandRange(0.5f, 1.0f);
				AddBox(EVoxMat::Concrete,
					FVector(OX, OY, Z - H * 0.5f), FVector(Sz, Sz, H));
				Z -= H;
				Acc += H;
				const float NSz = Sz * 0.5f;
				OX += Rng.FRandRange(-(Sz - NSz) * 0.5f, (Sz - NSz) * 0.5f);
				OY += Rng.FRandRange(-(Sz - NSz) * 0.5f, (Sz - NSz) * 0.5f);
				Sz = NSz;
			}
		}
	}

	// ---- 4) 楼梯视觉: 8 级台阶, 25 高 x 50 深, 混凝土 ----
	for (const FVoxStair& St : Plan.Stairs)
	{
		const float LowTop = Plan.L[VoxCell(St.LowX, St.LowY)] * LayerH;
		const float LX = St.LowX * CellSize + 200.0f;
		const float LY = St.LowY * CellSize + 200.0f;
		for (int32 I = 0; I < 8; ++I)
		{
			// 第 i 级: 从低格远端边起 i*50 深, 顶面 LowTop+(i+1)*25
			const float Along = -200.0f + (I + 0.5f) * 50.0f;
			const float TopZ = LowTop + (I + 1) * 25.0f;
			const FVector C(
				LX + VDX[St.Dir] * Along, LY + VDY[St.Dir] * Along,
				(LowTop - 100.0f + TopZ) * 0.5f);
			const FVector Size = (St.Dir == 1 || St.Dir == 3)
				? FVector(50.0f, 400.0f, TopZ - LowTop + 100.0f)
				: FVector(400.0f, 50.0f, TopZ - LowTop + 100.0f);
			AddBox(EVoxMat::Concrete, C, Size);
		}
	}
}

void AWFC3DArenaGenerator_Voxel::BuildShell(
	const FVoxPlan& Plan, FRandomStream& Rng,
	TArray<FTransform>& OutConcrete, TArray<FTransform>& OutMarble,
	TArray<FTransform>& OutGlow, TArray<FTransform>& OutGlass)
{
	const FVector Center(3800.0f, 3800.0f, 0.0f);
	const float R = Plan.ShellR;

	auto MatPick = [&](float Fr) -> EVoxMat
	{
		// 外壳统一混凝土: 大理石/彩色渐变只留平台点缀 (美术要求)
		(void)Fr;
		return EVoxMat::Concrete;
	};
	auto MatPickKid = [&](float Fr) -> EVoxMat
	{
		(void)Fr;
		return EVoxMat::Concrete;
	};
	auto PushBox = [&](EVoxMat M, const FVector& C, float Sz, const FRotator& Rot)
	{
		const FTransform T(Rot, C, FVector(Sz / 100.0f));
		switch (M)
		{
		case EVoxMat::Concrete: OutConcrete.Add(T); break;
		case EVoxMat::Marble: OutMarble.Add(T); break;
		case EVoxMat::Glow: OutGlow.Add(T); break;
		case EVoxMat::Glass: OutGlass.Add(T); break;
		default: break;
		}
	};
	// 可玩包围盒: XY 0..7600 留 800 边距, z 不低于 -600;
	// 落进去的块沿径向推出方形外 800+ 处 (保持数量)
	auto PushOutside = [&Center](FVector& P)
	{
		if (P.Z <= -600.0f)
		{
			return;
		}
		const float DX_ = P.X - Center.X;
		const float DY_ = P.Y - Center.Y;
		const float M = FMath::Max(FMath::Abs(DX_), FMath::Abs(DY_));
		if (M <= 1.0f)
		{
			return;
		}
		// 方形半边 4600 (3800..8400), 目标 max-norm 6000 (>1400 边距)
		if (FMath::Abs(P.X - Center.X) < 4600.0f && FMath::Abs(P.Y - Center.Y) < 4600.0f)
		{
			const float T = 6000.0f / M;
			P.X = Center.X + DX_ * T;
			P.Y = Center.Y + DY_ * T;
		}
	};

	// 外壳带: 3D 抖动网格 (G=1000) 均匀布点 -> 密度有界不再稀疏
	const float G = 1000.0f;
	const float Extent = R * 1.3f;
	const int32 ShellStart = OutConcrete.Num() + OutMarble.Num() + OutGlass.Num();
	for (float OX = -Extent; OX < Extent; OX += G)
	{
		for (float OY = -Extent; OY < Extent; OY += G)
		{
			for (float OZ = -0.75f * R; OZ < 0.15f * R; OZ += G)
			{
				FVector P(Center.X + OX + G * 0.5f + Rng.FRandRange(-0.4f * G, 0.4f * G),
					Center.Y + OY + G * 0.5f + Rng.FRandRange(-0.4f * G, 0.4f * G),
					Center.Z + OZ + G * 0.5f + Rng.FRandRange(-0.4f * G, 0.4f * G));
				const float Dist = (P - Center).Size();
				const float SkipR = Rng.FRand();
				if (Dist < 0.92f * R || Dist > 1.25f * R || SkipR < 0.12f)
				{
					continue;
				}
				PushOutside(P);
				const float RF = Rng.FRand();
				float Mother = (RF < 0.70f) ? Rng.FRandRange(300.0f, 900.0f)
					: (RF < 0.95f) ? Rng.FRandRange(900.0f, 1500.0f)
					: Rng.FRandRange(1500.0f, 2200.0f);
				const EVoxMat M = MatPick(Rng.FRand());
				if (M == EVoxMat::Glass && Mother > 600.0f)
				{
					Mother = Rng.FRandRange(200.0f, 600.0f);
				}
				const FRotator MotherRot(Rng.FRandRange(-9.0f, 9.0f),
					Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(-9.0f, 9.0f));
				PushBox(M, P, Mother, MotherRot);
				// 子块: 1-3 个, 母块 0.4-0.6 倍 (>=150), 切向/径向偏移
				const int32 Kids = Rng.RandRange(1, 3);
				const FVector Dir = (P - Center).GetSafeNormal();
				const FVector T1 = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
				const FVector T2 = FVector::CrossProduct(Dir, T1).GetSafeNormal();
				for (int32 K = 0; K < Kids; ++K)
				{
					const float Sz = FMath::Max(150.0f,
						Mother * Rng.FRandRange(0.4f, 0.6f));
					FVector KP = P
						+ T1 * Rng.FRandRange(-0.5f * Mother, 0.5f * Mother)
						+ T2 * Rng.FRandRange(-0.5f * Mother, 0.5f * Mother)
						+ Dir * Rng.FRandRange(-0.3f * Mother, 0.3f * Mother);
					PushOutside(KP);
					const FRotator KRot(Rng.FRandRange(-20.0f, 20.0f),
						Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(-20.0f, 20.0f));
					PushBox(MatPickKid(Rng.FRand()), KP, Sz, KRot);
				}
			}
		}
	}

	// 远景巨柱: 剪影
	const int32 Pillars = Rng.RandRange(4, 8);
	for (int32 I = 0; I < Pillars; ++I)
	{
		const float Ang = Rng.FRandRange(0.0f, 2.0f * PI);
		const float Dist = R * Rng.FRandRange(1.2f, 1.5f);
		FVector P(Center.X + FMath::Cos(Ang) * Dist,
			Center.Y + FMath::Sin(Ang) * Dist,
			Rng.FRandRange(-2000.0f, -500.0f));
		PushOutside(P);
		const float W = Rng.FRandRange(400.0f, 800.0f);
		const float H = Rng.FRandRange(3000.0f, 5000.0f);
		const FTransform T(FRotator::ZeroRotator,
			FVector(P.X, P.Y, P.Z + H * 0.5f), FVector(W / 100.0f, W / 100.0f, H / 100.0f));
		OutConcrete.Add(T);
	}

	// 内侧碎屑: 抖动网格 (步长 900) 覆盖环带 [4800, 0.92R], 均匀 50%
	const float DG = 900.0f;
	const float DExtent = R * 0.92f;
	for (float OX = -DExtent; OX < DExtent; OX += DG)
	{
		for (float OY = -DExtent; OY < DExtent; OY += DG)
		{
			FVector P(Center.X + OX + DG * 0.5f + Rng.FRandRange(-0.4f * DG, 0.4f * DG),
				Center.Y + OY + DG * 0.5f + Rng.FRandRange(-0.4f * DG, 0.4f * DG),
				Rng.FRandRange(-2500.0f, 600.0f));
			const float HDist = FVector2D(P.X - Center.X, P.Y - Center.Y).Size();
			const float KeepR = Rng.FRand();
			if (HDist < 4800.0f || HDist > 0.92f * R || KeepR >= 0.5f)
			{
				continue;
			}
			PushOutside(P);
			PushBox(MatPickKid(Rng.FRand()), P, Rng.FRandRange(100.0f, 300.0f),
				FRotator::ZeroRotator);
		}
	}
	UE_LOG(LogERWFC, Log, TEXT("[Voxel] Shell instances: %d"),
		OutConcrete.Num() + OutMarble.Num() + OutGlass.Num() - ShellStart);
}

bool AWFC3DArenaGenerator_Voxel::Generate(const FWFCGenerationRequest& Request)
{
	LastResult = FWFCGenerationResult();
	ClearGenerated();
	DynMats.Reset();

	if (Request.Scale.X != GridW || Request.Scale.Y != GridH)
	{
		UE_LOG(LogERWFC, Warning,
			TEXT("[Voxel] Expects Scale 19x19, got %dx%dx%d; using 19x19."),
			Request.Scale.X, Request.Scale.Y, Request.Scale.Z);
	}

	const int32 Seed = (Request.Seed != 0) ? Request.Seed : FMath::Rand();
	const double StartTime = FPlatformTime::Seconds();

	// ---- 资产 ----
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogERWFC, Error, TEXT("[Voxel] No engine cube mesh."));
		return false;
	}
	MatConcrete = LoadMat(TEXT("/Game/Materials/Architecture/MI_Concrete_1.MI_Concrete_1"));
	MatMarble = LoadMat(TEXT("/Game/Materials/Architecture/MI_Marble_1.MI_Marble_1"));
	MatGlowBase = LoadMat(TEXT("/Game/Voxel/Materials/M_Voxel_Glow.M_Voxel_Glow"));
	MatGlassBase = LoadMat(TEXT("/Game/Voxel/Materials/M_Voxel_Glass.M_Voxel_Glass"));

	FRandomStream Rng(Seed);

	// 彩虹渐变: 7 色取两种不同色
	static const FLinearColor Rainbow[7] =
	{
		FLinearColor(1.0f, 0.1f, 0.1f), FLinearColor(1.0f, 0.45f, 0.05f),
		FLinearColor(1.0f, 0.9f, 0.1f), FLinearColor(0.1f, 1.0f, 0.3f),
		FLinearColor(0.1f, 0.9f, 1.0f), FLinearColor(0.15f, 0.3f, 1.0f),
		FLinearColor(0.6f, 0.15f, 1.0f)
	};
	const int32 CI0 = Rng.RandRange(0, 6);
	int32 CI1 = Rng.RandRange(0, 5);
	if (CI1 >= CI0) { ++CI1; }
	for (UMaterialInterface* Base : { MatGlowBase, MatGlassBase })
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("ColorA"), Rainbow[CI0]);
		MID->SetVectorParameterValue(TEXT("ColorB"), Rainbow[CI1]);
		DynMats.Add(MID);
	}

	// ---- 高度场 + 楼梯 (带重试) ----
	FVoxPlan Plan;
	Plan.ShellR = Rng.FRandRange(7000.0f, 9000.0f);
	int32 Tries = 0;
	bool bOk = false;
	for (; Tries < 30; ++Tries)
	{
		Plan.Stairs.Reset();
		if (BuildPlan(Seed, Rng, Plan))
		{
			bOk = true;
			break;
		}
	}
	if (!bOk)
	{
		UE_LOG(LogERWFC, Error, TEXT("[Voxel] Plan failed after %d tries (seed=%d)."), Tries, Seed);
		return false;
	}
	++Tries;

	// ---- 探针 ----
	int32 MaxHighL = -1;
	for (const FVoxStair& St : Plan.Stairs)
	{
		const int32 HX = St.LowX + VDX[St.Dir];
		const int32 HY = St.LowY + VDY[St.Dir];
		const int32 HighL = Plan.L[VoxCell(HX, HY)];
		const FVector Landing(HX * CellSize + 200.0f, HY * CellSize + 200.0f,
			HighL * LayerH + 20.0f);
		LastResult.LandingProbes.Add(Landing);
		if (HighL > MaxHighL)
		{
			MaxHighL = HighL;
			LastResult.UpperNavProbe = Landing;
		}
	}
	if (Plan.Stairs.Num() > 0)
	{
		const FVoxStair& St = Plan.Stairs[0];
		const float LowTop = Plan.L[VoxCell(St.LowX, St.LowY)] * LayerH;
		LastResult.StairMidProbe = FVector(
			(St.LowX + 0.5f + VDX[St.Dir] * 0.5f) * CellSize,
			(St.LowY + 0.5f + VDY[St.Dir] * 0.5f) * CellSize,
			LowTop + 100.0f);
	}
	// 地面探针: L=0 非角色格, 4 邻居也全是 L=0 非角色格, 离楼梯最远;
	// 找不到则放宽为仅自身条件
	int32 ProbeCell = INDEX_NONE;
	auto PickProbe = [&](bool bRequireFlatNeighbors) -> int32
	{
		int32 BestI = INDEX_NONE;
		float BestD = -1.0f;
		for (int32 Y = 0; Y < GridH; ++Y)
		{
			for (int32 X = 0; X < GridW; ++X)
			{
				const int32 I = VoxCell(X, Y);
				if (Plan.L[I] != 0 || Plan.RoleCells.Contains(I))
				{
					continue;
				}
				if (bRequireFlatNeighbors)
				{
					bool bOk = true;
					for (int32 S = 0; S < 4 && bOk; ++S)
					{
						const int32 NX = X + VDX[S];
						const int32 NY = Y + VDY[S];
						const int32 NI = (NX >= 0 && NY >= 0 && NX < GridW && NY < GridH)
							? VoxCell(NX, NY) : INDEX_NONE;
						if (NI == INDEX_NONE || Plan.L[NI] != 0
							|| Plan.RoleCells.Contains(NI))
						{
							bOk = false;
						}
					}
					if (!bOk)
					{
						continue;
					}
				}
				float MinD = FLT_MAX;
				for (const FVoxStair& St : Plan.Stairs)
				{
					const float D = FMath::Square(X - St.LowX) + FMath::Square(Y - St.LowY);
					MinD = FMath::Min(MinD, D);
				}
				if (MinD > BestD)
				{
					BestD = MinD;
					BestI = I;
				}
			}
		}
		return BestI;
	};
	ProbeCell = PickProbe(true);
	if (ProbeCell == INDEX_NONE)
	{
		ProbeCell = PickProbe(false);
	}
	if (ProbeCell != INDEX_NONE)
	{
		LastResult.GroundNavProbe = FVector(
			(ProbeCell % GridW) * CellSize + 200.0f,
			(ProbeCell / GridW) * CellSize + 200.0f, 20.0f);
	}

	// ---- 碰撞 + 可见实例 (共享块切分先行) ----
	PartitionBlocks(Plan, Seed, ProbeCell);
	BuildCollision(Plan, Rng, ProbeCell);
	BuildVisual(Plan, Rng);
	SpawnLighting(Rng);

	TArray<FTransform> ShConcrete, ShMarble, ShGlow, ShGlass;
	BuildShell(Plan, Rng, ShConcrete, ShMarble, ShGlow, ShGlass);
	if (UInstancedStaticMeshComponent* C = GetMatComponent(EVoxMat::Concrete))
	{
		C->AddInstances(ShConcrete, false);
	}
	if (UInstancedStaticMeshComponent* C = GetMatComponent(EVoxMat::Marble))
	{
		C->AddInstances(ShMarble, false);
	}
	if (UInstancedStaticMeshComponent* C = GetMatComponent(EVoxMat::Glow))
	{
		C->AddInstances(ShGlow, false);
	}
	if (UInstancedStaticMeshComponent* C = GetMatComponent(EVoxMat::Glass))
	{
		C->AddInstances(ShGlass, false);
	}

	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	int32 Counts[4] = { 0, 0, 0, 0 };
	for (int32 I = 0; I < MatComponents.Num(); ++I)
	{
		if (MatComponents[I])
		{
			Counts[I] = MatComponents[I]->GetInstanceCount();
		}
	}

	LastResult.bSuccess = true;
	LastResult.UsedSeed = Seed;
	LastResult.UsedScale = FIntVector(GridW, GridH, 2);
	LastResult.CellWorldSize = GetCellWorldSize();
	LastResult.ModuleCount = 1;
	LastResult.CellCount = GridW * GridH * 2;
	LastResult.DurationMs = DurationMs;

	UE_LOG(LogERWFC, Log,
		TEXT("[Voxel] Generated seed=%d motif=%s tries=%d stairs=%d shellR=%.0f "
			"instances: concrete=%d marble=%d glow=%d glass=%d in %.2f ms."),
		Seed, Plan.Motif == 0 ? TEXT("Bowl") : TEXT("Ziggurat"), Tries,
		Plan.Stairs.Num(), Plan.ShellR,
		Counts[0], Counts[1], Counts[2], Counts[3], DurationMs);
	return true;
}

// ---------------------------------------------------------------------------
// 氛围: 单一主光 + 微弱冷补光 + 下方雾池 (参考"漂浮沙盘"目标)
// ---------------------------------------------------------------------------

void AWFC3DArenaGenerator_Voxel::SpawnLighting(FRandomStream& Rng)
{
	// 与 Nature 共用一份氛围实现 (三点光+虚空雾, -Atmo 开启)
	ER::Atmosphere::SpawnVoidAtmosphere(this, Rng, AtmoActors);
}
