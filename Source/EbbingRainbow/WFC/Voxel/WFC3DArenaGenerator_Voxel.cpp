// Copyright Epic Games, Inc. All Rights Reserved.

#include "Voxel/WFC3DArenaGenerator_Voxel.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"

namespace
{
	const int32 DX[4] = { 0, 1, 0, -1 };
	const int32 DY[4] = { 1, 0, -1, 0 };

	FORCEINLINE int32 CellIndex(int32 X, int32 Y) { return Y * 19 + X; }

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
}

UMaterialInterface* AWFC3DArenaGenerator_Voxel::LoadMat(const TCHAR* Path)
{
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!M)
	{
		M = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UE_LOG(LogTemp, Warning,
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
		E = (R < 0.35f) ? 0 : (R < 0.6f) ? 1 : (R < 0.85f) ? 2 : 3;
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
	const int32 MaxL = (Motif == 1) ? Rng.RandRange(2, 3) : 3;

	uint8 Dom[GridW * GridH];
	FMemory::Memset(Dom, DomAll, sizeof(Dom));

	auto PopCount = [](uint8 M) { return FMath::CountBits(static_cast<uint32>(M)); };

	// 弧相容: 从 Cell 域裁剪出发, 把约束沿 4 邻传播
	auto Propagate = [&](int32 StartIdx) -> bool
	{
		TArray<int32> Queue;
		Queue.Add(StartIdx);
		TBitArray<> InQ;
		InQ.Init(false, GridW * GridH);
		InQ[StartIdx] = true;
		while (Queue.Num() > 0)
		{
			const int32 A = Queue.Pop();
			InQ[A] = false;
			const int32 AX = A % GridW;
			const int32 AY = A / GridW;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + DX[S];
				const int32 BY = AY + DY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = CellIndex(BX, BY);
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

	for (int32 Settled = 0; Settled < GridW * GridH; ++Settled)
	{
		// 最小熵未定格 + 小随机扰动
		int32 Best = INDEX_NONE;
		float BestScore = FLT_MAX;
		for (int32 I = 0; I < GridW * GridH; ++I)
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

		const int32 X = Best % GridW;
		const int32 Y = Best / GridW;

		// 已坍缩同标签邻居数 -> 权重 x3/个, 促成大平台
		int32 SameN[5] = { 0, 0, 0, 0, 0 };
		for (int32 S = 0; S < 4; ++S)
		{
			const int32 NX = X + DX[S];
			const int32 NY = Y + DY[S];
			if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH)
			{
				continue;
			}
			const uint8 ND = Dom[CellIndex(NX, NY)];
			if (PopCount(ND) == 1)
			{
				const int32 NL = FMath::FloorLog2(ND);
				++SameN[NL];
			}
		}

		float Weights[5];
		float Total = 0.0f;
		for (int32 Lb = 0; Lb < 5; ++Lb)
		{
			Weights[Lb] = ((Dom[Best] & (1 << Lb)) == 0) ? 0.0f
				: PriorWeight(Motif, MaxL, Cx, Cy, X, Y, Lb)
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

	for (int32 I = 0; I < GridW * GridH; ++I)
	{
		const int32 Lb = FMath::FloorLog2(Dom[I]);
		OutL[I] = (Lb == 0) ? LVoid : static_cast<uint8>(Lb - 1);
	}
	return true;
}

void AWFC3DArenaGenerator_Voxel::FixupSmallRegions(uint8* L)
{
	// <3 格的同高连通块并入相邻较低的块
	bool bChanged = true;
	for (int32 Guard = 0; Guard < 8 && bChanged; ++Guard)
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
					const int32 BX = AX + DX[S];
					const int32 BY = AY + DY[S];
					if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
					{
						continue;
					}
					const int32 B = CellIndex(BX, BY);
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
			if (L[I] == LVoid || RegionSize[Region[I]] >= 3)
			{
				continue;
			}
			// 找相邻最低的非 Void 层并入
			const int32 AX = I % GridW;
			const int32 AY = I / GridW;
			int32 BestL = -1;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 BX = AX + DX[S];
				const int32 BY = AY + DY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const uint8 NL = L[CellIndex(BX, BY)];
				if (NL != LVoid && NL < L[I] && (BestL < 0 || NL < BestL))
				{
					BestL = NL;
				}
			}
			if (BestL >= 0)
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
				const int32 A = CellIndex(X, Y);
				if (L[A] == LVoid)
				{
					continue;
				}
				for (int32 S = 1; S <= 2; ++S)
				{
					const int32 BX = X + DX[S];
					const int32 BY = Y + DY[S];
					if (BX >= GridW || BY >= GridH)
					{
						continue;
					}
					const int32 B = CellIndex(BX, BY);
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
}

bool AWFC3DArenaGenerator_Voxel::PlaceStairs(FRandomStream& Rng, FVoxPlan& Plan)
{
	const int32 N = GridW * GridH;
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
				const int32 BX = AX + DX[S];
				const int32 BY = AY + DY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 B = CellIndex(BX, BY);
				if (Plan.RegionOf[B] == -1 && Plan.L[B] == Plan.L[A])
				{
					Plan.RegionOf[B] = Rid;
					Stack.Add(B);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[VoxelDbg] regions=%d"), RegionSize.Num());

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
				const int32 BX = X + DX[S];
				const int32 BY = Y + DY[S];
				if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
				{
					continue;
				}
				const int32 A = CellIndex(X, Y);
				const int32 B = CellIndex(BX, BY);
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

	UE_LOG(LogTemp, Log, TEXT("[VoxelDbg] adj=%d"), Adjs.Num());

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

	UE_LOG(LogTemp, Log, TEXT("[VoxelDbg] chosen=%d leftover=%d"), Chosen.Num(), Leftover.Num());

	// 每个邻接在共享边中随机选一条; 每低侧格最多一部楼梯, 入口格不能 Void/更高
	TSet<int64> UsedLow;
	for (int32 E : Chosen)
	{
		TArray<FVoxStair> Cands = Adjs[E].Edges;
		for (int32 I = Cands.Num() - 1; I > 0; --I)
		{
			Cands.Swap(I, Rng.RandRange(0, I));
		}
		bool bPlaced = false;
		for (const FVoxStair& St : Cands)
		{
			const int64 LowKey = (static_cast<int64>(St.LowX) << 32) | St.LowY;
			if (UsedLow.Contains(LowKey))
			{
				continue;
			}
			// 入口 = 低侧格反方向的邻居
			const int32 EX = St.LowX - DX[St.Dir];
			const int32 EY = St.LowY - DY[St.Dir];
			if (EX < 0 || EY < 0 || EX >= GridW || EY >= GridH)
			{
				continue;
			}
			const uint8 EL = Plan.L[CellIndex(EX, EY)];
			if (EL == LVoid || EL > Plan.L[CellIndex(St.LowX, St.LowY)])
			{
				continue;
			}
			UsedLow.Add(LowKey);
			Plan.Stairs.Add(St);
			bPlaced = true;
			break;
		}
		// 找不到合法边就跳过: BFS 校验会兜住连通性
		(void)bPlaced;
	}
	return Plan.Stairs.Num() > 0;
}

bool AWFC3DArenaGenerator_Voxel::CheckConnectivity(const FVoxPlan& Plan) const
{
	const int32 N = GridW * GridH;
	TArray<uint8> Visited;
	Visited.Init(0, N);

	// 楼梯边查找表
	TSet<int64> StairEdge;
	for (const FVoxStair& St : Plan.Stairs)
	{
		const int32 Lo = CellIndex(St.LowX, St.LowY);
		const int32 Hi = CellIndex(St.LowX + DX[St.Dir], St.LowY + DY[St.Dir]);
		const int64 K1 = (static_cast<int64>(Lo) << 32) | Hi;
		const int64 K2 = (static_cast<int64>(Hi) << 32) | Lo;
		StairEdge.Add(K1);
		StairEdge.Add(K2);
	}

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
			const int32 BX = AX + DX[S];
			const int32 BY = AY + DY[S];
			if (BX < 0 || BY < 0 || BX >= GridW || BY >= GridH)
			{
				continue;
			}
			const int32 B = CellIndex(BX, BY);
			if (Visited[B] || Plan.L[B] == LVoid)
			{
				continue;
			}
			const int64 Key = (static_cast<int64>(A) << 32) | B;
			if (Plan.L[A] != Plan.L[B] && !StairEdge.Contains(Key))
			{
				continue;
			}
			Visited[B] = 1;
			Stack.Add(B);
		}
	}
	return Count == NonVoid;
}

bool AWFC3DArenaGenerator_Voxel::BuildPlan(
	int32 Seed, FRandomStream& Rng, FVoxPlan& OutPlan)
{
	OutPlan.Motif = Rng.RandRange(0, 1);
	const float Cx = 9.0f + Rng.RandRange(-2, 2);
	const float Cy = 9.0f + Rng.RandRange(-2, 2);

	if (!CollapseHeightField(Rng, OutPlan.Motif, Cx, Cy, OutPlan.L))
	{
		return false;
	}
	FixupSmallRegions(OutPlan.L);

	// L=0 至少 25%
	int32 NonVoid = 0;
	int32 L0 = 0;
	for (int32 I = 0; I < GridW * GridH; ++I)
	{
		if (OutPlan.L[I] != LVoid)
		{
			++NonVoid;
			if (OutPlan.L[I] == 0) { ++L0; }
		}
	}
	if (NonVoid == 0 || L0 * 4 < NonVoid)
	{
		return false;
	}

	// 出生侧: x<=4 与 x>=14 各 >=8 个非 Void 格
	int32 West = 0;
	int32 East = 0;
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			if (OutPlan.L[CellIndex(X, Y)] == LVoid)
			{
				continue;
			}
			if (X <= 4) { ++West; }
			if (X >= 14) { ++East; }
		}
	}
	if (West < 8 || East < 8)
	{
		return false;
	}

	if (!PlaceStairs(Rng, OutPlan) || !CheckConnectivity(OutPlan))
	{
		return false;
	}
	return true;
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

void AWFC3DArenaGenerator_Voxel::BuildCollision(const FVoxPlan& Plan, FRandomStream& Rng)
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
		StairCells.Add((static_cast<int64>(St.LowX + DX[St.Dir]) << 32) | (St.LowY + DY[St.Dir]));
	}

	// 平台实心柱: -100 .. L*200
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const uint8 L = Plan.L[CellIndex(X, Y)];
			if (L == LVoid)
			{
				continue;
			}
			const float Top = L * LayerH;
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
		const float LowTop = Plan.L[CellIndex(St.LowX, St.LowY)] * LayerH;
		const float LX = St.LowX * CellSize + 200.0f;
		const float LY = St.LowY * CellSize + 200.0f;
		const float DxF = DX[St.Dir];
		const float DyF = DY[St.Dir];
		for (int32 I = 0; I < 8; ++I)
		{
			const float Along = -200.0f + (I + 0.5f) * 50.0f;
			const float Top = LowTop + (I + 1) * 25.0f;
			const FVector C(LX + DxF * Along, LY + DyF * Along, (Top - 100.0f) * 0.5f);
			const FVector Size = (St.Dir == 1 || St.Dir == 3)
				? FVector(50.0f, 300.0f, Top + 100.0f)
				: FVector(300.0f, 50.0f, Top + 100.0f);
			AddCol(C, Size);
		}
	}

	// 掩体: 每 ~12 个非楼梯格 1 个, 避开楼梯格/入口/落点
	TArray<int32> CoverCand;
	TSet<int64> Forbidden = StairCells;
	for (const FVoxStair& St : Plan.Stairs)
	{
		const int32 EX = St.LowX - DX[St.Dir];
		const int32 EY = St.LowY - DY[St.Dir];
		if (EX >= 0 && EY >= 0 && EX < GridW && EY < GridH)
		{
			Forbidden.Add((static_cast<int64>(EX) << 32) | EY);
		}
	}
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			if (Plan.L[CellIndex(X, Y)] == LVoid)
			{
				continue;
			}
			if (Forbidden.Contains((static_cast<int64>(X) << 32) | Y))
			{
				continue;
			}
			CoverCand.Add(CellIndex(X, Y));
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
		const float Top = Plan.L[CellIndex(X, Y)] * LayerH;
		const float W = Rng.FRandRange(100.0f, 200.0f);
		const float H = (Rng.FRand() < 0.5f) ? 100.0f : 150.0f;
		const FVector C(
			X * CellSize + 200.0f + Rng.FRandRange(-50.0f, 50.0f),
			Y * CellSize + 200.0f + Rng.FRandRange(-50.0f, 50.0f),
			Top + H * 0.5f);
		AddCol(C, FVector(W, W, H));
		// 掩体可见部分用大理石 (不可见碰撞已挡导航)
		AddBox(EVoxMat::Marble, C, FVector(W, W, H));
	}
}

