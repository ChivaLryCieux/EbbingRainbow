// Copyright Epic Games, Inc. All Rights Reserved.

#include "WFC3DModuleLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"

UWFC3DModuleLibrary::UWFC3DModuleLibrary()
{
}

int32 UWFC3DModuleLibrary::ScanDirectoryForStaticMeshes(
	const FString& DirectoryPath,
	FName DefaultHorizontalSocket,
	FName DefaultVerticalSocket,
	bool bAutoRotations,
	bool bClearExisting)
{
	if (bClearExisting)
	{
		Modules.Empty();
	}

	if (DirectoryPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC] ScanDirectoryForStaticMeshes: DirectoryPath is empty!"));
		return 0;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(*DirectoryPath);
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	int32 AddedCount = 0;
	for (const FAssetData& AssetData : AssetDataList)
	{
		UStaticMesh* LoadedMesh = Cast<UStaticMesh>(AssetData.GetAsset());
		if (!LoadedMesh)
		{
			continue;
		}

		// 检查是否已经存在该 Mesh
		bool bAlreadyExists = false;
		for (const FWFC3DModuleDefinition& ExistingDef : Modules)
		{
			if (ExistingDef.Mesh == LoadedMesh)
			{
				bAlreadyExists = true;
				break;
			}
		}

		if (bAlreadyExists)
		{
			continue;
		}

		FWFC3DModuleDefinition NewDef;
		NewDef.ModuleId = FName(*LoadedMesh->GetName());
		NewDef.Mesh = LoadedMesh;
		NewDef.Weight = 1.0f;
		NewDef.bAutoGenerateRotations = bAutoRotations;

		// 默认插槽配置
		NewDef.SetSocket(EWFC3DDirection::PosX, DefaultHorizontalSocket);
		NewDef.SetSocket(EWFC3DDirection::NegX, DefaultHorizontalSocket);
		NewDef.SetSocket(EWFC3DDirection::PosY, DefaultHorizontalSocket);
		NewDef.SetSocket(EWFC3DDirection::NegY, DefaultHorizontalSocket);
		NewDef.SetSocket(EWFC3DDirection::PosZ, DefaultVerticalSocket);
		NewDef.SetSocket(EWFC3DDirection::NegZ, DefaultVerticalSocket);

		Modules.Add(NewDef);
		AddedCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("[WFC] Scanned directory '%s': added %d meshes. Total modules: %d"), *DirectoryPath, AddedCount, Modules.Num());
	return AddedCount;
}

namespace
{
	void RotateSocketsYaw90(const FName InSockets[6], FName OutSockets[6])
	{
		OutSockets[0] = InSockets[3]; // New PosX from old NegY
		OutSockets[1] = InSockets[2]; // New NegX from old PosY
		OutSockets[2] = InSockets[0]; // New PosY from old PosX
		OutSockets[3] = InSockets[1]; // New NegY from old NegX
		OutSockets[4] = InSockets[4]; // PosZ unchanged
		OutSockets[5] = InSockets[5]; // NegZ unchanged
	}
}

bool UWFC3DModuleLibrary::AreSocketsCompatible(FName SocketA, FName SocketB) const
{
	if (SocketA.IsNone() && SocketB.IsNone())
	{
		return true;
	}

	if (CustomCompatibilityPairs.Num() > 0)
	{
		for (const FWFC3DSocketCompatibilityPair& Pair : CustomCompatibilityPairs)
		{
			if (Pair.SocketA == SocketA && Pair.SocketB == SocketB)
			{
				return true;
			}
			if (Pair.bBidirectional && Pair.SocketA == SocketB && Pair.SocketB == SocketA)
			{
				return true;
			}
		}
		return false;
	}

	return (SocketA == SocketB);
}

