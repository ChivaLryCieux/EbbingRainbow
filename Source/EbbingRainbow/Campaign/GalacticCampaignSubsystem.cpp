// Copyright Epic Games, Inc. All Rights Reserved.

#include "Campaign/GalacticCampaignSubsystem.h"

#include "Campaign/GalacticCampaignSaveGame.h"
#include "Core/ERLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	bool IsSupportedFaction(EFactionId Faction)
	{
		return Faction == EFactionId::Empire || Faction == EFactionId::Dominion || Faction == EFactionId::Rebels;
	}

	bool IsSupportedLegionClass(ELegionClass Archetype)
	{
		return Archetype == ELegionClass::Shieldbearer || Archetype == ELegionClass::Pikeman
			|| Archetype == ELegionClass::Vanguard || Archetype == ELegionClass::Marksman;
	}

	bool IsSupportedTacticalProfile(ELegionTacticalProfile Profile)
	{
		return Profile == ELegionTacticalProfile::Balanced || Profile == ELegionTacticalProfile::HoldHighGround
			|| Profile == ELegionTacticalProfile::AssaultHighGround || Profile == ELegionTacticalProfile::ValleyChokepoint
			|| Profile == ELegionTacticalProfile::OpenManeuver;
	}

	bool HasCampaignDebugOverride()
	{
		static const TCHAR* ShotFlags[] = {
			TEXT("LegionShot"), TEXT("SkyShot"), TEXT("MapShot"), TEXT("DeployShot"),
			TEXT("LegionCloseup"), TEXT("LegionAIShot"), TEXT("ResultShot")
		};
		for (const TCHAR* Flag : ShotFlags)
		{
			if (FParse::Param(FCommandLine::Get(), Flag)) { return true; }
		}

		int32 Ignored = 0;
		return FParse::Value(FCommandLine::Get(), TEXT("-LegionSeed="), Ignored)
			|| FParse::Value(FCommandLine::Get(), TEXT("-LegionLegions="), Ignored)
			|| FParse::Value(FCommandLine::Get(), TEXT("-LegionSoldiers="), Ignored);
	}
}

void UGalacticCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGameInstance* GameInstance = GetGameInstance();
	const FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
	const bool bGameWorld = Context && Context->WorldType == EWorldType::Game;
	const bool bVolatile = !bGameWorld || IsRunningDedicatedServer() || IsRunningCommandlet()
		|| GIsAutomationTesting || HasCampaignDebugOverride();

	if (bVolatile)
	{
		SaveSlotPrefix.Empty();
		SaveStatus = EGalacticCampaignSaveStatus::Volatile;
		InitializeCampaignState(2026);
		return;
	}

	SaveSlotPrefix = TEXT("ER_GalacticCampaign");
	SaveStatus = EGalacticCampaignSaveStatus::Volatile;
	InitFactionsAndSectors();
	LoadCampaign(true);
}

UGalacticCampaignSubsystem* UGalacticCampaignSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine) { return nullptr; }
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World) { return nullptr; }
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UGalacticCampaignSubsystem>() : nullptr;
}

FText UGalacticCampaignSubsystem::GetSaveNotice() const
{
	switch (SaveStatus)
	{
	case EGalacticCampaignSaveStatus::Recovered:    return INVTEXT("存档损坏，已恢复可用备份");
	case EGalacticCampaignSaveStatus::WriteFailed:  return INVTEXT("进度未保存；可在设置中重试");
	case EGalacticCampaignSaveStatus::LoadFailed:   return INVTEXT("无法读取战役存档；原存档已保留");
	case EGalacticCampaignSaveStatus::Incompatible: return INVTEXT("战役存档来自较新的版本；原存档已保留");
	default:                                        return FText::GetEmpty();
	}
}

void UGalacticCampaignSubsystem::ResetCampaign(int32 Seed)
{
	if (!IsCampaignAvailable() || bHasActiveContract || LastSavedGeneration != 0)
	{
		return;
	}
	InitializeCampaignState(Seed);
	PublishCampaignChange();
}

