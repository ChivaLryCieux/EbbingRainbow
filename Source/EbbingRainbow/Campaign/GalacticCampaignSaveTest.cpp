// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/GalacticCampaignSaveGame.h"
#include "Campaign/GalacticCampaignSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FCampaignTestInstance
	{
		TStrongObjectPtr<UGameInstance> GameInstance;
		TStrongObjectPtr<UGalacticCampaignSubsystem> Campaign;
	};

	void DeleteCampaignTestSlots(const FString& Prefix)
	{
		UGameplayStatics::DeleteGameInSlot(Prefix + TEXT("_A"), 0);
		UGameplayStatics::DeleteGameInSlot(Prefix + TEXT("_B"), 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGalacticCampaignSaveTest, "EbbingRainbow.Campaign.Save",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGalacticCampaignSaveTest::RunTest(const FString& Parameters)
{
	const FString Prefix = TEXT("ER_GalacticCampaignTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	DeleteCampaignTestSlots(Prefix);

	auto CreateInstance = [&]() -> FCampaignTestInstance
	{
		FCampaignTestInstance Instance;
		Instance.GameInstance.Reset(NewObject<UGameInstance>());
		Instance.Campaign.Reset(NewObject<UGalacticCampaignSubsystem>(Instance.GameInstance.Get()));
		Instance.Campaign->SaveSlotPrefix = Prefix;
		Instance.Campaign->InitFactionsAndSectors();
		return Instance;
	};

	// ---- 初始战役: 1200 银河币 + 三份合同, 首张耐久快照落盘 ----
	FCampaignTestInstance Initial = CreateInstance();
	TestTrue(TEXT("initial persistent campaign loads"), Initial.Campaign->LoadCampaign(true));
	TestEqual(TEXT("initial credits"), Initial.Campaign->GetCredits(), 1200);
	TestEqual(TEXT("initial offers"), Initial.Campaign->GetAvailableContracts().Num(), 3);
	TestEqual(TEXT("initial durable generation"), Initial.Campaign->LastSavedGeneration, (int64)1);
	TestEqual(TEXT("initial save status"), (int32)Initial.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::Saved);

	// ---- 招募与同步升级: 余额和编制变化必须可重载 ----
	TestTrue(TEXT("recruit succeeds"), Initial.Campaign->RecruitLegion(ELegionClass::Vanguard));
	TestTrue(TEXT("sync upgrade succeeds"), Initial.Campaign->UpgradeLegionSync(1));
	TestEqual(TEXT("balance after recruit and upgrade"), Initial.Campaign->GetCredits(), 400);
	TestEqual(TEXT("roster size after recruit"), Initial.Campaign->GetPlayerLegions().Num(), 5);

	Initial.Campaign->RefreshContracts();
	const FGalacticContract RefreshedOffer = Initial.Campaign->GetAvailableContracts()[0];
	const int32 RefreshedNonce = Initial.Campaign->ContractRefreshNonce;
	const int64 RefreshedGeneration = Initial.Campaign->LastSavedGeneration;

	FCampaignTestInstance Reloaded = CreateInstance();
	TestTrue(TEXT("reload after roster/refresh"), Reloaded.Campaign->LoadCampaign(false));
	TestEqual(TEXT("reloaded credits"), Reloaded.Campaign->GetCredits(), 400);
	TestEqual(TEXT("reloaded roster size"), Reloaded.Campaign->GetPlayerLegions().Num(), 5);
	TestEqual(TEXT("reloaded sync level"), Reloaded.Campaign->GetPlayerLegions()[0].SyncLevel, 2);
	TestEqual(TEXT("reloaded seed"), Reloaded.Campaign->CampaignSeed, 2026);
	TestEqual(TEXT("reloaded nonce"), Reloaded.Campaign->ContractRefreshNonce, RefreshedNonce);
	TestEqual(TEXT("reloaded generation"), Reloaded.Campaign->LastSavedGeneration, RefreshedGeneration);
	TestEqual(TEXT("offer count after reload"), Reloaded.Campaign->GetAvailableContracts().Num(), 3);
	const FGalacticContract& ReloadedOffer = Reloaded.Campaign->GetAvailableContracts()[0];
	TestEqual(TEXT("offer id persisted"), ReloadedOffer.ContractId, RefreshedOffer.ContractId);
	TestEqual(TEXT("offer terrain seed persisted"), ReloadedOffer.TerrainSeed, RefreshedOffer.TerrainSeed);
	TestEqual(TEXT("offer reward persisted"), ReloadedOffer.RewardCredits, RefreshedOffer.RewardCredits);
	TestEqual(TEXT("offer wave count persisted"), ReloadedOffer.EnemyWaves.Num(), RefreshedOffer.EnemyWaves.Num());
	if (ReloadedOffer.EnemyWaves.Num() > 0 && RefreshedOffer.EnemyWaves.Num() > 0)
	{
		TestEqual(TEXT("offer wave archetype persisted"), (int32)ReloadedOffer.EnemyWaves[0].Archetype, (int32)RefreshedOffer.EnemyWaves[0].Archetype);
		TestEqual(TEXT("offer wave soldiers persisted"), ReloadedOffer.EnemyWaves[0].SoldierCount, RefreshedOffer.EnemyWaves[0].SoldierCount);
		TestTrue(TEXT("offer wave delay persisted"), FMath::IsNearlyEqual(ReloadedOffer.EnemyWaves[0].DelaySeconds, RefreshedOffer.EnemyWaves[0].DelaySeconds));
		TestTrue(TEXT("offer wave lane persisted"), FMath::IsNearlyEqual(ReloadedOffer.EnemyWaves[0].LaneBias, RefreshedOffer.EnemyWaves[0].LaneBias));
	}

	// ---- 出击检查点: 重启后合同仍在, 活跃合同不持久化, 无经济/伤亡副作用 ----
	const int32 DeploymentId = ReloadedOffer.ContractId;
	const int32 CreditsBeforeDeploy = Reloaded.Campaign->GetCredits();
	TestTrue(TEXT("begin contract succeeds"), Reloaded.Campaign->BeginContract(DeploymentId));
	TestTrue(TEXT("contract active before battle"), Reloaded.Campaign->HasActiveContract());

	FCampaignTestInstance CheckpointReload = CreateInstance();
	TestTrue(TEXT("checkpoint reload"), CheckpointReload.Campaign->LoadCampaign(false));
	TestFalse(TEXT("active contract is not persisted"), CheckpointReload.Campaign->HasActiveContract());
	TestTrue(TEXT("same offer survives checkpoint"), CheckpointReload.Campaign->FindContract(DeploymentId) != nullptr);
	TestEqual(TEXT("checkpoint credits unchanged"), CheckpointReload.Campaign->GetCredits(), CreditsBeforeDeploy);
	TestFalse(TEXT("checkpoint outcome remains invalid"), CheckpointReload.Campaign->GetLastOutcome().bValid);

	// ---- 错合同/无合同结算必须完全无副作用 ----
	const int32 CreditsBeforeBogus = CheckpointReload.Campaign->GetCredits();
	const int32 CycleBeforeBogus = CheckpointReload.Campaign->GetCycleIndex();
	CheckpointReload.Campaign->CommitBattleOutcome(DeploymentId + 1000, true, TMap<int32, int32>(), 0, 0);
	CheckpointReload.Campaign->CommitBattleOutcome(DeploymentId, true, TMap<int32, int32>(), 0, 0);
	TestEqual(TEXT("bogus commits do not change credits"), CheckpointReload.Campaign->GetCredits(), CreditsBeforeBogus);
	TestEqual(TEXT("bogus commits do not change cycle"), CheckpointReload.Campaign->GetCycleIndex(), CycleBeforeBogus);
	TestFalse(TEXT("bogus commits do not create outcome"), CheckpointReload.Campaign->GetLastOutcome().bValid);

	TestTrue(TEXT("wrong contract cannot begin"), !CheckpointReload.Campaign->BeginContract(DeploymentId + 1000));
	TestFalse(TEXT("wrong begin leaves no active contract"), CheckpointReload.Campaign->HasActiveContract());

	// ---- 有效胜利: 周期、幸存者、报酬、战报、阵营控制/星区/声望与 next 计数器全部持久化 ----
	TestTrue(TEXT("valid contract remains active for win"), Reloaded.Campaign->HasActiveContract());
	const FGalacticContract WinContract = Reloaded.Campaign->GetActiveContract();
	const int32 CreditsBeforeWin = Reloaded.Campaign->GetCredits();
	const int32 CycleBeforeWin = Reloaded.Campaign->GetCycleIndex();
	const FFactionStatus* EmployerBefore = Reloaded.Campaign->FindFaction(WinContract.Employer);
	const FGalacticSector* SectorBefore = Reloaded.Campaign->FindSector(WinContract.TargetSectorId);
	TestTrue(TEXT("employer and sector exist"), EmployerBefore != nullptr && SectorBefore != nullptr);
	const int32 EmployerRepBefore = EmployerBefore ? EmployerBefore->Reputation : 0;
	const float EmployerControlBefore = EmployerBefore ? EmployerBefore->GalacticControl : 0.0f;
	const float SectorIntensityBefore = SectorBefore ? SectorBefore->ConflictIntensity : 0.0f;

	TMap<int32, int32> Survivors;
	const int32 SurvivorLegionId = Reloaded.Campaign->GetPlayerLegions()[0].LegionId;
	const int32 SurvivorCountBefore = Reloaded.Campaign->GetPlayerLegions()[0].AliveSoldiers;
	const int32 SurvivorBattlesBefore = Reloaded.Campaign->GetPlayerLegions()[0].BattlesSurvived;
	for (const FManagedLegionData& Legion : Reloaded.Campaign->GetPlayerLegions())
	{
		Survivors.Add(Legion.LegionId, Legion.AliveSoldiers);
	}
	Survivors[SurvivorLegionId] = FMath::Max(0, SurvivorCountBefore - 1);
	Reloaded.Campaign->CommitBattleOutcome(DeploymentId, true, Survivors, 1, 23);

	TestEqual(TEXT("win advances cycle"), Reloaded.Campaign->GetCycleIndex(), CycleBeforeWin + 1);
	TestEqual(TEXT("win pays reward"), Reloaded.Campaign->GetCredits(), CreditsBeforeWin + WinContract.RewardCredits);
	TestEqual(TEXT("win writes survivor count"), Reloaded.Campaign->GetPlayerLegions()[0].AliveSoldiers, SurvivorCountBefore - 1);
	TestEqual(TEXT("win increments survivor battle count"), Reloaded.Campaign->GetPlayerLegions()[0].BattlesSurvived, SurvivorBattlesBefore + 1);
	TestTrue(TEXT("win outcome valid"), Reloaded.Campaign->GetLastOutcome().bValid);
	TestTrue(TEXT("win outcome victory"), Reloaded.Campaign->GetLastOutcome().bVictory);
	TestEqual(TEXT("win outcome reward"), Reloaded.Campaign->GetLastOutcome().CreditsEarned, WinContract.RewardCredits);
	TestEqual(TEXT("win outcome casualties"), Reloaded.Campaign->GetLastOutcome().PlayerCasualties, 1);

	float ControlSumAfterWin = 0.0f;
	for (const FFactionStatus& Faction : Reloaded.Campaign->GetFactions())
	{
		ControlSumAfterWin += Faction.GalacticControl;
	}
	TestTrue(TEXT("control sum remains normalized"), FMath::Abs(ControlSumAfterWin - 100.0f) <= 0.01f);
	const FFactionStatus* EmployerAfterWin = Reloaded.Campaign->FindFaction(WinContract.Employer);
	const FGalacticSector* SectorAfterWin = Reloaded.Campaign->FindSector(WinContract.TargetSectorId);
	TestTrue(TEXT("employer control changed"), EmployerAfterWin && !FMath::IsNearlyEqual(EmployerAfterWin->GalacticControl, EmployerControlBefore));
	TestEqual(TEXT("employer reputation advanced"), EmployerAfterWin ? EmployerAfterWin->Reputation : -999, EmployerRepBefore + WinContract.ReputationGain);
	TestTrue(TEXT("sector owner changed to employer"), SectorAfterWin && SectorAfterWin->ControllingFaction == WinContract.Employer);
	TestTrue(TEXT("sector intensity increased"), SectorAfterWin && SectorAfterWin->ConflictIntensity > SectorIntensityBefore);
	const int32 NextLegionAfterWin = Reloaded.Campaign->NextLegionId;
	const int32 NextContractAfterWin = Reloaded.Campaign->NextContractId;

	FCampaignTestInstance WinReload = CreateInstance();
	TestTrue(TEXT("reload after win"), WinReload.Campaign->LoadCampaign(false));
	TestEqual(TEXT("reloaded win credits"), WinReload.Campaign->GetCredits(), CreditsBeforeWin + WinContract.RewardCredits);
	TestEqual(TEXT("reloaded win cycle"), WinReload.Campaign->GetCycleIndex(), CycleBeforeWin + 1);
	TestEqual(TEXT("reloaded survivor count"), WinReload.Campaign->GetPlayerLegions()[0].AliveSoldiers, SurvivorCountBefore - 1);
	TestEqual(TEXT("reloaded next legion id"), WinReload.Campaign->NextLegionId, NextLegionAfterWin);
	TestEqual(TEXT("reloaded next contract id"), WinReload.Campaign->NextContractId, NextContractAfterWin);
	TestTrue(TEXT("reloaded win outcome valid"), WinReload.Campaign->GetLastOutcome().bValid);
	const FFactionStatus* RestoredEmployer = WinReload.Campaign->FindFaction(WinContract.Employer);
	const FGalacticSector* RestoredSector = WinReload.Campaign->FindSector(WinContract.TargetSectorId);
	TestTrue(TEXT("win faction control and reputation persisted"), RestoredEmployer && EmployerAfterWin
		&& RestoredEmployer->GalacticControl == EmployerAfterWin->GalacticControl
		&& RestoredEmployer->Reputation == EmployerAfterWin->Reputation);
	TestTrue(TEXT("win sector ownership and intensity persisted"), RestoredSector && SectorAfterWin
		&& RestoredSector->ControllingFaction == SectorAfterWin->ControllingFaction
		&& RestoredSector->ConflictIntensity == SectorAfterWin->ConflictIntensity);

	// ---- 同一 ContractId 重复结算不得重复发奖 ----
	const int32 CreditsAfterWinReload = WinReload.Campaign->GetCredits();
	const int32 CycleAfterWinReload = WinReload.Campaign->GetCycleIndex();
	WinReload.Campaign->CommitBattleOutcome(DeploymentId, true, Survivors, 1, 23);
	TestEqual(TEXT("repeated commit does not reward again"), WinReload.Campaign->GetCredits(), CreditsAfterWinReload);
	TestEqual(TEXT("repeated commit does not advance cycle"), WinReload.Campaign->GetCycleIndex(), CycleAfterWinReload);

	// ---- 后续败北: 仅支付 1/5 撤离保障金并写出败北战报 ----
	TestEqual(TEXT("new offers after win"), WinReload.Campaign->GetAvailableContracts().Num(), 3);
	const FGalacticContract DefeatContract = WinReload.Campaign->GetAvailableContracts()[0];
	TestTrue(TEXT("begin defeat contract"), WinReload.Campaign->BeginContract(DefeatContract.ContractId));
	WinReload.Campaign->CommitBattleOutcome(DefeatContract.ContractId, false, Survivors, 0, 3);
	TestEqual(TEXT("defeat pays one fifth"), WinReload.Campaign->GetCredits(), CreditsAfterWinReload + DefeatContract.RewardCredits / 5);
	TestTrue(TEXT("defeat outcome valid"), WinReload.Campaign->GetLastOutcome().bValid);
	TestFalse(TEXT("defeat outcome is not victory"), WinReload.Campaign->GetLastOutcome().bVictory);
	TestEqual(TEXT("defeat outcome reward"), WinReload.Campaign->GetLastOutcome().CreditsEarned, DefeatContract.RewardCredits / 5);

	FCampaignTestInstance DefeatReload = CreateInstance();
	TestTrue(TEXT("reload after defeat"), DefeatReload.Campaign->LoadCampaign(false));
	TestTrue(TEXT("reloaded defeat outcome valid"), DefeatReload.Campaign->GetLastOutcome().bValid);
	TestFalse(TEXT("reloaded defeat outcome is not victory"), DefeatReload.Campaign->GetLastOutcome().bVictory);
	TestEqual(TEXT("reloaded defeat balance"), DefeatReload.Campaign->GetCredits(), CreditsAfterWinReload + DefeatContract.RewardCredits / 5);

	// ---- 最新槽损坏: 新实例回退较旧世代并进入 Recovered ----
	const FString SlotA = Prefix + TEXT("_A");
	const FString SlotB = Prefix + TEXT("_B");
	ERGalacticSave::FReadResult ReadA = ERGalacticSave::ReadSlot(SlotA);
	ERGalacticSave::FReadResult ReadB = ERGalacticSave::ReadSlot(SlotB);
	TestTrue(TEXT("both generations exist before corruption"), ReadA.State == ERGalacticSave::ESlotState::Valid && ReadB.State == ERGalacticSave::ESlotState::Valid);
	const bool bAIsLatest = ReadA.Generation >= ReadB.Generation;
	const FString LatestSlot = bAIsLatest ? SlotA : SlotB;
	const int64 OlderGeneration = bAIsLatest ? ReadB.Generation : ReadA.Generation;
	TestTrue(TEXT("truncate latest slot"), UGameplayStatics::SaveDataToSlot(TArray<uint8>{ 0x01 }, LatestSlot, 0));

	FCampaignTestInstance Recovered = CreateInstance();
	TestTrue(TEXT("recovered instance loads"), Recovered.Campaign->LoadCampaign(false));
	TestEqual(TEXT("recovered status"), (int32)Recovered.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::Recovered);
	TestEqual(TEXT("recovered older generation"), Recovered.Campaign->LastSavedGeneration, OlderGeneration);
	TestEqual(TEXT("recovered wallet rolled back"), Recovered.Campaign->GetCredits(), CreditsAfterWinReload);
	TestEqual(TEXT("recovered cycle rolled back"), Recovered.Campaign->GetCycleIndex(), CycleAfterWinReload);

	// ---- 双槽皆坏: LoadFailed; 重试/招募/出击均不得覆盖原文件 ----
	TestTrue(TEXT("truncate remaining slot"), UGameplayStatics::SaveDataToSlot(TArray<uint8>{ 0x01 }, bAIsLatest ? SlotB : SlotA, 0));
	FCampaignTestInstance Blocked = CreateInstance();
	TestFalse(TEXT("both-corrupt load fails"), Blocked.Campaign->LoadCampaign(false));
	TestEqual(TEXT("both-corrupt status"), (int32)Blocked.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::LoadFailed);
	TestFalse(TEXT("both-corrupt campaign unavailable"), Blocked.Campaign->IsCampaignAvailable());
	TestFalse(TEXT("load retry does not resurrect missing data"), Blocked.Campaign->RetryCampaignSave());
	TestEqual(TEXT("retry remains load failed"), (int32)Blocked.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::LoadFailed);
	TestFalse(TEXT("recruit blocked while load failed"), Blocked.Campaign->RecruitLegion(ELegionClass::Pikeman));
	TestFalse(TEXT("begin contract blocked while load failed"), Blocked.Campaign->BeginContract(1));
	TArray<uint8> CorruptA;
	TArray<uint8> CorruptB;
	TestTrue(TEXT("corrupt A bytes retained"), UGameplayStatics::LoadDataFromSlot(CorruptA, SlotA, 0) && CorruptA.Num() == 1);
	TestTrue(TEXT("corrupt B bytes retained"), UGameplayStatics::LoadDataFromSlot(CorruptB, SlotB, 0) && CorruptB.Num() == 1);

	// ---- 未来版本页脚: 即使存在较旧好档也拒绝使用/覆盖, 保留原档等待新版本 ----
	DeleteCampaignTestSlots(Prefix);
	FCampaignTestInstance Versioned = CreateInstance();
	TestTrue(TEXT("version-test initial load"), Versioned.Campaign->LoadCampaign(true));
	Versioned.Campaign->RefreshContracts();
	ReadA = ERGalacticSave::ReadSlot(SlotA);
	ReadB = ERGalacticSave::ReadSlot(SlotB);
	TestTrue(TEXT("version-test has two generations"), ReadA.State == ERGalacticSave::ESlotState::Valid && ReadB.State == ERGalacticSave::ESlotState::Valid);
	const bool bAIsNewestValid = ReadA.Generation >= ReadB.Generation;
	const FString NewestSlot = bAIsNewestValid ? SlotA : SlotB;
	const FString OlderSlot = bAIsNewestValid ? SlotB : SlotA;
	const int64 OlderValidGeneration = bAIsNewestValid ? ReadB.Generation : ReadA.Generation;

	TArray<uint8> FutureBytes;
	TestTrue(TEXT("read newest bytes"), UGameplayStatics::LoadDataFromSlot(FutureBytes, NewestSlot, 0));
	constexpr int32 FooterSizeForTest = 3 * sizeof(uint32) + sizeof(int64);
	TestTrue(TEXT("future payload has footer"), FutureBytes.Num() > FooterSizeForTest);
	if (FutureBytes.Num() > FooterSizeForTest)
	{
		uint32 FutureVersion = ERGalacticSave::CurrentVersion + 1;
		FMemoryWriter VersionWriter(FutureBytes, true);
		VersionWriter.Seek(FutureBytes.Num() - FooterSizeForTest + sizeof(uint32));
		VersionWriter << FutureVersion;
		const uint32 FixedChecksum = FCrc::MemCrc32(FutureBytes.GetData(), FutureBytes.Num() - sizeof(uint32));
		uint32 ChecksumToWrite = FixedChecksum;
		FMemoryWriter ChecksumWriter(FutureBytes, true);
		ChecksumWriter.Seek(FutureBytes.Num() - sizeof(uint32));
		ChecksumWriter << ChecksumToWrite;
		TestTrue(TEXT("write future footer"), UGameplayStatics::SaveDataToSlot(FutureBytes, NewestSlot, 0));
	}

	FCampaignTestInstance FutureBlocked = CreateInstance();
	TestFalse(TEXT("future generation is incompatible"), FutureBlocked.Campaign->LoadCampaign(false));
	TestEqual(TEXT("future status incompatible"), (int32)FutureBlocked.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::Incompatible);
	TestFalse(TEXT("future campaign unavailable"), FutureBlocked.Campaign->IsCampaignAvailable());
	TestFalse(TEXT("future retry refused"), FutureBlocked.Campaign->RetryCampaignSave());
	TestFalse(TEXT("future recruit refused"), FutureBlocked.Campaign->RecruitLegion(ELegionClass::Pikeman));
	TestFalse(TEXT("future deploy refused"), FutureBlocked.Campaign->BeginContract(1));
	TestFalse(TEXT("future continue refused"), FutureBlocked.Campaign->CanContinueCampaign());
	TestFalse(TEXT("future new game refused"), FutureBlocked.Campaign->StartNewCampaign(2026));
	TestFalse(TEXT("future manual slot load refused"), FutureBlocked.Campaign->LoadCampaignFromSlot(0));
	const ERGalacticSave::FReadResult OlderAfterFuture = ERGalacticSave::ReadSlot(OlderSlot);
	TestEqual(TEXT("older good save retained"), (int32)OlderAfterFuture.State, (int32)ERGalacticSave::ESlotState::Valid);
	TestEqual(TEXT("older good generation retained"), OlderAfterFuture.Generation, OlderValidGeneration);
	TestEqual(TEXT("future generation zeroed to prevent overwrite"), FutureBlocked.Campaign->LastSavedGeneration, (int64)0);

	// ---- 写入失败: 保留 dirty/WriteFailed, 拒绝出击; 恢复槽位后重试成功并持久化 ----
	DeleteCampaignTestSlots(Prefix);
	FCampaignTestInstance WriteFailure = CreateInstance();
	TestTrue(TEXT("write-test initial load"), WriteFailure.Campaign->LoadCampaign(true));
	const int64 GenerationBeforeFailure = WriteFailure.Campaign->LastSavedGeneration;
	const FString ValidPrefix = Prefix;
#if PLATFORM_WINDOWS
	WriteFailure.Campaign->SaveSlotPrefix = ValidPrefix + TEXT("_Invalid?");
#else
	WriteFailure.Campaign->SaveSlotPrefix = ValidPrefix + TEXT("_Invalid/Path");
#endif
	const int32 FirstOfferBeforeFailure = WriteFailure.Campaign->GetAvailableContracts()[0].ContractId;
	WriteFailure.Campaign->RefreshContracts();
	TestEqual(TEXT("write failure status"), (int32)WriteFailure.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::WriteFailed);
	TestTrue(TEXT("write failure remains dirty"), WriteFailure.Campaign->bSaveDirty);
	TestEqual(TEXT("write failure does not advance durable generation"), WriteFailure.Campaign->LastSavedGeneration, GenerationBeforeFailure);
	const int32 FirstOfferAfterFailure = WriteFailure.Campaign->GetAvailableContracts()[0].ContractId;
	TestTrue(TEXT("write failure refreshed offer in memory"), FirstOfferAfterFailure != FirstOfferBeforeFailure);
	TestFalse(TEXT("dirty write failure blocks deployment"), WriteFailure.Campaign->BeginContract(FirstOfferAfterFailure));
	TestFalse(TEXT("blocked deployment does not activate contract"), WriteFailure.Campaign->HasActiveContract());

	WriteFailure.Campaign->SaveSlotPrefix = ValidPrefix;
	const int32 NonceBeforeRetry = WriteFailure.Campaign->ContractRefreshNonce;
	TestTrue(TEXT("retry after restoring prefix"), WriteFailure.Campaign->RetryCampaignSave());
	TestEqual(TEXT("retry status saved"), (int32)WriteFailure.Campaign->GetSaveStatus(), (int32)EGalacticCampaignSaveStatus::Saved);
	TestFalse(TEXT("retry clears dirty"), WriteFailure.Campaign->bSaveDirty);
	TestTrue(TEXT("retry advances generation"), WriteFailure.Campaign->LastSavedGeneration > GenerationBeforeFailure);

	FCampaignTestInstance RetryReload = CreateInstance();
	TestTrue(TEXT("reload after write retry"), RetryReload.Campaign->LoadCampaign(false));
	TestEqual(TEXT("retried nonce persisted"), RetryReload.Campaign->ContractRefreshNonce, NonceBeforeRetry);
	TestTrue(TEXT("retried offer persisted"), RetryReload.Campaign->FindContract(FirstOfferAfterFailure) != nullptr);

	// ---- 标题菜单 API: 无档不可继续; 新游戏激活会话; A/B 检查点按世代排序且可显式回滚 ----
	DeleteCampaignTestSlots(Prefix);
	FCampaignTestInstance Menu = CreateInstance();
	TestFalse(TEXT("menu continue unavailable without slots"), Menu.Campaign->CanContinueCampaign());
	TestFalse(TEXT("menu continue fails without slots"), Menu.Campaign->ContinueLatestCampaign());
	TestTrue(TEXT("menu new game succeeds"), Menu.Campaign->StartNewCampaign(2026));
	TestTrue(TEXT("menu new game activates session"), Menu.Campaign->IsCampaignSessionActive());
	TestTrue(TEXT("menu continue available after new game"), Menu.Campaign->CanContinueCampaign());
	const int32 InitialMenuNonce = Menu.Campaign->ContractRefreshNonce;
	Menu.Campaign->RefreshContracts();
	const TArray<FGalacticCampaignSaveSlotInfo> MenuSlots = Menu.Campaign->GetSaveSlotInfos();
	TestEqual(TEXT("menu exposes two checkpoint entries"), MenuSlots.Num(), 2);
	TestTrue(TEXT("menu checkpoints sorted newest first"), MenuSlots.Num() == 2 && MenuSlots[0].Generation >= MenuSlots[1].Generation);
	TestTrue(TEXT("menu checkpoints valid"), MenuSlots.Num() == 2 && MenuSlots[0].bValid && MenuSlots[1].bValid);
	if (MenuSlots.Num() == 2)
	{
		TestTrue(TEXT("menu load older checkpoint"), Menu.Campaign->LoadCampaignFromSlot(MenuSlots[1].SlotIndex));
		TestEqual(TEXT("menu rollback restores older nonce"), Menu.Campaign->ContractRefreshNonce, InitialMenuNonce);
		TestTrue(TEXT("menu rollback activates session"), Menu.Campaign->IsCampaignSessionActive());
		TestTrue(TEXT("menu continue after rollback"), Menu.Campaign->ContinueLatestCampaign());
	}

	DeleteCampaignTestSlots(Prefix);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
