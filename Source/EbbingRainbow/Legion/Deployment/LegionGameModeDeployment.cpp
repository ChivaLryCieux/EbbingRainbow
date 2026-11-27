// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/LegionBattle/LegionGameMode.h"

#include "Campaign/GalacticCampaignSubsystem.h"
#include "Core/ERAssetCatalog.h"
#include "Core/ERLog.h"
#include "Experimental/LegionBattle/LegionForest.h"
#include "Experimental/LegionBattle/LegionUnit.h"

#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TextureResource.h"

void ALegionGameMode::LoadRuntimeAssets()
{
	UERAssetCatalog* Catalog = UERAssetCatalog::Get(this);
	UMaterialInterface* BasicMat = Catalog ? Catalog->LoadAsset<UMaterialInterface>(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")) : nullptr;

	if (UMaterialInterface* Base = AllyMaterial.IsNull() ? BasicMat : AllyMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.92f, 0.90f, 1.0f));
		CachedAllyMat = MID;
	}

	// 敌方统一使用高亮红曜石色 (如果战役合同指定敌方势力则叠势力色)
	FLinearColor EnemyTint(0.92f, 0.14f, 0.10f, 1.0f);
	if (UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this))
	{
		if (Campaign->HasActiveContract())
		{
			EnemyTint = ERGalacticMeta::GetFactionColor(Campaign->GetActiveContract().EnemyFaction);
		}
	}
	EnemySkeletalTint = EnemyTint;
	if (UMaterialInterface* BaseE = EnemyMaterial.IsNull() ? BasicMat : EnemyMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseE, this);
		MID->SetVectorParameterValue(TEXT("Color"), EnemyTint);
		CachedEnemyMat = MID;
	}

	CachedAllyBody = AllyBodyMesh.IsNull() ? nullptr : AllyBodyMesh.LoadSynchronous();
	CachedEnemyBody = EnemyBodyMesh.IsNull() ? nullptr : EnemyBodyMesh.LoadSynchronous();
	CachedGearCube = Catalog ? Catalog->LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube")) : nullptr;

	CachedAllySkel = AllySkeletalMesh.IsNull() ? nullptr : AllySkeletalMesh.LoadSynchronous();
	CachedAnimIdle = AllyAnimIdle.IsNull() ? nullptr : AllyAnimIdle.LoadSynchronous();
	CachedAnimRelaxedIdle = AllyAnimRelaxedIdle.IsNull() ? nullptr : AllyAnimRelaxedIdle.LoadSynchronous();
	CachedAnimMove = AllyAnimMove.IsNull() ? nullptr : AllyAnimMove.LoadSynchronous();
	CachedRelaxedBS = AllyRelaxedBlendSpace.IsNull() ? nullptr : AllyRelaxedBlendSpace.LoadSynchronous();
	CachedCombatBS = AllyCombatBlendSpace.IsNull() ? nullptr : AllyCombatBlendSpace.LoadSynchronous();
	CachedAnimAttacks.Reset();
	for (const TSoftObjectPtr<UAnimationAsset>& P : AllyAnimAttacks)
	{
		if (UAnimationAsset* A = P.LoadSynchronous()) { CachedAnimAttacks.Add(A); }
	}
	CachedAnimDeaths.Reset();
	for (const TSoftObjectPtr<UAnimationAsset>& P : AllyAnimDeaths)
	{
		if (UAnimationAsset* A = P.LoadSynchronous()) { CachedAnimDeaths.Add(A); }
	}
}