void UGalacticCampaignSubsystem::InitializeCampaignState(int32 Seed)
{
	CampaignSeed = Seed;
	Credits = 1200;
	CycleIndex = 1;
	NextLegionId = 1;
	NextContractId = 1;
	ContractRefreshNonce = 0;
	bHasActiveContract = false;
	ActiveContract = FGalacticContract();
	LastOutcome = FLegionBattleOutcome();

	InitFactionsAndSectors();
	InitStarterLegions();
	GenerateContracts();

	UE_LOG(LogERLegion, Log, TEXT("[Campaign] Initialized Iris Foundation campaign (Seed=%d, Credits=%d, Legions=%d)"),
		CampaignSeed, Credits, PlayerLegions.Num());
}

void UGalacticCampaignSubsystem::InitFactionsAndSectors()
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

	AddStarter(*ERGalacticMeta::GetBrigadeName(1), ELegionClass::Shieldbearer, 20);
	AddStarter(*ERGalacticMeta::GetBrigadeName(2), ELegionClass::Pikeman,      20);
	AddStarter(*ERGalacticMeta::GetBrigadeName(3), ELegionClass::Vanguard,     18);
	AddStarter(*ERGalacticMeta::GetBrigadeName(4), ELegionClass::Marksman,     16);
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

void UGalacticCampaignSubsystem::GenerateContracts()
{
	AvailableContracts.Reset();
	++ContractRefreshNonce;
	FRandomStream Rng(CampaignSeed + CycleIndex * 7919 + ContractRefreshNonce * 3571);

	struct FContractTemplate
	{
		EFactionId Employer;
		EFactionId Enemy;
		const TCHAR* Title;
		const TCHAR* Briefing;
		int32 Danger;
		ELegionTacticalProfile Profile;
	};

	static const FContractTemplate Templates[] = {
		{ EFactionId::Empire, EFactionId::Rebels,
			TEXT("「铁幕肃清」边境高地防线清剿"),
			TEXT("自由叛军占据了裂隙制高点并部署多支伏击群。帝国军部雇佣虹膜基金会接管前线战术神经，彻底粉碎叛军攻势。"),
			2, ELegionTacticalProfile::AssaultHighGround },
		{ EFactionId::Rebels, EFactionId::Empire,
			TEXT("「断虹行动」帝国重装纵队伏击"),
			TEXT("帝国第7重盾先遣队正穿越山谷补给线。叛军同盟委托基金会指挥突击集群切入侧背，阻止其完成合围。"),
			3, ELegionTacticalProfile::ValleyChokepoint },
		{ EFactionId::Dominion, EFactionId::Rebels,
			TEXT("「晶脉守望」矿区高台阻击战"),
			TEXT("外环叛军空降突击队企图夺取自治领白矮星精炼枢纽。守住高地并击溃来袭增援波次。"),
			2, ELegionTacticalProfile::HoldHighGround },
		{ EFactionId::Empire, EFactionId::Dominion,
			TEXT("「轨道征税」自治领防区强行突破"),
			TEXT("自治领拒绝履行星环税赋条约。帝国授权基金会以高强度多波次攻势压垮其长戟与远程联防线。"),
			4, ELegionTacticalProfile::AssaultHighGround },
		{ EFactionId::Dominion, EFactionId::Empire,
			TEXT("「自由航道」中继站反包围作战"),
			TEXT("帝国舰队封锁了霜桥中继站外围平台。自治领愿以高额科研酬金聘请基金会远程调度兵团突围。"),
			3, ELegionTacticalProfile::OpenManeuver },
		{ EFactionId::Rebels, EFactionId::Dominion,
			TEXT("「星火夺取」自治领前哨突袭"),
			TEXT("叛军急需自治领前哨的神经电容阵列。利用地形遮蔽接近其远程阵地并全歼守备兵团。"),
			1, ELegionTacticalProfile::ValleyChokepoint }
	};

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
		C.TerrainSeed = CampaignSeed + CycleIndex * 97 + Slot * 313 + ContractRefreshNonce * 1009;
		C.TacticalProfile = Tpl.Profile;
		C.DangerRating = FMath::Clamp(Tpl.Danger + (CycleIndex > 3 ? 1 : 0), 1, 5);
		C.RewardCredits = 360 + C.DangerRating * 190 + Rng.RandRange(0, 80);
		C.ControlShift = 4.5f + C.DangerRating * 2.0f;
		C.ReputationGain = 10 + C.DangerRating * 4;

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
			if (C.DangerRating >= 3) { AddWave(ELegionClass::Pikeman, BaseCount + 2, 24.0f, 0.8f); }
		}
		else if (C.EnemyFaction == EFactionId::Dominion)
		{
			AddWave(ELegionClass::Pikeman,      BaseCount + 2, 0.0f, -0.45f);
			AddWave(ELegionClass::Pikeman,      BaseCount + 2, 0.0f,  0.45f);
			AddWave(ELegionClass::Marksman,     BaseCount + 2, 0.0f,  0.0f);
			AddWave(ELegionClass::Shieldbearer, BaseCount,     20.0f, 0.0f);
			if (C.DangerRating >= 3) { AddWave(ELegionClass::Marksman, BaseCount, 26.0f, 0.75f); }
		}
		else
		{
			AddWave(ELegionClass::Vanguard,     BaseCount + 2, 0.0f, -0.65f);
			AddWave(ELegionClass::Vanguard,     BaseCount + 2, 0.0f,  0.65f);
			AddWave(ELegionClass::Marksman,     BaseCount - 2, 0.0f,  0.0f);
			AddWave(ELegionClass::Vanguard,     BaseCount,     14.0f, -0.9f);
			AddWave(ELegionClass::Pikeman,      BaseCount,     16.0f,  0.9f);
		}
		AvailableContracts.Add(C);
	}
}