void AWFC3DArenaGenerator_Voxel::BuildVisual(const FVoxPlan& Plan, FRandomStream& Rng)
{
	// ---- 1) 同高矩形合并: 顶板 + 柱身 ----
	for (int32 Lev = 0; Lev <= 3; ++Lev)
	{
		uint8 Used[GridW * GridH];
		FMemory::Memset(Used, 0, sizeof(Used));
		for (int32 Y = 0; Y < GridH; ++Y)
		{
			for (int32 X = 0; X < GridW; ++X)
			{
				const int32 Idx = CellIndex(X, Y);
				if (Used[Idx] || Plan.L[Idx] != Lev)
				{
					continue;
				}
				// 贪心最大矩形
				int32 RW = 0;
				while (X + RW < GridW && !Used[CellIndex(X + RW, Y)]
					&& Plan.L[CellIndex(X + RW, Y)] == Lev)
				{
					++RW;
				}
				int32 RH = 1;
				for (;;)
				{
					if (Y + RH >= GridH)
					{
						break;
					}
					bool bRow = true;
					for (int32 K = 0; K < RW; ++K)
					{
						const int32 J = CellIndex(X + K, Y + RH);
						if (Used[J] || Plan.L[J] != Lev)
						{
							bRow = false;
							break;
						}
					}
					if (!bRow) { break; }
					++RH;
				}
				for (int32 K = 0; K < RW; ++K)
				{
					for (int32 J = 0; J < RH; ++J)
					{
						Used[CellIndex(X + K, Y + J)] = 1;
					}
				}

				const float Top = Lev * LayerH;
				const FVector Center(
					(X + RW * 0.5f) * CellSize, (Y + RH * 0.5f) * CellSize, Top - 50.0f);
				// L=0 偏混凝土 70%, 高层偏大理石 70%
				const EVoxMat SlabMat = (Lev == 0)
					? ((Rng.FRand() < 0.7f) ? EVoxMat::Concrete : EVoxMat::Marble)
					: ((Rng.FRand() < 0.7f) ? EVoxMat::Marble : EVoxMat::Concrete);
				AddBox(SlabMat, Center, FVector(RW * CellSize, RH * CellSize, 100.0f));
				if (Lev > 0)
				{
					AddBox(EVoxMat::Concrete,
						FVector(Center.X, Center.Y, Lev * 100.0f - 100.0f),
						FVector(RW * CellSize, RH * CellSize, Lev * LayerH));
				}
			}
		}
	}

	// ---- 2) 崖面挤出小块 ----
	auto FaceBlock = [&](int32 X, int32 Y, int32 S, float LoZ, float HiZ)
	{
		const int32 N = Rng.RandRange(0, 3);
		for (int32 I = 0; I < N; ++I)
		{
			const float Sz = (Rng.FRand() < 0.5f) ? 50.0f : 100.0f;
			const float Out = Rng.FRandRange(10.0f, 40.0f);
			const float Along = Rng.FRandRange(Sz * 0.5f, CellSize - Sz * 0.5f);
			const float Z = Rng.FRandRange(LoZ + Sz * 0.5f, HiZ - Sz * 0.5f);
			if (Z <= LoZ + Sz * 0.5f || Z >= HiZ - Sz * 0.5f)
			{
				continue;
			}
			// 面心 = 格边中点沿 dir 移出
			const float FX = X * CellSize + 200.0f + DX[S] * (200.0f + Out);
			const float FY = Y * CellSize + 200.0f + DY[S] * (200.0f + Out);
			FVector Pos(FX, FY, Z);
			// 沿面切向偏移
			const int32 T = (S + 1) % 4;
			Pos.X += DX[T] * (Along - 200.0f);
			Pos.Y += DY[T] * (Along - 200.0f);
			const float R = Rng.FRand();
			const EVoxMat M = (R < 0.05f) ? EVoxMat::Glow : (R < 0.08f) ? EVoxMat::Glass : EVoxMat::Concrete;
			const FVector Size = (S == 1 || S == 3)
				? FVector(Out + Sz * 0.5f, Sz, Sz)
				: FVector(Sz, Out + Sz * 0.5f, Sz);
			AddBox(M, Pos, Size);
		}
	};

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const uint8 L = Plan.L[CellIndex(X, Y)];
			if (L == LVoid)
			{
				continue;
			}
			const float Top = L * LayerH;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 NX = X + DX[S];
				const int32 NY = Y + DY[S];
				float LoTop = -100.0f;
				bool bFace = false;
				if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH)
				{
					bFace = true;
				}
				else
				{
					const uint8 NL = Plan.L[CellIndex(NX, NY)];
					if (NL == LVoid)
					{
						bFace = true;
					}
					else if (NL < L)
					{
						bFace = true;
						LoTop = NL * LayerH;
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
			if (Plan.L[CellIndex(X, Y)] == LVoid)
			{
				continue;
			}
			bool bEdge = (X <= 1 || Y <= 1 || X >= GridW - 2 || Y >= GridH - 2);
			if (!bEdge)
			{
				for (int32 S = 0; S < 4 && !bEdge; ++S)
				{
					const int32 NX = X + DX[S];
					const int32 NY = Y + DY[S];
					if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH
						|| Plan.L[CellIndex(NX, NY)] == LVoid)
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
			float Z = -100.0f;
			float Sz = 400.0f;
			float OX = X * CellSize + 200.0f;
			float OY = Y * CellSize + 200.0f;
			float Acc = 0.0f;
			while (Acc < Total && Sz >= 40.0f)
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

	// ---- 4) 楼梯视觉: 8 级台阶, 25 高 x 50 深 x 300 宽 ----
	for (const FVoxStair& St : Plan.Stairs)
	{
		const float LowTop = Plan.L[CellIndex(St.LowX, St.LowY)] * LayerH;
		const float LX = St.LowX * CellSize + 200.0f;
		const float LY = St.LowY * CellSize + 200.0f;
		for (int32 I = 0; I < 8; ++I)
		{
			// 第 i 级: 从低格远端边起 i*50 深, 顶面 LowTop+(i+1)*25
			const float Along = -200.0f + (I + 0.5f) * 50.0f;
			const float TopZ = LowTop + (I + 1) * 25.0f;
			const FVector C(
				LX + DX[St.Dir] * Along, LY + DY[St.Dir] * Along,
				(LowTop - 100.0f + TopZ) * 0.5f);
			const FVector Size = (St.Dir == 1 || St.Dir == 3)
				? FVector(50.0f, 300.0f, TopZ - LowTop + 100.0f)
				: FVector(300.0f, 50.0f, TopZ - LowTop + 100.0f);
			AddBox(EVoxMat::Marble, C, Size);
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
		if (Fr < 0.75f) { return EVoxMat::Concrete; }
		if (Fr < 0.92f) { return EVoxMat::Marble; }
		return (Fr < 0.96f) ? EVoxMat::Glow : EVoxMat::Glass;
	};
	auto PushBox = [&](EVoxMat M, const FVector& C, float Sz)
	{
		const FTransform T(FRotator::ZeroRotator, C, FVector(Sz / 100.0f));
		switch (M)
		{
		case EVoxMat::Concrete: OutConcrete.Add(T); break;
		case EVoxMat::Marble: OutMarble.Add(T); break;
		case EVoxMat::Glow: OutGlow.Add(T); break;
		case EVoxMat::Glass: OutGlass.Add(T); break;
		default: break;
		}
	};
	// 可玩包围盒: XY 0..7600 留 800 边距, z 不低于 -600
	auto OutsidePlayable = [](const FVector& P)
	{
		return P.X < -800.0f || P.X > 8400.0f || P.Y < -800.0f || P.Y > 8400.0f
			|| P.Z < -600.0f;
	};

	// Fibonacci 采样 + 3D 噪声碎裂
	const int32 Samples = 600;
	const float Golden = PI * (3.0f - FMath::Sqrt(5.0f));
	for (int32 I = 0; I < Samples; ++I)
	{
		const float Z = 1.0f - 2.0f * (I + 0.5f) / Samples;
		const float Rad = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
		const float Theta = Golden * I;
		const FVector Dir(Rad * FMath::Cos(Theta), Rad * FMath::Sin(Theta), Z);
		const FVector P = Center + Dir * R;
		if (P.Z > 0.35f * R || !OutsidePlayable(P))
		{
			continue;
		}
		// 噪声阈值保留 ~35%
		if (ValueNoise3(Dir.X * 2.3f + 7.0f, Dir.Y * 2.3f + 3.0f, Dir.Z * 2.3f + 11.0f) < 0.45f)
		{
			continue;
		}
		const float Mother = Rng.FRandRange(400.0f, 1200.0f);
		const EVoxMat M = MatPick(Rng.FRand());
		PushBox(M, P, Mother);
		// 子块簇: 3-12 个, 边长逐级减半, 沿切向偏移
		const int32 Kids = Rng.RandRange(3, 12);
		const FVector T1 = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		const FVector T2 = FVector::CrossProduct(Dir, T1).GetSafeNormal();
		float Sz = Mother;
		for (int32 K = 0; K < Kids; ++K)
		{
			Sz *= 0.5f;
			const FVector Off = T1 * Rng.FRandRange(-Mother, Mother) * 0.4f
				+ T2 * Rng.FRandRange(-Mother, Mother) * 0.4f
				+ Dir * Rng.FRandRange(-Mother * 0.3f, Mother * 0.3f);
			const FVector KP = P + Off;
			if (OutsidePlayable(KP))
			{
				PushBox(MatPick(Rng.FRand()), KP, FMath::Max(Sz, 50.0f));
			}
		}
	}

	// 远景巨柱: 剪影
	const int32 Pillars = Rng.RandRange(4, 8);
	for (int32 I = 0; I < Pillars; ++I)
	{
		const float Ang = Rng.FRandRange(0.0f, 2.0f * PI);
		const float Dist = R * Rng.FRandRange(1.2f, 1.5f);
		const FVector P(Center.X + FMath::Cos(Ang) * Dist,
			Center.Y + FMath::Sin(Ang) * Dist,
			Rng.FRandRange(-2000.0f, -500.0f));
		const float W = Rng.FRandRange(400.0f, 800.0f);
		const float H = Rng.FRandRange(3000.0f, 5000.0f);
		if (!OutsidePlayable(FVector(P.X, P.Y, 0.0f)))
		{
			continue;
		}
		const FTransform T(FRotator::ZeroRotator,
			FVector(P.X, P.Y, P.Z + H * 0.5f), FVector(W / 100.0f, W / 100.0f, H / 100.0f));
		if (Rng.FRand() < 0.8f)
		{
			OutConcrete.Add(T);
		}
		else
		{
			OutMarble.Add(T);
		}
	}

	// 漂浮碎屑
	const int32 Debris = Rng.RandRange(150, 250);
	for (int32 I = 0; I < Debris; ++I)
	{
		const float Ang = Rng.FRandRange(0.0f, 2.0f * PI);
		const float Dist = Rng.FRandRange(4200.0f, R * 0.9f);
		const FVector P(Center.X + FMath::Cos(Ang) * Dist,
			Center.Y + FMath::Sin(Ang) * Dist,
			Rng.FRandRange(-3000.0f, 2000.0f));
		if (!OutsidePlayable(P))
		{
			continue;
		}
		PushBox(MatPick(Rng.FRand()), P, Rng.FRandRange(25.0f, 100.0f));
	}
}

bool AWFC3DArenaGenerator_Voxel::Generate(const FWFCGenerationRequest& Request)
{
	LastResult = FWFCGenerationResult();
	ClearGenerated();
	DynMats.Reset();

	if (Request.Scale.X != GridW || Request.Scale.Y != GridH)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Voxel] Expects Scale 19x19, got %dx%dx%d; using 19x19."),
			Request.Scale.X, Request.Scale.Y, Request.Scale.Z);
	}

	const int32 Seed = (Request.Seed != 0) ? Request.Seed : FMath::Rand();
	const double StartTime = FPlatformTime::Seconds();

	// ---- 资产 ----
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[Voxel] No engine cube mesh."));
		return false;
	}
	MatConcrete = LoadMat(TEXT("/Game/Voxel/Materials/M_Voxel_Concrete.M_Voxel_Concrete"));
	MatMarble = LoadMat(TEXT("/Game/Voxel/Materials/M_Voxel_Marble.M_Voxel_Marble"));
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
		UE_LOG(LogTemp, Error, TEXT("[Voxel] Plan failed after %d tries (seed=%d)."), Tries, Seed);
		return false;
	}
	++Tries;

	// ---- 探针 ----
	int32 MaxHighL = -1;
	for (const FVoxStair& St : Plan.Stairs)
	{
		const int32 HX = St.LowX + DX[St.Dir];
		const int32 HY = St.LowY + DY[St.Dir];
		const int32 HighL = Plan.L[CellIndex(HX, HY)];
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
		const float LowTop = Plan.L[CellIndex(St.LowX, St.LowY)] * LayerH;
		LastResult.StairMidProbe = FVector(
			(St.LowX + 0.5f + DX[St.Dir] * 0.5f) * CellSize,
			(St.LowY + 0.5f + DY[St.Dir] * 0.5f) * CellSize,
			LowTop + 100.0f);
	}
	{
		// 地面探针: L=0 且离楼梯最远的格
		int32 BestI = INDEX_NONE;
		float BestD = -1.0f;
		for (int32 Y = 0; Y < GridH; ++Y)
		{
			for (int32 X = 0; X < GridW; ++X)
			{
				if (Plan.L[CellIndex(X, Y)] != 0)
				{
					continue;
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
					BestI = CellIndex(X, Y);
				}
			}
		}
		if (BestI != INDEX_NONE)
		{
			LastResult.GroundNavProbe = FVector(
				(BestI % GridW) * CellSize + 200.0f,
				(BestI / GridW) * CellSize + 200.0f, 20.0f);
		}
	}

	// ---- 碰撞 + 可见实例 ----
	BuildCollision(Plan, Rng);
	BuildVisual(Plan, Rng);

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

	UE_LOG(LogTemp, Log,
		TEXT("[Voxel] Generated seed=%d motif=%s tries=%d stairs=%d shellR=%.0f "
			"instances: concrete=%d marble=%d glow=%d glass=%d in %.2f ms."),
		Seed, Plan.Motif == 0 ? TEXT("Bowl") : TEXT("Ziggurat"), Tries,
		Plan.Stairs.Num(), Plan.ShellR,
		Counts[0], Counts[1], Counts[2], Counts[3], DurationMs);
	return true;
}