void ALegionGameMode::SelectSpawnZone(const FBox& TerrainBounds, int32 Seed, bool bFilterBySituation,
	ELegionTacticalProfile TacticalProfile, float ConflictIntensity, int32 DangerRating)
{
	DeployCenter = FVector::ZeroVector;
	EffectiveBattlefieldHalfExtent = 0.0f;
	BattleAxisYawDeg = InitialYawDeg;
	ActiveZoneIndex = INDEX_NONE;
	ActiveZoneName = TEXT("中央战区");

	// 先算整张地形的默认中心兜底 (适配无候选区的小型演武平台)
	if (TerrainBounds.IsValid)
	{
		const FVector C = TerrainBounds.GetCenter();
		DeployCenter = FVector(C.X, C.Y, 0.0f);
		const FVector Ex = TerrainBounds.GetExtent();
		EffectiveBattlefieldHalfExtent = FMath::Max(2500.0f, FMath::Min(Ex.X, Ex.Y) - 2500.0f);
	}

	if (!bUseRandomSpawnZone || CandidateZones.Num() == 0 || !TerrainBounds.IsValid) { return; }

	// 仅保留完整落在当前 Landscape 内部 (保留 >=20m 外围缓冲) 的候选战区
	constexpr float EdgeMargin = 2000.0f;
	TArray<int32> ValidIndices;
	for (int32 i = 0; i < CandidateZones.Num(); ++i)
	{
		const FLegionSpawnZone& Z = CandidateZones[i];
		const float R = Z.HalfExtentCm + EdgeMargin;
		if (Z.CenterXY.X - R >= TerrainBounds.Min.X && Z.CenterXY.X + R <= TerrainBounds.Max.X &&
			Z.CenterXY.Y - R >= TerrainBounds.Min.Y && Z.CenterXY.Y + R <= TerrainBounds.Max.Y)
		{
			ValidIndices.Add(i);
		}
	}
	if (ValidIndices.Num() == 0) { return; }

	int32 ForcedZone = INDEX_NONE;
	int32 ForcedProfile = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("-LegionProfile="), ForcedProfile) &&
		ForcedProfile >= 0 && ForcedProfile <= (int32)ELegionTacticalProfile::OpenManeuver)
	{
		bFilterBySituation = true;
		TacticalProfile = static_cast<ELegionTacticalProfile>(ForcedProfile);
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("-LegionZone="), ForcedZone) &&
		ForcedZone >= 0 && ForcedZone < ValidIndices.Num())
	{
		ActiveZoneIndex = ValidIndices[ForcedZone];
	}
	else if (bFilterBySituation && ValidIndices.Num() > 1)
	{
		// 条件抽选: 按委托战术倾向 + 星区冲突烈度 + 危险星级对候选区打分, 取 Top-3 池再用种子抽取
		const float RuggedWeight = FMath::Clamp(ConflictIntensity * 0.6f + (DangerRating - 1) * 0.12f, 0.0f, 1.0f);
		auto ScoreZone = [&](const FLegionSpawnZone& Z) -> float
		{
			float Score = 0.0f;
			switch (TacticalProfile)
			{
			case ELegionTacticalProfile::HoldHighGround:
				Score += (Z.PlayerHighGroundDeltaCm >= 200.0f)
					? (40.0f + FMath::Clamp(Z.PlayerHighGroundDeltaCm, 0.0f, 2500.0f) * 0.015f)
					: (Z.PlayerHighGroundDeltaCm <= -800.0f ? -40.0f : 0.0f);
				break;
			case ELegionTacticalProfile::AssaultHighGround:
				Score += (Z.PlayerHighGroundDeltaCm <= -800.0f)
					? (40.0f + FMath::Clamp(-Z.PlayerHighGroundDeltaCm, 0.0f, 3200.0f) * 0.012f)
					: (Z.PlayerHighGroundDeltaCm >= 800.0f ? -30.0f : 0.0f);
				break;
			case ELegionTacticalProfile::ValleyChokepoint:
				Score += (Z.ReliefSpanCm - 8000.0f) * 0.008f + (Z.WalkableRatio <= 0.84f ? 26.0f : 0.0f);
				break;
			case ELegionTacticalProfile::OpenManeuver:
				Score += (Z.WalkableRatio - 0.72f) * 260.0f - FMath::Max(0.0f, Z.ReliefSpanCm - 11000.0f) * 0.005f;
				break;
			case ELegionTacticalProfile::Balanced:
			default:
				Score += (FMath::Abs(Z.PlayerHighGroundDeltaCm) < 1500.0f ? 22.0f : 0.0f)
					+ (Z.WalkableRatio - 0.72f) * 80.0f;
				break;
			}
			Score += RuggedWeight * (Z.ReliefSpanCm - 9500.0f) * 0.005f;
			return Score;
		};

		TArray<TPair<float, int32>> Ranked;
		Ranked.Reserve(ValidIndices.Num());
		for (int32 Idx : ValidIndices)
		{
			Ranked.Emplace(ScoreZone(CandidateZones[Idx]), Idx);
		}
		Ranked.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
		{
			return A.Key > B.Key;
		});

		const int32 PoolSize = FMath::Min(3, Ranked.Num());
		FRandomStream Rng(Seed);
		const int32 PickRank = Rng.RandRange(0, PoolSize - 1);
		ActiveZoneIndex = Ranked[PickRank].Value;
	}
	else
	{
		FRandomStream Rng(Seed);
		ActiveZoneIndex = ValidIndices[Rng.RandRange(0, ValidIndices.Num() - 1)];
	}

	const FLegionSpawnZone& Zone = CandidateZones[ActiveZoneIndex];
	DeployCenter = FVector(Zone.CenterXY.X, Zone.CenterXY.Y, 0.0f);
	EffectiveBattlefieldHalfExtent = Zone.HalfExtentCm;
	BattleAxisYawDeg = Zone.BattleAxisYawDeg;
	ActiveZoneName = Zone.DisplayName.IsEmpty()
		? FString::Printf(TEXT("战区 %d"), ActiveZoneIndex + 1)
		: Zone.DisplayName;

	UE_LOG(LogERLegion, Log,
		TEXT("[Legion] Selected spawn zone [%d/%d] '%s' Center=(%.0f, %.0f) HalfExt=%.0f Yaw=%.0f (Seed=%d, Profile=%s, Conflict=%.2f, Danger=%d, Walk=%.1f%%, SpanZ=%.0fcm, DeltaZ=%+.0fcm)"),
		ActiveZoneIndex, ValidIndices.Num(), *ActiveZoneName,
		DeployCenter.X, DeployCenter.Y, EffectiveBattlefieldHalfExtent, BattleAxisYawDeg,
		Seed, *ERGalacticMeta::GetTacticalProfileName(TacticalProfile), ConflictIntensity, DangerRating,
		Zone.WalkableRatio * 100.0f, Zone.ReliefSpanCm, Zone.PlayerHighGroundDeltaCm);
}

void ALegionGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World) { return; }

	TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_Deploy);

	LoadRuntimeAssets();

	UGalacticCampaignSubsystem* Campaign = UGalacticCampaignSubsystem::Get(this);
	const bool bUseContract = Campaign && Campaign->HasActiveContract();
	BattleContractId = bUseContract ? Campaign->GetActiveContract().ContractId : INDEX_NONE;

	// 决定本场战区随机种子: -LegionSeed=N > 合同 TerrainSeed > 运行时真随机
	int32 MatchSeed = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-LegionSeed="), MatchSeed))
	{
		if (bUseContract)
		{
			MatchSeed = Campaign->GetActiveContract().TerrainSeed;
		}
		else
		{
			MatchSeed = (int32)((FPlatformTime::Cycles64() ^ (uint64)FMath::Rand()) & 0x7FFFFFFF);
			if (MatchSeed == 0) { MatchSeed = 1337; }
		}
	}
	ActiveMatchSeed = MatchSeed;

	int32 SectorId = 0;
	float ConflictIntensity = 0.5f;
	int32 DangerRating = 2;
	ELegionTacticalProfile TacticalProfile = ELegionTacticalProfile::Balanced;
	if (bUseContract)
	{
		const FGalacticContract& Contract = Campaign->GetActiveContract();
		SectorId = Contract.TargetSectorId;
		DangerRating = Contract.DangerRating;
		TacticalProfile = Contract.TacticalProfile;
		if (const FGalacticSector* Sec = Campaign->FindSector(SectorId))
		{
			ConflictIntensity = Sec->ConflictIntensity;
		}
	}

	// 1. 探测地形包围盒, 按银河局势与委托倾向筛选局部战区, 并应用动态气象
	FBox TerrainBounds(ForceInit);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass() && It->GetClass()->GetName().Contains(TEXT("Landscape")))
		{
			FVector O, E;
			It->GetActorBounds(false, O, E);
			TerrainBounds += FBox::BuildAABB(O, E);
		}
	}
	SelectSpawnZone(TerrainBounds, ActiveMatchSeed, bUseContract, TacticalProfile, ConflictIntensity, DangerRating);

	ActiveWeatherPreset = FLegionWeatherSystem::SelectPreset(ActiveMatchSeed, SectorId, ConflictIntensity, DangerRating);
	ActiveWeatherName = FLegionWeatherSystem::GetPresetName(ActiveWeatherPreset);
	FLegionWeatherSystem::Apply(World, ActiveWeatherPreset, BattleAxisYawDeg, ActiveMatchSeed);

	FVector BattleForward, BattleRight;
	GetBattleBasis(BattleForward, BattleRight);
	const float PlayerYaw = BattleAxisYawDeg;
	const float EnemyYaw = FRotator::NormalizeAxis(BattleAxisYawDeg + 180.0f);

	const float HalfExt = GetBattlefieldHalfExtent();
	TerrainGrid.Build(World, DeployCenter, HalfExt, 250.0f, 38.0f);
	float CenterZ = 0.0f, CenterSlope = 0.0f;
	if (TerrainGrid.SampleHeightAndSlope(DeployCenter, CenterZ, CenterSlope))
	{
		DeployCenter.Z = CenterZ;
	}

	// 林地: 必须在地图纹理烘焙与兵团布阵之前 (密林会改写可通行格并体现在战略图上)
	SpawnForest();

	BuildDeploymentMapTextures();
	DeployInitialForces(bUseContract ? Campaign : nullptr, BattleForward, BattleRight, PlayerYaw, EnemyYaw);
	PositionDeploymentCommander(*World, BattleForward);

	SetTacticalTimeMode(ELegionTimeMode::Normal);

	// 自动化截图链路与显式跳过参数不走接驳屏, 战斗立即打响;
	// 正常玩家流程由 ALegionHUD 挂载 ULegionDeployScreen, 点击「进入战场」后调 BeginBattle
	const bool bSkipDeployScreen = bAutoShotRequested || bSkyShotRequested || bMapShotRequested || bCloseupShotRequested || bAIShotRequested || bResultShotRequested || bPerfTestRequested
		|| FParse::Param(FCommandLine::Get(), TEXT("NoDeployScreen"));
	bBattleStarted = bSkipDeployScreen;

	// -LegionAIShot 开局即分配弱 AI (方便立即测试巡逻/戒备/侦察), 其他链路延迟 1.8s 启动以兼容开局静止回归截图
	bInitialWeakAIAssigned = false;
	InitialWeakAIDelay = bAIShotRequested ? 0.4f : 1.8f;

	UE_LOG(LogERLegion, Log, TEXT("[Legion] Deployed %d ally legions vs %d enemy legions (+%d pending waves), animated=%d, deployscreen=%d"),
		PlayerLegions.Num(), EnemyLegions.Num(), PendingWaves.Num(), (int32)bUseAnimatedAlly, (int32)!bBattleStarted);
}