bool UWFC3DModuleLibrary::CompileLibrary(
	TArray<FCompiledWFC3DModule>& OutCompiledModules,
	TArray<TArray<TBitArray<>>>& OutCompatibilityTable) const
{
	OutCompiledModules.Empty();
	OutCompatibilityTable.Empty();

	// 1. 展开用户配置模块
	for (const FWFC3DModuleDefinition& Def : Modules)
	{
		// 基础 0 度变体
		FCompiledWFC3DModule BaseMod;
		BaseMod.SourceModuleId = Def.ModuleId;
		BaseMod.VariantId = FName(*FString::Printf(TEXT("%s_Rot0"), *Def.ModuleId.ToString()));
		BaseMod.Mesh = Def.Mesh;
		BaseMod.LocalTransform = FTransform(Def.AdditionalRotation, Def.AdditionalOffset, Def.MeshScale);
		BaseMod.Weight = FMath::Max(0.01f, Def.Weight);
		for (int32 DirIdx = 0; DirIdx < 6; ++DirIdx)
		{
			BaseMod.Sockets[DirIdx] = Def.GetSocket(WFC3DUtils::IndexToDirection(DirIdx));
		}

		for (const FWFC3DMeshPart& Part : Def.AdditionalParts)
		{
			FCompiledWFC3DMeshPart CompiledPart;
			CompiledPart.Mesh = Part.Mesh;
			CompiledPart.LocalTransform = Part.RelativeTransform;
			BaseMod.AdditionalParts.Add(CompiledPart);
		}

		OutCompiledModules.Add(BaseMod);

		// 如果开启自动四向旋转，派生 90°, 180°, 270° 变体
		if (Def.bAutoGenerateRotations && (Def.Mesh != nullptr || Def.AdditionalParts.Num() > 0))
		{
			FName PrevSockets[6];
			FMemory::Memcpy(PrevSockets, BaseMod.Sockets, sizeof(PrevSockets));

			const float RotAngles[3] = { 90.0f, 180.0f, 270.0f };
			for (int32 RotIdx = 0; RotIdx < 3; ++RotIdx)
			{
				FCompiledWFC3DModule RotMod;
				RotMod.SourceModuleId = Def.ModuleId;
				RotMod.VariantId = FName(*FString::Printf(TEXT("%s_Rot%d"), *Def.ModuleId.ToString(), FMath::RoundToInt(RotAngles[RotIdx])));
				RotMod.Mesh = Def.Mesh;
				
				FRotator CombinedRot = Def.AdditionalRotation;
				CombinedRot.Yaw += RotAngles[RotIdx];
				RotMod.LocalTransform = FTransform(CombinedRot, Def.AdditionalOffset, Def.MeshScale);
				RotMod.Weight = FMath::Max(0.01f, Def.Weight);

				const FRotator YawRot(0.0f, RotAngles[RotIdx], 0.0f);
				for (const FWFC3DMeshPart& Part : Def.AdditionalParts)
				{
					FCompiledWFC3DMeshPart CompiledPart;
					CompiledPart.Mesh = Part.Mesh;
					const FVector RotatedLoc = YawRot.RotateVector(Part.RelativeTransform.GetLocation());
					const FRotator RotatedRot = (YawRot.Quaternion() * Part.RelativeTransform.GetRotation()).Rotator();
					CompiledPart.LocalTransform = FTransform(RotatedRot, RotatedLoc, Part.RelativeTransform.GetScale3D());
					RotMod.AdditionalParts.Add(CompiledPart);
				}

				RotateSocketsYaw90(PrevSockets, RotMod.Sockets);
				FMemory::Memcpy(PrevSockets, RotMod.Sockets, sizeof(PrevSockets));

				OutCompiledModules.Add(RotMod);
			}
		}
	}

	// 2. 注入空气 (空单元格) 模块
	if (bIncludeAirModule)
	{
		FCompiledWFC3DModule AirMod;
		AirMod.SourceModuleId = FName(TEXT("Air"));
		AirMod.VariantId = FName(TEXT("Air"));
		AirMod.Mesh = nullptr;
		AirMod.LocalTransform = FTransform::Identity;
		AirMod.Weight = FMath::Max(0.01f, AirModuleWeight);
		for (int32 i = 0; i < 6; ++i)
		{
			AirMod.Sockets[i] = AirSocketName;
		}
		OutCompiledModules.Add(AirMod);
	}

	const int32 TotalModules = OutCompiledModules.Num();
	if (TotalModules == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WFC] CompileLibrary: No modules defined in library!"));
		return false;
	}

	// 3. 构建快速查找相容矩阵: [ModA][Dir] -> BitArray(合法 ModB)
	OutCompatibilityTable.SetNum(TotalModules);
	for (int32 ModAIdx = 0; ModAIdx < TotalModules; ++ModAIdx)
	{
		OutCompatibilityTable[ModAIdx].SetNum(6);
		for (int32 DirIdx = 0; DirIdx < 6; ++DirIdx)
		{
			EWFC3DDirection Dir = WFC3DUtils::IndexToDirection(DirIdx);
			EWFC3DDirection OppDir = WFC3DUtils::GetOppositeDirection(Dir);
			int32 OppDirIdx = WFC3DUtils::DirectionToIndex(OppDir);

			TBitArray<>& Bitmask = OutCompatibilityTable[ModAIdx][DirIdx];
			Bitmask.Init(false, TotalModules);

			FName SocketA = OutCompiledModules[ModAIdx].Sockets[DirIdx];

			for (int32 ModBIdx = 0; ModBIdx < TotalModules; ++ModBIdx)
			{
				FName SocketB = OutCompiledModules[ModBIdx].Sockets[OppDirIdx];
				if (AreSocketsCompatible(SocketA, SocketB))
				{
					Bitmask[ModBIdx] = true;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WFC] CompileLibrary: Successfully compiled %d variant modules and built adjacency matrix."), TotalModules);
	return true;
}
