// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Campaign/GalacticCampaignTypes.h"
#include "GalacticCampaignSaveGame.generated.h"

USTRUCT()
struct FGalacticSavedFaction
{
	GENERATED_BODY()

	UPROPERTY()
	EFactionId Faction = EFactionId::Empire;

	UPROPERTY()
	int32 Reputation = 0;

	UPROPERTY()
	float GalacticControl = 0.0f;
};

USTRUCT()
struct FGalacticSavedSector
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SectorId = INDEX_NONE;

	UPROPERTY()
	EFactionId Owner = EFactionId::Empire;

	UPROPERTY()
	float ConflictIntensity = 0.0f;
};

/** 原生银河战役快照: 仅持久化动态战役状态, 不保存 UI 元数据/静态文案/世界对象/机器设置。 */
UCLASS()
class EBBINGRAINBOW_API UGalacticCampaignSaveGame final : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 CampaignSeed = 0;

	UPROPERTY()
	int32 GalaxyCoins = 0;

	UPROPERTY()
	int32 CycleIndex = 0;

	UPROPERTY()
	int32 NextLegionId = 0;

	UPROPERTY()
	int32 NextContractId = 0;

	UPROPERTY()
	int32 ContractRefreshNonce = 0;

	UPROPERTY()
	TArray<FGalacticSavedFaction> Factions;

	UPROPERTY()
	TArray<FGalacticSavedSector> Sectors;

	UPROPERTY()
	TArray<FManagedLegionData> PlayerLegions;

	UPROPERTY()
	TArray<FGalacticContract> AvailableContracts;

	UPROPERTY()
	FLegionBattleOutcome LastOutcome;
};

namespace ERGalacticSave
{
	inline constexpr uint32 CurrentVersion = 1;

	enum class ESlotState : uint8
	{
		Missing,
		Damaged,
		NewerVersion,
		Valid
	};

	struct FReadResult
	{
		ESlotState State = ESlotState::Missing;
		int64 Generation = 0;
		UGalacticCampaignSaveGame* Snapshot = nullptr;
	};

	FReadResult ReadSlot(const FString& SlotName);
	bool WriteSlot(const FString& SlotName, UGalacticCampaignSaveGame& Snapshot, int64 Generation);
}