void ALegionGameMode::BuildDeploymentMapTextures()
{
	// 生成黑白等高线地图纹理: 256px 角标小地图 + 1536px 全屏战略图 (后者覆盖 ~2K 视口不糊)
	if (TerrainGrid.IsBuilt())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ER_Legion_MapBake);
		auto BakeMapTexture = [&](int32 MapPx) -> UTexture2D*
		{
			TArray<FColor> Pixels;
			TerrainGrid.BuildMinimapPixels(MapPx, Pixels);
			if (Pixels.Num() != MapPx * MapPx) { return nullptr; }
			UTexture2D* Tex = UTexture2D::CreateTransient(MapPx, MapPx, PF_B8G8R8A8);
			if (!Tex) { return nullptr; }
			Tex->SRGB = true;
			Tex->Filter = TF_Bilinear;
			Tex->AddressX = TA_Clamp;
			Tex->AddressY = TA_Clamp;
			if (FTexturePlatformData* PD = Tex->GetPlatformData())
			{
				if (PD->Mips.Num() > 0)
				{
					void* Data = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
					FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
					PD->Mips[0].BulkData.Unlock();
					Tex->UpdateResource();
				}
			}
			return Tex;
		};

		MinimapTexture = BakeMapTexture(256);
		StrategicMapTexture = BakeMapTexture(1536);
		// 等高线地图视角放大后换用的高精度图 (BuildMinimapPixels 内部多线程烘焙)
		DetailMapTexture = BakeMapTexture(3072);
	}
}

