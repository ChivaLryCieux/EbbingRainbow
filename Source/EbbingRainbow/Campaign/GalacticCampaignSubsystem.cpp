// Copyright Epic Games, Inc. All Rights Reserved.

#include "Campaign/GalacticCampaignSubsystem.h"

#include "Core/ERLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UGalacticCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetCampaign(2026);
}

UGalacticCampaignSubsystem* UGalacticCampaignSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine) { return nullptr; }
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World) { return nullptr; }
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UGalacticCampaignSubsystem>() : nullptr;
}

void UGalacticCampaignSubsystem::ResetCampaign(int32 Seed)
{
	CampaignSeed = Seed;
	Credits = 1200;
	CycleIndex = 1;
	NextLegionId = 1;
	NextContractId = 1;
	bHasActiveContract = false;
	LastOutcome = FLegionBattleOutcome();

	FRandomStream Rng(CampaignSeed);
	InitFactionsAndSectors(Rng);
	InitStarterLegions();
	RefreshContracts();

	UE_LOG(LogERLegion, Log, TEXT("[Campaign] Initialized Iris Foundation campaign (Seed=%d, Credits=%d, Legions=%d)"),
		CampaignSeed, Credits, PlayerLegions.Num());
}

void UGalacticCampaignSubsystem::InitFactionsAndSectors(FRandomStream& Rng)
{
	Factions.Reset();

	{
		FFactionStatus S;
		S.Faction = EFactionId::Empire;
		S.DisplayName = ERGalacticMeta::GetFactionName(S.Faction);
		S.CodeName = ERGalacticMeta::GetFactionCode(S.Faction);
		S.DoctrineBrief = TEXT("核心星域正规军。军费雄厚，偏重重盾防线清剿与阵地推进。");
		S.Color = ERGalacticMeta::GetFactionColor(S.Faction);
		S.Reputation = 10;
		S.GalacticControl = 44.0f;
		Factions.Add(S);
	}
	{
		FFactionStatus S;
		S.Faction = EFactionId::Dominion;
		S.DisplayName = ERGalacticMeta::GetFactionName(S.Faction);
		S.CodeName = ERGalacticMeta::GetFactionCode(S.Faction);
		S.DoctrineBrief = TEXT("星环矿业与自治邦联。擅长高地阻击与长戟守备，信誉稳定。");
		S.Color = ERGalacticMeta::GetFactionColor(S.Faction);
		S.Reputation = 5;
		S.GalacticControl = 32.0f;
		Factions.Add(S);
	}
	{
		FFactionStatus S;
		S.Faction = EFactionId::Rebels;
		S.DisplayName = ERGalacticMeta::GetFactionName(S.Faction);
		S.CodeName = ERGalacticMeta::GetFactionCode(S.Faction);
		S.DoctrineBrief = TEXT("外环反抗同盟。多采用游击与空降包抄，危险高但控制权跃迁猛烈。");
		S.Color = ERGalacticMeta::GetFactionColor(S.Faction);
		S.Reputation = 0;
		S.GalacticControl = 24.0f;
		Factions.Add(S);
	}

	Sectors.Reset();
	struct FSectorSeed { const TCHAR* Name; const TCHAR* Code; FVector2D Pos; EFactionId Owner; float Conflict; };
	static const FSectorSeed Presets[] = {
		{ TEXT("天枢工业环"),   TEXT("SEC-01 // DUBHE"),    FVector2D(0.18f, 0.34f), EFactionId::Empire,   0.45f },
		{ TEXT("白矮星矿带"),   TEXT("SEC-02 // SIRIUS-B"), FVector2D(0.36f, 0.72f), EFactionId::Dominion, 0.62f },
		{ TEXT("苍白裂隙"),     TEXT("SEC-03 // PALE RIFT"),FVector2D(0.50f, 0.28f), EFactionId::Empire,   0.78f },
		{ TEXT("赫尔墨斯前哨"), TEXT("SEC-04 // HERMES"),   FVector2D(0.62f, 0.60f), EFactionId::Rebels,   0.85f },
		{ TEXT("霜桥中继站"),   TEXT("SEC-05 // BIFROST"),  FVector2D(0.78f, 0.36f), EFactionId::Dominion, 0.50f },
		{ TEXT("新伊甸边缘"),   TEXT("SEC-06 // EDEN RIM"), FVector2D(0.84f, 0.74f), EFactionId::Rebels,   0.68f },
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Presets); ++i)
	{
		FGalacticSector Sec;
		Sec.SectorId = i;
		Sec.Name = Presets[i].Name;
		Sec.Code = Presets[i].Code;
		Sec.MapPos = Presets[i].Pos;
		Sec.ControllingFaction = Presets[i].Owner;
		Sec.ConflictIntensity = Presets[i].Conflict;
		Sectors.Add(Sec);
	}
}

