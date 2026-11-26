// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "GalacticCampaignSubsystem.generated.h"

class UGalacticCampaignSaveGame;

/** 银河战役存档状态 (Volatile = 命令行/编辑器调试沙盘, 不写盘) */
enum class EGalacticCampaignSaveStatus : uint8
{
	Volatile,
	Saved,
	Recovered,
	WriteFailed,
	LoadFailed,
	Incompatible
};

/**
 * 虹膜基金会银河战役全局子系统 (跨 Hall / Lvl_Legion 关卡存活):
 *  - 维护基金会资金、玩家常备虹膜链接兵团编制、三大阵营声望与银河控制率、星区态势；
 *  - 生成并派发阵营委托合同 (FGalacticContract)，驱动 Lvl_Legion 战场配置；
 *  - 结算战果：更新兵团战损、发放经费、推动银河战局与星区控制权变迁；
 *  - 正常游戏世界以双槽位原生 .sav 持久化战役快照。
 */
UCLASS()
class EBBINGRAINBOW_API UGalacticCampaignSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Campaign")
	static UGalacticCampaignSubsystem* Get(const UObject* WorldContextObject);

	/** 内存沙盘重置 (不会替换已经落盘的战役) */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	void ResetCampaign(int32 Seed = 2026);

	// ---------------- 存档状态 ----------------

	EGalacticCampaignSaveStatus GetSaveStatus() const { return SaveStatus; }
	bool IsCampaignAvailable() const
	{
		return SaveStatus != EGalacticCampaignSaveStatus::LoadFailed
			&& SaveStatus != EGalacticCampaignSaveStatus::Incompatible;
	}
	FText GetSaveNotice() const;
	bool RetryCampaignSave();

	// ---------------- 查询 ----------------

	UFUNCTION(BlueprintPure, Category = "Campaign")
	int32 GetCredits() const { return Credits; }

	UFUNCTION(BlueprintPure, Category = "Campaign")
	int32 GetCycleIndex() const { return CycleIndex; }

	const TArray<FFactionStatus>& GetFactions() const { return Factions; }
	const FFactionStatus* FindFaction(EFactionId Faction) const;

	const TArray<FGalacticSector>& GetSectors() const { return Sectors; }
	const FGalacticSector* FindSector(int32 SectorId) const;

	const TArray<FManagedLegionData>& GetPlayerLegions() const { return PlayerLegions; }

	const TArray<FGalacticContract>& GetAvailableContracts() const { return AvailableContracts; }
	const FGalacticContract* FindContract(int32 ContractId) const;

	bool HasActiveContract() const { return bHasActiveContract; }
	const FGalacticContract& GetActiveContract() const { return ActiveContract; }
	const FLegionBattleOutcome& GetLastOutcome() const { return LastOutcome; }

	// ---------------- 委托与部署 ----------------

	/** 刷新下一批委托合同 */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	void RefreshContracts();

	/** 接取指定委托并跳转至大兵团战场 Lvl_Legion */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	bool AcceptAndDeployContract(int32 ContractId, const UObject* WorldContext);

	/** 战斗结算入口 (由 ALegionGameMode 战终调用, ContractId 必须与当前合同一致) */
	void CommitBattleOutcome(int32 ContractId, bool bVictory, const TMap<int32, int32>& SurvivingByLegionId,
		int32 TotalPlayerCasualties, int32 TotalEnemyEliminated);

	// ---------------- 军团整备 ----------------

	/** 补充单个兵团战损兵员 */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	bool ReinforceLegion(int32 LegionId);

	/** 一键补齐全部兵团兵员 */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	int32 ReinforceAllLegions();

	/** 提升兵团虹膜 AI 同步等级 */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	bool UpgradeLegionSync(int32 LegionId);

	/** 扩编新兵团 (上限 8 个兵团) */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	bool RecruitLegion(ELegionClass Archetype);

	int32 GetRecruitCost(ELegionClass Archetype) const;
	static constexpr int32 MaxPlayerLegions = 8;

	DECLARE_MULTICAST_DELEGATE(FOnCampaignUpdated);
	FOnCampaignUpdated OnCampaignUpdated;

private:
	friend class FGalacticCampaignSaveTest;

	void InitializeCampaignState(int32 Seed);
	void InitFactionsAndSectors();
	void InitStarterLegions();
	void GenerateContracts();
	bool BeginContract(int32 ContractId);
	bool LoadCampaign(bool bCreateIfMissing);
	bool SaveCampaign();
	bool IsValidSnapshot(const UGalacticCampaignSaveGame& Snapshot) const;
	void ApplySnapshot(const UGalacticCampaignSaveGame& Snapshot);
	void PublishCampaignChange();

	FManagedLegionData* FindPlayerLegionMutable(int32 LegionId);
	void ShiftGalacticControl(EFactionId Winner, EFactionId Loser, float Amount, int32 SectorId);
	void NormalizeControls();

	UPROPERTY()
	int32 CampaignSeed = 2026;

	UPROPERTY()
	int32 Credits = 1200;

	UPROPERTY()
	int32 CycleIndex = 1;

	UPROPERTY()
	int32 NextLegionId = 1;

	UPROPERTY()
	int32 NextContractId = 1;

	UPROPERTY()
	int32 ContractRefreshNonce = 0;

	UPROPERTY()
	TArray<FFactionStatus> Factions;

	UPROPERTY()
	TArray<FGalacticSector> Sectors;

	UPROPERTY()
	TArray<FManagedLegionData> PlayerLegions;

	UPROPERTY()
	TArray<FGalacticContract> AvailableContracts;

	UPROPERTY()
	bool bHasActiveContract = false;

	UPROPERTY()
	FGalacticContract ActiveContract;

	UPROPERTY()
	FLegionBattleOutcome LastOutcome;

	FString SaveSlotPrefix;
	EGalacticCampaignSaveStatus SaveStatus = EGalacticCampaignSaveStatus::Volatile;
	int32 LastSavedSlot = INDEX_NONE;
	int64 LastSavedGeneration = 0;
	bool bSaveDirty = false;
};