void ALegionGameMode::DeployInitialForces(const UGalacticCampaignSubsystem* Campaign, const FVector& BattleForward, const FVector& BattleRight, float PlayerYaw, float EnemyYaw)
{
	// 2. 从战役子系统读取委托编成, 或走独立演武沙盘预设
	PlayerLegions.Reset();
	EnemyLegions.Reset();
	PendingWaves.Reset();
	InitialPlayerCounts.Reset();
	InitialEnemySoldiersTotal = 0;

	if (Campaign)
	{
		const FGalacticContract& Contract = Campaign->GetActiveContract();
		BattleTitleText = Contract.Title;
		BattleSubText = FString::Printf(TEXT("委托  ·  %s 对 %s  ·  第 %02d 星区 · %s · %s"),
			*ERGalacticMeta::GetFactionCode(Contract.Employer),
			*ERGalacticMeta::GetFactionCode(Contract.EnemyFaction),
			Contract.TargetSectorId + 1,
			*ActiveZoneName,
			*ActiveWeatherName);

		// 计算我方存活兵团总数决定是否开启骨骼动画模式
		TArray<FManagedLegionData> AlivePlayerRoster;
		int32 TotalAllySoldiers = 0;
		for (const FManagedLegionData& M : Campaign->GetPlayerLegions())
		{
			if (M.AliveSoldiers > 0)
			{
				AlivePlayerRoster.Add(M);
				TotalAllySoldiers += M.AliveSoldiers;
			}
		}
		bUseAnimatedAlly = (CachedAllySkel != nullptr) && (bForceSkeletalSoldiers || TotalAllySoldiers <= MaxAnimatedSoldiersPerSide);

		const int32 NP = AlivePlayerRoster.Num();
		const float X0 = -0.5f * FMath::Max(0, NP - 1) * LegionSpacing;
		for (int32 i = 0; i < NP; ++i)
		{
			const FManagedLegionData& M = AlivePlayerRoster[i];
			const FVector RawPos = DeployCenter
				- BattleForward * (LineDistance * 0.5f)
				+ BattleRight * (X0 + i * LegionSpacing);
			const FVector SafePos = TerrainGrid.IsBuilt() ? TerrainGrid.ClampToWalkable(RawPos) : RawPos;
			if (ALegionUnit* L = SpawnLegion(ELegionTeam::Player, M.Archetype, M.AliveSoldiers,
				SafePos, PlayerYaw, M.LegionId, M.Callsign, M.SyncLevel))
			{
				PlayerLegions.Add(L);
				InitialPlayerCounts.Add(M.LegionId, M.AliveSoldiers);
			}
		}

		// 敌军波次: Delay<=0 立即布阵, Delay>0 存入 PendingWaves 倒计时增援
		int32 TotalEnemyContractSoldiers = 0;
		for (const FEnemyWaveLegionSpec& W : Contract.EnemyWaves)
		{
			TotalEnemyContractSoldiers += W.SoldierCount;
		}
		bUseAnimatedEnemy = (CachedAllySkel != nullptr) && (bForceSkeletalSoldiers || TotalEnemyContractSoldiers <= MaxAnimatedSoldiersPerSide);

		const float FrontSpan = FMath::Max(4500.0f, (float)NP * LegionSpacing * 0.8f);
		for (int32 w = 0; w < Contract.EnemyWaves.Num(); ++w)
		{
			const FEnemyWaveLegionSpec& W = Contract.EnemyWaves[w];
			InitialEnemySoldiersTotal += W.SoldierCount;

			const float LateralOffset = W.LaneBias * FrontSpan * 0.5f;
			const float DepthOffset = LineDistance * (W.DelaySeconds > 0.0f ? 0.72f : 0.5f);
			const FVector RawDrop = DeployCenter + BattleForward * DepthOffset + BattleRight * LateralOffset;
			const FVector DropPos = TerrainGrid.IsBuilt()
				? TerrainGrid.ClampToWalkable(RawDrop)
				: RawDrop;

			if (W.DelaySeconds <= 0.01f)
			{
				const FString Label = ERGalacticMeta::GetEnemyBrigadeName(EnemyLegions.Num() + 1, true, Contract.EnemyFaction);
				if (ALegionUnit* L = SpawnLegion(ELegionTeam::Enemy, W.Archetype, W.SoldierCount,
					DropPos, EnemyYaw, INDEX_NONE, Label, 1))
				{
					EnemyLegions.Add(L);
				}
			}
			else
			{
				FPendingEnemyWave PW;
				PW.Spec = W;
				PW.DropLocation = DropPos;
				PendingWaves.Add(PW);
			}
		}
	}
	else
	{
		// 独立沙盘演武模式: 四兵种齐上阵 (4v4 默认, 支持命令行 -LegionLegions / -LegionSoldiers 压测)
		BattleTitleText = TEXT("虹膜军团演武");
		BattleSubText = FString::Printf(TEXT("虹膜基金会  ·  %s  ·  %s"), *ActiveZoneName, *ActiveWeatherName);

		const int32 N = FMath::Max(1, LegionsPerSide);
		bUseAnimatedAlly = (CachedAllySkel != nullptr) && (bForceSkeletalSoldiers || N * SoldiersPerLegion <= MaxAnimatedSoldiersPerSide);
		bUseAnimatedEnemy = bUseAnimatedAlly;
		static const ELegionClass Classes[4] = {
			ELegionClass::Shieldbearer, ELegionClass::Pikeman, ELegionClass::Vanguard, ELegionClass::Marksman
		};
		const float X0 = -0.5f * (N - 1) * LegionSpacing;
		for (int32 i = 0; i < N; ++i)
		{
			const ELegionClass C = Classes[i % 4];
			const float Lat = X0 + i * LegionSpacing;

			const FVector RawP = DeployCenter - BattleForward * (LineDistance * 0.5f) + BattleRight * Lat;
			const FVector PPos = TerrainGrid.IsBuilt() ? TerrainGrid.ClampToWalkable(RawP) : RawP;
			if (ALegionUnit* L = SpawnLegion(ELegionTeam::Player, C, SoldiersPerLegion,
				PPos, PlayerYaw, i + 1, ERGalacticMeta::GetBrigadeName(i + 1), 1))
			{
				PlayerLegions.Add(L);
				InitialPlayerCounts.Add(i + 1, SoldiersPerLegion);
			}

			const FVector RawE = DeployCenter + BattleForward * (LineDistance * 0.5f) + BattleRight * Lat;
			const FVector EPos = TerrainGrid.IsBuilt() ? TerrainGrid.ClampToWalkable(RawE) : RawE;
			if (ALegionUnit* L = SpawnLegion(ELegionTeam::Enemy, C, SoldiersPerLegion,
				EPos, EnemyYaw, INDEX_NONE, ERGalacticMeta::GetEnemyBrigadeName(i + 1), 1))
			{
				EnemyLegions.Add(L);
				InitialEnemySoldiersTotal += SoldiersPerLegion;
			}
		}
	}
}

