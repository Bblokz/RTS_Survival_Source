// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace
{
	const FString WorldCampaignSaveSlotName = TEXT("WorldCampaignState");
	const FString SaveGameExtension = TEXT(".sav");
	const FString BackupDirectoryName = TEXT("backup");
	constexpr int32 WorldCampaignUserIndex = 0;
}

UWorldStateAndSaveManager::UWorldStateAndSaveManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldStateAndSaveManager::CacheCurrentWorldState(const AGeneratorWorldCampaign& WorldGenerator)
{
	M_WorldCampaignState = WorldGenerator.BuildWorldCampaignStateFromCurrentGeneration();
}

void UWorldStateAndSaveManager::CachePlayerProfileSaveData(const FPlayerProfileSaveData& PlayerProfileSaveData)
{
	M_PlayerProfileSaveData = PlayerProfileSaveData;
}

bool UWorldStateAndSaveManager::SaveCampaignState()
{
	const FPlayerProfileSaveData PlayerSaveState = AggregatePlayerSaveState();
	const FWorldCampaignState WorldCampaignState = AggregateWorldCampaignState();

	if (not SaveMainCampaignSlot(WorldCampaignState, PlayerSaveState))
	{
		return false;
	}

	return SaveBackupCampaignSlot();
}

bool UWorldStateAndSaveManager::LoadCampaignState(FWorldCampaignState& OutWorldCampaignState,
                                                  FPlayerProfileSaveData& OutPlayerProfileSaveData)
{
	USaveGame* LoadedSaveGame = UGameplayStatics::LoadGameFromSlot(WorldCampaignSaveSlotName, WorldCampaignUserIndex);
	USaveWorldCampaign* LoadedCampaignSave = Cast<USaveWorldCampaign>(LoadedSaveGame);
	if (not IsValid(LoadedCampaignSave))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to load world campaign save payload."));
		return false;
	}

	if (not LoadedCampaignSave->WorldCampaignState.GetIsValidForRestore())
	{
		RTSFunctionLibrary::ReportError(TEXT("World campaign save payload has no anchors to restore."));
		return false;
	}

	M_WorldCampaignState = LoadedCampaignSave->WorldCampaignState;
	M_PlayerProfileSaveData = LoadedCampaignSave->PlayerProfileSaveData;
	OutWorldCampaignState = M_WorldCampaignState;
	OutPlayerProfileSaveData = M_PlayerProfileSaveData;
	return true;
}

FWorldCampaignState UWorldStateAndSaveManager::AggregateWorldCampaignState() const
{
	return M_WorldCampaignState;
}

FPlayerProfileSaveData UWorldStateAndSaveManager::AggregatePlayerSaveState() const
{
	return M_PlayerProfileSaveData;
}

bool UWorldStateAndSaveManager::SaveMainCampaignSlot(const FWorldCampaignState& WorldCampaignState,
                                                     const FPlayerProfileSaveData& PlayerProfileSaveData) const
{
	USaveWorldCampaign* SavePayload = CreateSavePayload(WorldCampaignState, PlayerProfileSaveData);
	if (not IsValid(SavePayload))
	{
		return false;
	}

	const bool bDidSave = UGameplayStatics::SaveGameToSlot(SavePayload, WorldCampaignSaveSlotName,
	                                                     WorldCampaignUserIndex);
	if (not bDidSave)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to save world campaign state."));
		return false;
	}

	return true;
}

bool UWorldStateAndSaveManager::SaveBackupCampaignSlot() const
{
	if (not EnsureBackupDirectoryExists())
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString MainSaveFilePath = GetMainSaveFilePath();
	const FString BackupSaveFilePath = GetBackupSaveFilePath();
	if (not PlatformFile.FileExists(*MainSaveFilePath))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot create world campaign backup because main save file does not exist."));
		return false;
	}

	const bool bDidCopyBackup = PlatformFile.CopyFile(*BackupSaveFilePath, *MainSaveFilePath);
	if (not bDidCopyBackup)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create world campaign backup save."));
		return false;
	}

	return true;
}

USaveWorldCampaign* UWorldStateAndSaveManager::CreateSavePayload(
	const FWorldCampaignState& WorldCampaignState,
	const FPlayerProfileSaveData& PlayerProfileSaveData) const
{
	USaveWorldCampaign* SavePayload = Cast<USaveWorldCampaign>(
		UGameplayStatics::CreateSaveGameObject(USaveWorldCampaign::StaticClass()));
	if (not IsValid(SavePayload))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create world campaign save payload."));
		return nullptr;
	}

	SavePayload->WorldCampaignState = WorldCampaignState;
	SavePayload->PlayerProfileSaveData = PlayerProfileSaveData;
	return SavePayload;
}

FString UWorldStateAndSaveManager::GetMainSaveFilePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), WorldCampaignSaveSlotName + SaveGameExtension);
}

FString UWorldStateAndSaveManager::GetBackupSaveFilePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), BackupDirectoryName,
	                       WorldCampaignSaveSlotName + SaveGameExtension);
}

bool UWorldStateAndSaveManager::EnsureBackupDirectoryExists() const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString BackupDirectoryPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), BackupDirectoryName);
	if (PlatformFile.DirectoryExists(*BackupDirectoryPath))
	{
		return true;
	}

	const bool bDidCreateDirectory = PlatformFile.CreateDirectoryTree(*BackupDirectoryPath);
	if (not bDidCreateDirectory)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create world campaign backup save directory."));
		return false;
	}

	return true;
}