void UGalacticCampaignSubsystem::InitStarterLegions()
{
	PlayerLegions.Reset();

	auto AddStarter = [&](const TCHAR* Callsign, ELegionClass Class, int32 Count)
	{
		FManagedLegionData L;
		L.LegionId = NextLegionId++;
		L.Callsign = Callsign;
		L.Archetype = Class;
		L.MaxSoldiers = Count;
		L.AliveSoldiers = Count;
		L.SyncLevel = 1;
		PlayerLegions.Add(L);
	};

	AddStarter(TEXT("虹膜一号 // 铁砧盾卫"), ELegionClass::Shieldbearer, 20);
	AddStarter(TEXT("虹膜二号 // 折戟矛阵"), ELegionClass::Pikeman,      20);
	AddStarter(TEXT("虹膜三号 // 绯红刃锋"), ELegionClass::Vanguard,     18);
	AddStarter(TEXT("虹膜四号 // 远瞳齐射"), ELegionClass::Marksman,     16);
}

const FFactionStatus* UGalacticCampaignSubsystem::FindFaction(EFactionId Faction) const
{
	for (const FFactionStatus& F : Factions)
	{
		if (F.Faction == Faction) { return &F; }
	}
	return nullptr;
}

const FGalacticSector* UGalacticCampaignSubsystem::FindSector(int32 SectorId) const
{
	for (const FGalacticSector& S : Sectors)
	{
		if (S.SectorId == SectorId) { return &S; }
	}
	return nullptr;
}

FManagedLegionData* UGalacticCampaignSubsystem::FindPlayerLegionMutable(int32 LegionId)
{
	for (FManagedLegionData& L : PlayerLegions)
	{
		if (L.LegionId == LegionId) { return &L; }
	}
	return nullptr;
}

const FGalacticContract* UGalacticCampaignSubsystem::FindContract(int32 ContractId) const
{
	for (const FGalacticContract& C : AvailableContracts)
	{
		if (C.ContractId == ContractId) { return &C; }
	}
	return nullptr;
}