void ALegionGameMode::PositionDeploymentCommander(UWorld& World, const FVector& BattleForward)
{
	// 3. 第一人称指挥官: 站到我方战线身后, 面朝敌方
	if (APlayerController* PC = World.GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			FVector Start = DeployCenter - BattleForward * (LineDistance * 0.5f + 2200.0f);
			if (PlayerLegions.Num() > 0)
			{
				FVector SumPos = FVector::ZeroVector;
				int32 N = 0;
				for (const ALegionUnit* L : PlayerLegions)
				{
					if (!L) { continue; }
					SumPos += L->GetActorLocation();
					++N;
				}
				if (N > 0)
				{
					const FVector MeanPos = SumPos / (float)N;
					Start = MeanPos - BattleForward * 2200.0f;
				}
			}
			if (TerrainGrid.IsBuilt()) { Start = TerrainGrid.ClampToWalkable(Start); }
			float GroundZ = Start.Z, Slope = 0.0f;
			TerrainGrid.SampleHeightAndSlope(Start, GroundZ, Slope);
			Pawn->SetActorLocation(FVector(Start.X, Start.Y, GroundZ + 120.0f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(-4.0f, BattleAxisYawDeg, 0.0f));
		}
	}
}

void ALegionGameMode::SpawnForest()
{
	if (!bSpawnForest || !TerrainGrid.IsBuilt() || FParse::Param(FCommandLine::Get(), TEXT("NoForest"))) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }

	int32 Seed = (ActiveMatchSeed != 0) ? ActiveMatchSeed : DefaultForestSeed;
	FParse::Value(FCommandLine::Get(), TEXT("-ForestSeed="), Seed);

	FVector BattleForward, BattleRight;
	GetBattleBasis(BattleForward, BattleRight);
	const FVector2D Center2D(DeployCenter.X, DeployCenter.Y);
	const FVector2D Fwd2D(BattleForward.X, BattleForward.Y);

	// 部署带: 我方线 / 敌方线 / 延迟空降线 (沿战线轴向旋转对齐), 两侧各清空 ForestClearHalfDepth
	TArray<FLegionOrientedBand> Exclusions;
	for (const float Frac : { -0.5f, 0.5f, 0.72f })
	{
		FLegionOrientedBand Band;
		Band.Center = Center2D + Fwd2D * (LineDistance * Frac);
		Band.Forward = Fwd2D;
		Band.HalfDepth = ForestClearHalfDepth;
		Exclusions.Add(Band);
	}

	if (!Forest)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Forest = World->SpawnActor<ALegionForest>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	}
	if (!Forest) { return; }

	// 连通性: 三条纵向车道 (我方线 -> 敌方线) 中, 无树时本就连通的, 种树后也必须连通
	struct FLane { FVector From, To; bool bBaseOk = false; };
	TArray<FLane> Lanes;
	for (const float DX : { -9000.0f, 0.0f, 9000.0f })
	{
		FLane L;
		L.From = TerrainGrid.ClampToWalkable(DeployCenter - BattleForward * (LineDistance * 0.5f) + BattleRight * DX);
		L.To   = TerrainGrid.ClampToWalkable(DeployCenter + BattleForward * (LineDistance * 0.5f) + BattleRight * DX);
		Lanes.Add(L);
	}
	TerrainGrid.ResetTrees();
	for (FLane& L : Lanes)
	{
		TArray<FVector> Tmp;
		L.bBaseOk = TerrainGrid.FindPath(L.From, L.To, Tmp);
	}

	bool bOk = false;
	for (const float Scale : { 1.0f, 0.7f, 0.45f })
	{
		Forest->Generate(World, TerrainGrid, Seed, Scale, Exclusions);
		bOk = true;
		for (const FLane& L : Lanes)
		{
			TArray<FVector> Tmp;
			if (L.bBaseOk && !TerrainGrid.FindPath(L.From, L.To, Tmp)) { bOk = false; break; }
		}
		if (bOk) { break; }
		UE_LOG(LogERLegion, Warning, TEXT("[Forest] seed=%d scale=%.2f blocks a lane, retry thinner"), Seed, Scale);
	}
	if (!bOk)
	{
		Forest->ClearTrees();
		TerrainGrid.ResetTrees();
		UE_LOG(LogERLegion, Warning, TEXT("[Forest] seed=%d could not keep lanes open, forest disabled"), Seed);
	}
}

