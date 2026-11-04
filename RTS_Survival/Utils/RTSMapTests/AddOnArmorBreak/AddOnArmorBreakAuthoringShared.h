// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

// Helpers shared by the editor-only authoring commands of the add-on armour break test (test content only).

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorBreakTestTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AddOnArmorBreakAuthoringShared
{
	using namespace AddOnArmorBreakTest;

	inline bool LoadShardCentroids(TArray<FVector>& OutCentroids)
	{
		const FString LayoutPath = FPaths::ProjectDir() / LayoutFile;
		FString LayoutText;
		TSharedPtr<FJsonObject> Layout;
		if (not FFileHelper::LoadFileToString(LayoutText, *LayoutPath)
			|| not FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LayoutText), Layout) || not Layout.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING cannot read %s"), *LayoutPath);
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Shard : Layout->GetArrayField(TEXT("shards")))
		{
			const TArray<TSharedPtr<FJsonValue>>& Centroid = Shard->AsObject()->GetArrayField(TEXT("centroid"));
			OutCentroids.Add(FVector(Centroid[0]->AsNumber(), Centroid[1]->AsNumber(), Centroid[2]->AsNumber()));
		}
		return OutCentroids.Num() == ShardCount;
	}

	inline bool LoadShardMeshes(TArray<UStaticMesh*>& OutMeshes)
	{
		for (int32 ShardIndex = 0; ShardIndex < ShardCount; ++ShardIndex)
		{
			UStaticMesh* const Mesh = LoadObject<UStaticMesh>(nullptr, *GetShardMeshPath(ShardIndex));
			if (not IsValid(Mesh))
			{
				UE_LOG(LogTemp, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING missing shard mesh %d"), ShardIndex);
				return false;
			}
			OutMeshes.Add(Mesh);
		}
		return true;
	}

	/** @return A fresh package for the asset; an existing asset of that name is moved out to the transient package. */
	inline UPackage* PrepareFreshPackage(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetShortName(PackageName);
		const bool bExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* const Package = bExists ? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		if (not IsValid(Package))
		{
			return nullptr;
		}
		Package->FullyLoad();
		if (UObject* const Existing = StaticFindObject(UObject::StaticClass(), Package, *AssetName))
		{
			Existing->ClearFlags(RF_Public | RF_Standalone);
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Existing->MarkAsGarbage();
		}
		return Package;
	}

	inline bool SaveAsset(UObject& Asset)
	{
		UPackage* const Package = Asset.GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                                                 FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *FileName, SaveArgs);
	}
}

#endif // WITH_EDITOR