void UGalacticCampaignSubsystem::RefreshContracts()
{
	if (!IsCampaignAvailable() || bHasActiveContract) { return; }
	GenerateContracts();
	PublishCampaignChange();
}

bool UGalacticCampaignSubsystem::BeginContract(int32 ContractId)
{
	if (!IsCampaignAvailable() || bHasActiveContract) { return false; }
	const FGalacticContract* Found = FindContract(ContractId);
	if (!Found) { return false; }

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

	const bool bNeedsCheckpoint = !SaveSlotPrefix.IsEmpty() && (bSaveDirty || LastSavedGeneration == 0);
	if (bNeedsCheckpoint && !SaveCampaign())
	{
		UE_LOG(LogERLegion, Warning, TEXT("[Campaign] Deployment blocked: durable campaign checkpoint could not be written."));
		OnCampaignUpdated.Broadcast();
		return false;
	}

	ActiveContract = *Found;
	bHasActiveContract = true;
	if (bNeedsCheckpoint)
	{
		OnCampaignUpdated.Broadcast();
	}
	return true;
}

bool UGalacticCampaignSubsystem::AcceptAndDeployContract(int32 ContractId, const UObject* WorldContext)
{
	UWorld* World = (GEngine && WorldContext)
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World || !World->IsGameWorld()) { return false; }
	if (!BeginContract(ContractId)) { return false; }

	UE_LOG(LogERLegion, Log, TEXT("[Campaign] Deploying contract '%s' (%s vs %s) -> Lvl_Legion"),
		*ActiveContract.Title,
		*ERGalacticMeta::GetFactionName(ActiveContract.Employer),
		*ERGalacticMeta::GetFactionName(ActiveContract.EnemyFaction));

	UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Battle/Maps/Lvl_Legion")));
	return true;
}