void UGalacticCampaignSubsystem::RefreshContracts()
{
	AvailableContracts.Reset();
	FRandomStream Rng(CampaignSeed + CycleIndex * 7919);

	struct FContractTemplate
	{
		EFactionId Employer;
		EFactionId Enemy;
		const TCHAR* Title;
		const TCHAR* Briefing;
		int32 Danger;
	};

	static const FContractTemplate Templates[] = {
		{
			EFactionId::Empire, EFactionId::Rebels,
			TEXT("「铁幕肃清」边境高地防线清剿"),
			TEXT("自由叛军占据了裂隙制高点并部署多支伏击群。帝国军部雇佣虹膜基金会接管前线战术神经，彻底粉碎叛军攻势。"),
			2
		},
		{
			EFactionId::Rebels, EFactionId::Empire,
			TEXT("「断虹行动」帝国重装纵队伏击"),
			TEXT("帝国第7重盾先遣队正穿越山谷补给线。叛军同盟委托基金会指挥突击集群切入侧背，阻止其完成合围。"),
			3
		},
		{
			EFactionId::Dominion, EFactionId::Rebels,
			TEXT("「晶脉守望」矿区高台阻击战"),
			TEXT("外环叛军空降突击队企图夺取自治领白矮星精炼枢纽。守住高地并击溃来袭增援波次。"),
			2
		},
		{
			EFactionId::Empire, EFactionId::Dominion,
			TEXT("「轨道征税」自治领防区强行突破"),
			TEXT("自治领拒绝履行星环税赋条约。帝国授权基金会以高强度多波次攻势压垮其长戟与远程联防线。"),
			4
		},
		{
			EFactionId::Dominion, EFactionId::Empire,
			TEXT("「自由航道」中继站反包围作战"),
			TEXT("帝国舰队封锁了霜桥中继站外围平台。自治领愿以高额科研酬金聘请基金会远程调度兵团突围。"),
			3
		},
		{
			EFactionId::Rebels, EFactionId::Dominion,
			TEXT("「星火夺取」自治领前哨突袭"),
			TEXT("叛军急需自治领前哨的神经电容阵列。利用地形遮蔽接近其远程阵地并全歼守备兵团。"),
			1
		}
	};

	// 每个周期产出 3 份覆盖三大阵营的委托
	const EFactionId Employers[3] = { EFactionId::Empire, EFactionId::Dominion, EFactionId::Rebels };
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		TArray<int32> Matching;
		for (int32 i = 0; i < UE_ARRAY_COUNT(Templates); ++i)
		{
			if (Templates[i].Employer == Employers[Slot]) { Matching.Add(i); }
		}
		const int32 PickIdx = Matching[Rng.RandRange(0, Matching.Num() - 1)];
		const FContractTemplate& Tpl = Templates[PickIdx];

		FGalacticContract C;
		C.ContractId = NextContractId++;
		C.Employer = Tpl.Employer;
		C.EnemyFaction = Tpl.Enemy;
		C.Title = Tpl.Title;
		C.Briefing = Tpl.Briefing;
		C.TargetSectorId = (Slot * 2 + Rng.RandRange(0, 1)) % FMath::Max(1, Sectors.Num());
		C.TerrainSeed = CampaignSeed + CycleIndex * 97 + Slot * 313;
		C.DangerRating = FMath::Clamp(Tpl.Danger + (CycleIndex > 3 ? 1 : 0), 1, 5);
		C.RewardCredits = 360 + C.DangerRating * 190 + Rng.RandRange(0, 80);
		C.ControlShift = 4.5f + C.DangerRating * 2.0f;
		C.ReputationGain = 10 + C.DangerRating * 4;

		// 按危险等级与敌方阵营偏重生成多兵种敌军编成 (含后续空降增援波次)
		const int32 BaseCount = 8 + C.DangerRating * 3;
		auto AddWave = [&](ELegionClass Class, int32 Count, float Delay, float Lane)
		{
			FEnemyWaveLegionSpec W;
			W.Archetype = Class;
			W.SoldierCount = Count;
			W.DelaySeconds = Delay;
			W.LaneBias = Lane;
			C.EnemyWaves.Add(W);
		};

		if (C.EnemyFaction == EFactionId::Empire)
		{
			AddWave(ELegionClass::Shieldbearer, BaseCount + 4, 0.0f, -0.55f);
			AddWave(ELegionClass::Shieldbearer, BaseCount + 4, 0.0f,  0.55f);
			AddWave(ELegionClass::Marksman,     BaseCount,     0.0f,  0.0f);
			AddWave(ELegionClass::Vanguard,     BaseCount,     18.0f, -0.8f);
			if (C.DangerRating >= 3)
			{
				AddWave(ELegionClass::Pikeman,  BaseCount + 2, 24.0f,  0.8f);
			}
		}
		else if (C.EnemyFaction == EFactionId::Dominion)
		{
			AddWave(ELegionClass::Pikeman,      BaseCount + 2, 0.0f, -0.45f);
			AddWave(ELegionClass::Pikeman,      BaseCount + 2, 0.0f,  0.45f);
			AddWave(ELegionClass::Marksman,     BaseCount + 2, 0.0f,  0.0f);
			AddWave(ELegionClass::Shieldbearer, BaseCount,     20.0f, 0.0f);
			if (C.DangerRating >= 3)
			{
				AddWave(ELegionClass::Marksman, BaseCount,     26.0f, 0.75f);
			}
		}
		else // Rebels
		{
			AddWave(ELegionClass::Vanguard,     BaseCount + 2, 0.0f, -0.65f);
			AddWave(ELegionClass::Vanguard,     BaseCount + 2, 0.0f,  0.65f);
			AddWave(ELegionClass::Marksman,     BaseCount - 2, 0.0f,  0.0f);
			AddWave(ELegionClass::Vanguard,     BaseCount,     14.0f, -0.9f);
			AddWave(ELegionClass::Pikeman,      BaseCount,     16.0f,  0.9f);
		}

		AvailableContracts.Add(C);
	}

	OnCampaignUpdated.Broadcast();
}