ALegionUnit* ALegionGameMode::SpawnLegion(ELegionTeam Team, ELegionClass Archetype, int32 SoldierCount,
	const FVector& Location, float YawDeg, int32 CampaignId, const FString& Callsign, int32 SyncLevel)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 兵团 Actor 本身保持无旋转, 仅把初始朝向传递给各个士兵自身转动
	ALegionUnit* L = GetWorld()->SpawnActor<ALegionUnit>(LegionClass, Location, FRotator::ZeroRotator, P);
	if (L)
	{
		L->SetTerrainGrid(&TerrainGrid);
		L->SetAnimationSuites(CachedRelaxedBS, CachedCombatBS, CachedAnimRelaxedIdle);
		const int32 SeedOffset = (Team == ELegionTeam::Player ? 101 : 307) * (PlayerLegions.Num() + EnemyLegions.Num() + 1) + (int32)Archetype * 17;
		L->SetDeterministicSeed(ActiveMatchSeed ^ SeedOffset);
		if (Team == ELegionTeam::Player)
		{
			L->InitLegion(Team, Archetype, SoldierCount,
				CachedAllyBody, nullptr, bUseAnimatedAlly ? nullptr : CachedAllyMat,
				bUseAnimatedAlly ? CachedAllySkel : nullptr,
				CachedAnimIdle, CachedAnimMove, CachedAnimAttacks, CachedAnimDeaths,
				CampaignId, Callsign, SyncLevel, YawDeg,
				FLinearColor(0.84f, 0.90f, 0.98f, 1.0f));
		}
		else
		{
			// 敌我统一采用 Manny 骨骼模型与动作, 通过 "Paint Tint" 染为敌方阵营色区分
			L->InitLegion(Team, Archetype, SoldierCount,
				CachedEnemyBody, nullptr, bUseAnimatedEnemy ? nullptr : CachedEnemyMat,
				bUseAnimatedEnemy ? CachedAllySkel : nullptr,
				CachedAnimIdle, CachedAnimMove, CachedAnimAttacks, CachedAnimDeaths,
				CampaignId, Callsign, SyncLevel, YawDeg,
				EnemySkeletalTint);
		}
	}
	return L;
}
