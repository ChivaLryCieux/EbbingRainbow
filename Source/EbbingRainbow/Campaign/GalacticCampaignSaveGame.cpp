// Copyright Epic Games, Inc. All Rights Reserved.

#include "Campaign/GalacticCampaignSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace ERGalacticSave
{
	namespace Private
	{
		constexpr uint32 FooterMagic = 0x45524353;
		constexpr int32 FooterSize = 3 * sizeof(uint32) + sizeof(int64);
		constexpr int64 MaxSaveBytes = 1024 * 1024;
	}

	FReadResult ReadSlot(const FString& SlotName)
	{
		FReadResult Result;
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) { return Result; }

		TArray<uint8> Data;
		if (!UGameplayStatics::LoadDataFromSlot(Data, SlotName, 0))
		{
			Result.State = ESlotState::Damaged;
			return Result;
		}

		if (Data.Num() <= Private::FooterSize || Data.Num() > Private::MaxSaveBytes)
		{
			Result.State = ESlotState::Damaged;
			return Result;
		}

		FMemoryReader Reader(Data, true);
		Reader.Seek(Data.Num() - Private::FooterSize);
		uint32 Magic = 0;
		uint32 Version = 0;
		int64 Generation = 0;
		uint32 Checksum = 0;
		Reader << Magic << Version << Generation << Checksum;

		const uint32 ExpectedChecksum = FCrc::MemCrc32(Data.GetData(), Data.Num() - sizeof(uint32));
		if (Reader.IsError() || Magic != Private::FooterMagic || Version == 0 || Generation <= 0 || Checksum != ExpectedChecksum)
		{
			Result.State = ESlotState::Damaged;
			return Result;
		}

		Result.Generation = Generation;
		if (Version > CurrentVersion)
		{
			Result.State = ESlotState::NewerVersion;
			return Result;
		}

		Data.SetNum(Data.Num() - Private::FooterSize, EAllowShrinking::Yes);

		USaveGame* Loaded = UGameplayStatics::LoadGameFromMemory(Data);
		Result.Snapshot = Cast<UGalacticCampaignSaveGame>(Loaded);
		if (!Result.Snapshot)
		{
			Result.State = ESlotState::Damaged;
			Result.Generation = 0;
			return Result;
		}

		Result.State = ESlotState::Valid;
		return Result;
	}

	bool WriteSlot(const FString& SlotName, UGalacticCampaignSaveGame& Snapshot, int64 Generation)
	{
		if (Generation <= 0) { return false; }

		TArray<uint8> Data;
		if (!UGameplayStatics::SaveGameToMemory(&Snapshot, Data) || Data.Num() <= 0)
		{
			return false;
		}

		FMemoryWriter Writer(Data, true);
		Writer.Seek(Data.Num());
		uint32 Magic = Private::FooterMagic;
		uint32 Version = CurrentVersion;
		Writer << Magic << Version << Generation;
		uint32 Checksum = FCrc::MemCrc32(Data.GetData(), Data.Num());
		Writer << Checksum;

		if (Writer.IsError() || Data.Num() <= Private::FooterSize || Data.Num() > Private::MaxSaveBytes)
		{
			return false;
		}
		return UGameplayStatics::SaveDataToSlot(Data, SlotName, 0);
	}
}