void UGalacticCampaignSubsystem::CommitBattleOutcome(int32 ContractId, bool bVictory,
	const TMap<int32, int32>& SurvivingByLegionId, int32 TotalPlayerCasualties, int32 TotalEnemyEliminated)
{
	if (!bHasActiveContract || ContractId != ActiveContract.ContractId)
	{
		return;
	}

	FLegionBattleOutcome Out;
	Out.bValid = true;
	Out.bVictory = bVictory;
	Out.bFromContract = true;
	Out.PlayerCasualties = TotalPlayerCasualties;
	Out.EnemyEliminated = TotalEnemyEliminated;
	Out.ContractTitle = ActiveContract.Title;
	Out.Employer = ActiveContract.Employer;
	Out.EnemyFaction = ActiveContract.EnemyFaction;

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
	ActiveContract = FGalacticContract();
	CycleIndex++;
	GenerateContracts();
	LastOutcome = Out;
	PublishCampaignChange();

	UE_LOG(LogERLegion, Log, TEXT("[Campaign] Battle committed: Contract=%d Victory=%d, Credits+%d (Total=%d), Casualties=%d"),
		ContractId, (int32)bVictory, Out.CreditsEarned, Credits, TotalPlayerCasualties);
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

void UGalacticCampaignSubsystem::PublishCampaignChange()
{
	bSaveDirty = !SaveSlotPrefix.IsEmpty();
	if (bSaveDirty)
	{
		SaveCampaign();
	}
	OnCampaignUpdated.Broadcast();
}

bool UGalacticCampaignSubsystem::LoadCampaign(bool bCreateIfMissing)
{
	if (SaveSlotPrefix.IsEmpty()) { return false; }

	const FString SlotNames[2] = {
		SaveSlotPrefix + TEXT("_A"),
		SaveSlotPrefix + TEXT("_B")
	};
	ERGalacticSave::FReadResult Reads[2] = {
		ERGalacticSave::ReadSlot(SlotNames[0]),
		ERGalacticSave::ReadSlot(SlotNames[1])
	};

	bool bAnyFile = false;
	bool bAnyInvalid = false;
	int64 BestFutureGeneration = 0;
	int32 BestSlot = INDEX_NONE;
	int64 BestGeneration = 0;
	TStrongObjectPtr<UGalacticCampaignSaveGame> BestSnapshot;

	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		ERGalacticSave::FReadResult& Read = Reads[Slot];
		if (Read.State == ERGalacticSave::ESlotState::Missing) { continue; }
		bAnyFile = true;

		if (Read.State == ERGalacticSave::ESlotState::NewerVersion)
		{
			BestFutureGeneration = FMath::Max(BestFutureGeneration, Read.Generation);
			continue;
		}
		if (Read.State == ERGalacticSave::ESlotState::Valid && Read.Snapshot && IsValidSnapshot(*Read.Snapshot))
		{
			if (Read.Generation > BestGeneration)
			{
				BestGeneration = Read.Generation;
				BestSlot = Slot;
				BestSnapshot.Reset(Read.Snapshot);
			}
		}
		else
		{
			bAnyInvalid = true;
			UE_LOG(LogERLegion, Warning, TEXT("[Campaign] Save slot %s is damaged or semantically invalid."), *SlotNames[Slot]);
		}
	}

	if (BestFutureGeneration > 0 && (BestSlot == INDEX_NONE || BestFutureGeneration >= BestGeneration))
	{
		SaveStatus = EGalacticCampaignSaveStatus::Incompatible;
		LastSavedSlot = INDEX_NONE;
		LastSavedGeneration = 0;
		bSaveDirty = false;
		UE_LOG(LogERLegion, Warning, TEXT("[Campaign] Newer unsupported campaign save generation detected (%lld); existing saves were preserved."), BestFutureGeneration);
		return false;
	}

	if (BestSlot != INDEX_NONE && BestSnapshot.IsValid())
	{
		ApplySnapshot(*BestSnapshot.Get());
		LastSavedSlot = BestSlot;
		LastSavedGeneration = BestGeneration;
		bSaveDirty = false;
		SaveStatus = bAnyInvalid ? EGalacticCampaignSaveStatus::Recovered : EGalacticCampaignSaveStatus::Saved;
		UE_LOG(LogERLegion, Log, TEXT("[Campaign] Loaded save slot %s generation %lld%s."),
			*SlotNames[BestSlot], BestGeneration, bAnyInvalid ? TEXT(" (recovered past damaged sibling)") : TEXT(""));
		return true;
	}

	if (bAnyFile)
	{
		SaveStatus = EGalacticCampaignSaveStatus::LoadFailed;
		LastSavedSlot = INDEX_NONE;
		LastSavedGeneration = 0;
		bSaveDirty = false;
		UE_LOG(LogERLegion, Warning, TEXT("[Campaign] No readable campaign save; existing save files were preserved."));
		return false;
	}

	if (bCreateIfMissing)
	{
		InitializeCampaignState(2026);
		PublishCampaignChange();
		UE_LOG(LogERLegion, Log, TEXT("[Campaign] Created fresh persistent campaign seed 2026."));
		return IsCampaignAvailable();
	}

	SaveStatus = EGalacticCampaignSaveStatus::LoadFailed;
	return false;
}