bool UGalacticCampaignSubsystem::AcceptAndDeployContract(int32 ContractId, const UObject* WorldContext)
{
	const FGalacticContract* Found = FindContract(ContractId);
	if (!Found) { return false; }

	// 至少要有一名存活士兵才能部署
	int32 TotalAlive = 0;
	for (const FManagedLegionData& L : PlayerLegions)
	{
		TotalAlive += L.AliveSoldiers;
	}
	if (TotalAlive <= 0)
	{
		UE_LOG(LogERLegion, Warning, TEXT("[Campaign] Cannot deploy: all player legions have 0 soldiers!"));
		return false;
	}

	ActiveContract = *Found;
	bHasActiveContract = true;

	UWorld* World = (GEngine && WorldContext)
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World && GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}
	if (!World) { return false; }

	UE_LOG(LogERLegion, Log, TEXT("[Campaign] Deploying contract '%s' (%s vs %s) -> Lvl_Legion"),
		*ActiveContract.Title,
		*ERGalacticMeta::GetFactionName(ActiveContract.Employer),
		*ERGalacticMeta::GetFactionName(ActiveContract.EnemyFaction));

	UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Legion")));
	return true;
}

void UGalacticCampaignSubsystem::CommitBattleOutcome(bool bVictory, const TMap<int32, int32>& SurvivingByLegionId,
	int32 TotalPlayerCasualties, int32 TotalEnemyEliminated)
{
	FLegionBattleOutcome Out;
	Out.bValid = true;
	Out.bVictory = bVictory;
	Out.bFromContract = bHasActiveContract;
	Out.PlayerCasualties = TotalPlayerCasualties;
	Out.EnemyEliminated = TotalEnemyEliminated;

	// 写回玩家常备兵团战损
	for (FManagedLegionData& L : PlayerLegions)
	{
		if (const int32* Surv = SurvivingByLegionId.Find(L.LegionId))
		{
			L.AliveSoldiers = FMath::Clamp(*Surv, 0, L.MaxSoldiers);
			if (bVictory && L.AliveSoldiers > 0)
			{
				L.BattlesSurvived++;
			}
		}
	}

	if (bHasActiveContract)
	{
		Out.ContractTitle = ActiveContract.Title;
		Out.Employer = ActiveContract.Employer;
		Out.EnemyFaction = ActiveContract.EnemyFaction;

		if (bVictory)
		{
			Out.CreditsEarned = ActiveContract.RewardCredits;
			Out.ControlShiftApplied = ActiveContract.ControlShift;
			Out.ReputationDelta = ActiveContract.ReputationGain;
			Credits += Out.CreditsEarned;

			ShiftGalacticControl(ActiveContract.Employer, ActiveContract.EnemyFaction,
				ActiveContract.ControlShift, ActiveContract.TargetSectorId);

			for (FFactionStatus& F : Factions)
			{
				if (F.Faction == ActiveContract.Employer)
				{
					F.Reputation = FMath::Clamp(F.Reputation + ActiveContract.ReputationGain, -100, 100);
				}
				else if (F.Faction == ActiveContract.EnemyFaction)
				{
					F.Reputation = FMath::Clamp(F.Reputation - ActiveContract.ReputationGain / 2, -100, 100);
				}
			}
		}
		else
		{
			// 败北仍支付最低撤离保障金, 但敌方推进星区控制率
			Out.CreditsEarned = ActiveContract.RewardCredits / 5;
			Out.ControlShiftApplied = -ActiveContract.ControlShift * 0.6f;
			Out.ReputationDelta = -ActiveContract.ReputationGain / 2;
			Credits += Out.CreditsEarned;

			ShiftGalacticControl(ActiveContract.EnemyFaction, ActiveContract.Employer,
				ActiveContract.ControlShift * 0.6f, ActiveContract.TargetSectorId);

			for (FFactionStatus& F : Factions)
			{
				if (F.Faction == ActiveContract.Employer)
				{
					F.Reputation = FMath::Clamp(F.Reputation + Out.ReputationDelta, -100, 100);
				}
			}
		}

		bHasActiveContract = false;
		CycleIndex++;
		RefreshContracts();
	}

	LastOutcome = Out;
	OnCampaignUpdated.Broadcast();
	UE_LOG(LogERLegion, Log, TEXT("[Campaign] Battle committed: Victory=%d, Credits+%d (Total=%d), Casualties=%d"),
		(int32)bVictory, Out.CreditsEarned, Credits, TotalPlayerCasualties);
}