bool UGalacticCampaignSubsystem::SaveCampaign()
{
	if (SaveSlotPrefix.IsEmpty())
	{
		return true;
	}
	if (!IsCampaignAvailable() || bHasActiveContract)
	{
		return false;
	}

	UGalacticCampaignSaveGame* Snapshot = NewObject<UGalacticCampaignSaveGame>(this);
	Snapshot->CampaignSeed = CampaignSeed;
	Snapshot->GalaxyCoins = Credits;
	Snapshot->CycleIndex = CycleIndex;
	Snapshot->NextLegionId = NextLegionId;
	Snapshot->NextContractId = NextContractId;
	Snapshot->ContractRefreshNonce = ContractRefreshNonce;
	for (const FFactionStatus& F : Factions)
	{
		FGalacticSavedFaction SavedFaction;
		SavedFaction.Faction = F.Faction;
		SavedFaction.Reputation = F.Reputation;
		SavedFaction.GalacticControl = F.GalacticControl;
		Snapshot->Factions.Add(SavedFaction);
	}
	for (const FGalacticSector& S : Sectors)
	{
		FGalacticSavedSector SavedSector;
		SavedSector.SectorId = S.SectorId;
		SavedSector.Owner = S.ControllingFaction;
		SavedSector.ConflictIntensity = S.ConflictIntensity;
		Snapshot->Sectors.Add(SavedSector);
	}
	Snapshot->PlayerLegions = PlayerLegions;
	Snapshot->AvailableContracts = AvailableContracts;
	Snapshot->LastOutcome = LastOutcome;

	if (!IsValidSnapshot(*Snapshot))
	{
		bSaveDirty = true;
		SaveStatus = EGalacticCampaignSaveStatus::WriteFailed;
		UE_LOG(LogERLegion, Error, TEXT("[Campaign] Refusing to write semantically invalid campaign snapshot."));
		return false;
	}

	const int32 TargetSlot = (LastSavedSlot == 0) ? 1 : 0;
	const FString SlotName = SaveSlotPrefix + (TargetSlot == 0 ? TEXT("_A") : TEXT("_B"));
	const int64 Generation = LastSavedGeneration + 1;
	if (!ERGalacticSave::WriteSlot(SlotName, *Snapshot, Generation))
	{
		bSaveDirty = true;
		SaveStatus = EGalacticCampaignSaveStatus::WriteFailed;
		UE_LOG(LogERLegion, Warning, TEXT("[Campaign] Failed writing campaign save slot %s."), *SlotName);
		return false;
	}

	const ERGalacticSave::FReadResult Verify = ERGalacticSave::ReadSlot(SlotName);
	if (Verify.State != ERGalacticSave::ESlotState::Valid || !Verify.Snapshot || Verify.Generation != Generation)
	{
		bSaveDirty = true;
		SaveStatus = EGalacticCampaignSaveStatus::WriteFailed;
		UE_LOG(LogERLegion, Warning, TEXT("[Campaign] Read-back verification failed for campaign save slot %s."), *SlotName);
		return false;
	}

	LastSavedSlot = TargetSlot;
	LastSavedGeneration = Generation;
	bSaveDirty = false;
	SaveStatus = EGalacticCampaignSaveStatus::Saved;
	return true;
}

bool UGalacticCampaignSubsystem::RetryCampaignSave()
{
	bool bResult = true;
	if (SaveStatus == EGalacticCampaignSaveStatus::LoadFailed || SaveStatus == EGalacticCampaignSaveStatus::Incompatible)
	{
		bResult = LoadCampaign(false);
		OnCampaignUpdated.Broadcast();
		return bResult;
	}
	if (bSaveDirty)
	{
		bResult = SaveCampaign();
	}
	OnCampaignUpdated.Broadcast();
	return bResult;
}

bool UGalacticCampaignSubsystem::IsValidSnapshot(const UGalacticCampaignSaveGame& Snapshot) const
{
	if (Snapshot.CycleIndex <= 0 || Snapshot.ContractRefreshNonce <= 0 || Snapshot.GalaxyCoins < 0)
	{
		return false;
	}
	if (Snapshot.Factions.Num() != Factions.Num() || Snapshot.Sectors.Num() != Sectors.Num())
	{
		return false;
	}

	float ControlSum = 0.0f;
	for (const FGalacticSavedFaction& SavedFaction : Snapshot.Factions)
	{
		if (!IsSupportedFaction(SavedFaction.Faction)
			|| SavedFaction.Reputation < -100 || SavedFaction.Reputation > 100
			|| !FMath::IsFinite(SavedFaction.GalacticControl)
			|| SavedFaction.GalacticControl < 0.0f || SavedFaction.GalacticControl > 100.0f)
		{
			return false;
		}
		ControlSum += SavedFaction.GalacticControl;

		int32 CatalogMatches = 0;
		int32 SavedMatches = 0;
		for (const FFactionStatus& Catalog : Factions)
		{
			if (Catalog.Faction == SavedFaction.Faction) { ++CatalogMatches; }
		}
		for (const FGalacticSavedFaction& Other : Snapshot.Factions)
		{
			if (Other.Faction == SavedFaction.Faction) { ++SavedMatches; }
		}
		if (CatalogMatches != 1 || SavedMatches != 1) { return false; }
	}
	if (FMath::Abs(ControlSum - 100.0f) > 0.01f)
	{
		return false;
	}

	for (const FGalacticSavedSector& SavedSector : Snapshot.Sectors)
	{
		if (!IsSupportedFaction(SavedSector.Owner)
			|| !FMath::IsFinite(SavedSector.ConflictIntensity)
			|| SavedSector.ConflictIntensity < 0.0f || SavedSector.ConflictIntensity > 1.0f)
		{
			return false;
		}
		int32 CatalogMatches = 0;
		int32 SavedMatches = 0;
		for (const FGalacticSector& Catalog : Sectors)
		{
			if (Catalog.SectorId == SavedSector.SectorId) { ++CatalogMatches; }
		}
		for (const FGalacticSavedSector& Other : Snapshot.Sectors)
		{
			if (Other.SectorId == SavedSector.SectorId) { ++SavedMatches; }
		}
		if (CatalogMatches != 1 || SavedMatches != 1) { return false; }
	}

	if (Snapshot.PlayerLegions.Num() < 1 || Snapshot.PlayerLegions.Num() > MaxPlayerLegions)
	{
		return false;
	}
	int32 HighestLegionId = 0;
	for (const FManagedLegionData& Legion : Snapshot.PlayerLegions)
	{
		if (Legion.LegionId <= 0 || Legion.MaxSoldiers <= 0
			|| Legion.AliveSoldiers < 0 || Legion.AliveSoldiers > Legion.MaxSoldiers
			|| Legion.SyncLevel < 1 || Legion.SyncLevel > 5 || Legion.BattlesSurvived < 0
			|| !IsSupportedLegionClass(Legion.Archetype))
		{
			return false;
		}
		int32 IdMatches = 0;
		for (const FManagedLegionData& Other : Snapshot.PlayerLegions)
		{
			if (Other.LegionId == Legion.LegionId) { ++IdMatches; }
		}
		if (IdMatches != 1) { return false; }
		HighestLegionId = FMath::Max(HighestLegionId, Legion.LegionId);
	}
	if (Snapshot.NextLegionId <= HighestLegionId)
	{
		return false;
	}

	if (Snapshot.AvailableContracts.Num() != 3)
	{
		return false;
	}
	int32 HighestContractId = 0;
	for (const FGalacticContract& Contract : Snapshot.AvailableContracts)
	{
		if (Contract.ContractId <= 0 || !FindSector(Contract.TargetSectorId)
			|| !IsSupportedFaction(Contract.Employer) || !IsSupportedFaction(Contract.EnemyFaction)
			|| Contract.Employer == Contract.EnemyFaction
			|| !IsSupportedTacticalProfile(Contract.TacticalProfile)
			|| Contract.DangerRating < 1 || Contract.DangerRating > 5
			|| Contract.RewardCredits < 0
			|| !FMath::IsFinite(Contract.ControlShift) || Contract.ControlShift < 0.0f || Contract.ControlShift > 100.0f
			|| Contract.EnemyWaves.Num() <= 0)
		{
			return false;
		}
		int32 IdMatches = 0;
		for (const FGalacticContract& Other : Snapshot.AvailableContracts)
		{
			if (Other.ContractId == Contract.ContractId) { ++IdMatches; }
		}
		if (IdMatches != 1) { return false; }
		for (const FEnemyWaveLegionSpec& Wave : Contract.EnemyWaves)
		{
			if (!IsSupportedLegionClass(Wave.Archetype) || Wave.SoldierCount <= 0
				|| !FMath::IsFinite(Wave.DelaySeconds) || Wave.DelaySeconds < 0.0f
				|| !FMath::IsFinite(Wave.LaneBias) || Wave.LaneBias < -1.0f || Wave.LaneBias > 1.0f)
			{
				return false;
			}
		}
		HighestContractId = FMath::Max(HighestContractId, Contract.ContractId);
	}
	if (Snapshot.NextContractId <= HighestContractId)
	{
		return false;
	}

	const FLegionBattleOutcome& Outcome = Snapshot.LastOutcome;
	if (Outcome.bValid)
	{
		if (!IsSupportedFaction(Outcome.Employer) || !IsSupportedFaction(Outcome.EnemyFaction)
			|| Outcome.CreditsEarned < 0 || Outcome.PlayerCasualties < 0 || Outcome.EnemyEliminated < 0
			|| !FMath::IsFinite(Outcome.ControlShiftApplied))
		{
			return false;
		}
	}
	return true;
}