void UGalacticCampaignSubsystem::ShiftGalacticControl(EFactionId Winner, EFactionId Loser, float Amount, int32 SectorId)
{
	for (FFactionStatus& F : Factions)
	{
		if (F.Faction == Winner) { F.GalacticControl += Amount; }
		else if (F.Faction == Loser) { F.GalacticControl = FMath::Max(5.0f, F.GalacticControl - Amount); }
	}
	NormalizeControls();

	if (Sectors.IsValidIndex(SectorId))
	{
		Sectors[SectorId].ControllingFaction = Winner;
		Sectors[SectorId].ConflictIntensity = FMath::Clamp(Sectors[SectorId].ConflictIntensity + 0.15f, 0.1f, 1.0f);
	}
}

void UGalacticCampaignSubsystem::NormalizeControls()
{
	float Sum = 0.0f;
	for (const FFactionStatus& F : Factions) { Sum += F.GalacticControl; }
	if (Sum <= KINDA_SMALL_NUMBER) { return; }
	for (FFactionStatus& F : Factions)
	{
		F.GalacticControl = (F.GalacticControl / Sum) * 100.0f;
	}
}

bool UGalacticCampaignSubsystem::ReinforceLegion(int32 LegionId)
{
	FManagedLegionData* L = FindPlayerLegionMutable(LegionId);
	if (!L) { return false; }
	const int32 Missing = L->GetMissingSoldiers();
	if (Missing <= 0) { return false; }

	const int32 CostPer = L->GetReinforceCostPerSoldier();
	const int32 Affordable = FMath::Min(Missing, Credits / FMath::Max(1, CostPer));
	if (Affordable <= 0) { return false; }

	Credits -= Affordable * CostPer;
	L->AliveSoldiers += Affordable;
	OnCampaignUpdated.Broadcast();
	return true;
}

int32 UGalacticCampaignSubsystem::ReinforceAllLegions()
{
	int32 TotalRestored = 0;
	for (FManagedLegionData& L : PlayerLegions)
	{
		const int32 Missing = L.GetMissingSoldiers();
		if (Missing <= 0) { continue; }
		const int32 CostPer = L.GetReinforceCostPerSoldier();
		const int32 Affordable = FMath::Min(Missing, Credits / FMath::Max(1, CostPer));
		if (Affordable > 0)
		{
			Credits -= Affordable * CostPer;
			L.AliveSoldiers += Affordable;
			TotalRestored += Affordable;
		}
	}
	if (TotalRestored > 0) { OnCampaignUpdated.Broadcast(); }
	return TotalRestored;
}

bool UGalacticCampaignSubsystem::UpgradeLegionSync(int32 LegionId)
{
	FManagedLegionData* L = FindPlayerLegionMutable(LegionId);
	if (!L || L->SyncLevel >= 5) { return false; }
	const int32 Cost = L->GetSyncUpgradeCost();
	if (Credits < Cost) { return false; }

	Credits -= Cost;
	L->SyncLevel++;
	L->MaxSoldiers += 2;
	L->AliveSoldiers += 2;
	OnCampaignUpdated.Broadcast();
	return true;
}

int32 UGalacticCampaignSubsystem::GetRecruitCost(ELegionClass Archetype) const
{
	switch (Archetype)
	{
	case ELegionClass::Shieldbearer: return 480;
	case ELegionClass::Pikeman:      return 450;
	case ELegionClass::Vanguard:     return 520;
	case ELegionClass::Marksman:     return 560;
	default: return 500;
	}
}

bool UGalacticCampaignSubsystem::RecruitLegion(ELegionClass Archetype)
{
	if (PlayerLegions.Num() >= MaxPlayerLegions) { return false; }
	const int32 Cost = GetRecruitCost(Archetype);
	if (Credits < Cost) { return false; }

	Credits -= Cost;
	FManagedLegionData L;
	L.LegionId = NextLegionId++;
	L.Archetype = Archetype;
	L.MaxSoldiers = (Archetype == ELegionClass::Marksman) ? 16 : 20;
	L.AliveSoldiers = L.MaxSoldiers;
	L.SyncLevel = 1;
	L.Callsign = FString::Printf(TEXT("虹膜%s号 // %s"), *ERGalacticMeta::GetChineseOrdinal(L.LegionId), *ERGalacticMeta::GetClassName(Archetype));
	PlayerLegions.Add(L);

	OnCampaignUpdated.Broadcast();
	return true;
}