void UGalacticCampaignSubsystem::ApplySnapshot(const UGalacticCampaignSaveGame& Snapshot)
{
	CampaignSeed = Snapshot.CampaignSeed;
	Credits = Snapshot.GalaxyCoins;
	CycleIndex = Snapshot.CycleIndex;
	NextLegionId = Snapshot.NextLegionId;
	NextContractId = Snapshot.NextContractId;
	ContractRefreshNonce = Snapshot.ContractRefreshNonce;

	for (FFactionStatus& Faction : Factions)
	{
		for (const FGalacticSavedFaction& SavedFaction : Snapshot.Factions)
		{
			if (SavedFaction.Faction == Faction.Faction)
			{
				Faction.Reputation = SavedFaction.Reputation;
				Faction.GalacticControl = SavedFaction.GalacticControl;
				break;
			}
		}
	}
	for (FGalacticSector& Sector : Sectors)
	{
		for (const FGalacticSavedSector& SavedSector : Snapshot.Sectors)
		{
			if (SavedSector.SectorId == Sector.SectorId)
			{
				Sector.ControllingFaction = SavedSector.Owner;
				Sector.ConflictIntensity = SavedSector.ConflictIntensity;
				break;
			}
		}
	}

	PlayerLegions = Snapshot.PlayerLegions;
	AvailableContracts = Snapshot.AvailableContracts;
	LastOutcome = Snapshot.LastOutcome;
	ActiveContract = FGalacticContract();
	bHasActiveContract = false;
	bSaveDirty = false;
}

bool UGalacticCampaignSubsystem::ReinforceLegion(int32 LegionId)
{
	if (!IsCampaignAvailable() || bHasActiveContract) { return false; }
	FManagedLegionData* L = FindPlayerLegionMutable(LegionId);
	if (!L) { return false; }
	const int32 Missing = L->GetMissingSoldiers();
	if (Missing <= 0) { return false; }

	const int32 CostPer = L->GetReinforceCostPerSoldier();
	const int32 Affordable = FMath::Min(Missing, Credits / FMath::Max(1, CostPer));
	if (Affordable <= 0) { return false; }

	Credits -= Affordable * CostPer;
	L->AliveSoldiers += Affordable;
	PublishCampaignChange();
	return true;
}

int32 UGalacticCampaignSubsystem::ReinforceAllLegions()
{
	if (!IsCampaignAvailable() || bHasActiveContract) { return 0; }
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
	if (TotalRestored > 0) { PublishCampaignChange(); }
	return TotalRestored;
}

bool UGalacticCampaignSubsystem::UpgradeLegionSync(int32 LegionId)
{
	if (!IsCampaignAvailable() || bHasActiveContract) { return false; }
	FManagedLegionData* L = FindPlayerLegionMutable(LegionId);
	if (!L || L->SyncLevel >= 5) { return false; }
	const int32 Cost = L->GetSyncUpgradeCost();
	if (Credits < Cost) { return false; }

	Credits -= Cost;
	L->SyncLevel++;
	L->MaxSoldiers += 2;
	L->AliveSoldiers += 2;
	PublishCampaignChange();
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
	if (!IsCampaignAvailable() || bHasActiveContract) { return false; }
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
	L.Callsign = ERGalacticMeta::GetBrigadeName(L.LegionId);
	PlayerLegions.Add(L);

	PublishCampaignChange();
	return true;
}
