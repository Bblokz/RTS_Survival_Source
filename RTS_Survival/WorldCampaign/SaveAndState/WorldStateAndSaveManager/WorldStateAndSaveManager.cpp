// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace
{
	const FString WorldCampaignSaveSlotName = TEXT("WorldCampaignState");
	const FString SaveGameExtension = TEXT(".sav");
	const FString BackupDirectoryName = TEXT("backup");
	constexpr int32 WorldCampaignUserIndex = 0;

	// Empty = use the real slot. Set by tests so they never overwrite the real campaign.
	FString WorldCampaignSaveSlotNameOverride;
}

bool UWorldStateAndSaveManager::GetDoesCampaignSaveExist()
{
	return UGameplayStatics::DoesSaveGameExist(GetCampaignSaveSlotName(), WorldCampaignUserIndex);
}

void UWorldStateAndSaveManager::SetCampaignSaveSlotNameOverride(const FString& SlotName)
{
	WorldCampaignSaveSlotNameOverride = SlotName;
}

FString UWorldStateAndSaveManager::GetCampaignSaveSlotName()
{
#if !UE_BUILD_SHIPPING
	// -RTSWorldCampaignSaveSlot=Name: headless checks of the world map write to their own slot.
	static bool bHasReadCommandLineSlot = false;
	if (not bHasReadCommandLineSlot)
	{
		bHasReadCommandLineSlot = true;
		FString CommandLineSlotName;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSWorldCampaignSaveSlot="), CommandLineSlotName)
			&& WorldCampaignSaveSlotNameOverride.IsEmpty())
		{
			WorldCampaignSaveSlotNameOverride = CommandLineSlotName;
		}
	}
#endif
	return WorldCampaignSaveSlotNameOverride.IsEmpty() ? WorldCampaignSaveSlotName : WorldCampaignSaveSlotNameOverride;
}

UWorldStateAndSaveManager::UWorldStateAndSaveManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldStateAndSaveManager::CacheCurrentWorldState(const AGeneratorWorldCampaign& WorldGenerator)
{
	const int32 CachedCurrentTurn = M_WorldCampaignState.CurrentTurn;
	const int32 CachedEnemyObjectProceduralMapIndex = M_WorldCampaignState.EnemyObjectProceduralMapIndex;
	const FWorldCampaignProgressState CachedProgress = M_WorldCampaignState.Progress;
	M_WorldCampaignState = WorldGenerator.BuildWorldCampaignStateFromCurrentGeneration();
	M_WorldCampaignState.Progress = CachedProgress;
	M_WorldCampaignState.CurrentTurn = CachedCurrentTurn;
	M_WorldCampaignState.EnemyObjectProceduralMapIndex = CachedEnemyObjectProceduralMapIndex;
}

void UWorldStateAndSaveManager::CachePlayerProfileSaveData(const FPlayerProfileSaveData& PlayerProfileSaveData)
{
	M_PlayerProfileSaveData = PlayerProfileSaveData;
}

void UWorldStateAndSaveManager::CachePlayerCardData(const FPlayerCardSaveData& PlayerCardData)
{
	M_PlayerProfileSaveData.PlayerData.CardData = PlayerCardData;
}

void UWorldStateAndSaveManager::CacheWorldDivisionSaveData(
	const TArray<FWorldDivisionSaveData>& WorldDivisionSaveData)
{
	M_WorldCampaignState.WorldDivisions = WorldDivisionSaveData;
}

bool UWorldStateAndSaveManager::SaveCampaignState()
{
	const FPlayerProfileSaveData PlayerSaveState = AggregatePlayerSaveState();
	const FWorldCampaignState WorldCampaignState = AggregateWorldCampaignState();
	if (not WorldCampaignState.GetIsValidForRestore())
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot save world campaign state because anchor keys are invalid."));
		return false;
	}

	if (not SaveMainCampaignSlot(WorldCampaignState, PlayerSaveState))
	{
		return false;
	}

	return SaveBackupCampaignSlot();
}

bool UWorldStateAndSaveManager::LoadCampaignState(FWorldCampaignState& OutWorldCampaignState,
                                                  FPlayerProfileSaveData& OutPlayerProfileSaveData)
{
	USaveGame* LoadedSaveGame = UGameplayStatics::LoadGameFromSlot(GetCampaignSaveSlotName(), WorldCampaignUserIndex);
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

int32 UWorldStateAndSaveManager::AdvanceCurrentTurn()
{
	M_WorldCampaignState.CurrentTurn++;
	return M_WorldCampaignState.CurrentTurn;
}

int32 UWorldStateAndSaveManager::GetCurrentTurn() const
{
	return M_WorldCampaignState.CurrentTurn;
}

int32 UWorldStateAndSaveManager::GetWorldGenerationSeed() const
{
	return M_WorldCampaignState.WorldGenerationSeed;
}

int32 UWorldStateAndSaveManager::GetEnemyObjectProceduralMapIndex() const
{
	return M_WorldCampaignState.EnemyObjectProceduralMapIndex;
}

void UWorldStateAndSaveManager::ResetEnemyObjectProceduralMapIndex()
{
	M_WorldCampaignState.EnemyObjectProceduralMapIndex = 0;
}

void UWorldStateAndSaveManager::SetEnemyObjectProceduralMapIndex(const int32 NewIndex)
{
	M_WorldCampaignState.EnemyObjectProceduralMapIndex = FMath::Max(0, NewIndex);
}

int32 UWorldStateAndSaveManager::AdvanceEnemyObjectProceduralMapIndex(const int32 MapCount)
{
	if (MapCount <= 0)
	{
		ResetEnemyObjectProceduralMapIndex();
		return M_WorldCampaignState.EnemyObjectProceduralMapIndex;
	}

	M_WorldCampaignState.EnemyObjectProceduralMapIndex++;
	if (M_WorldCampaignState.EnemyObjectProceduralMapIndex >= MapCount)
	{
		ResetEnemyObjectProceduralMapIndex();
	}

	return M_WorldCampaignState.EnemyObjectProceduralMapIndex;
}

FWorldCampaignState UWorldStateAndSaveManager::AggregateWorldCampaignState() const
{
	FWorldCampaignState WorldCampaignState = M_WorldCampaignState;
	// The progress manager owns the live progress; the cached copy is only what was last loaded.
	const AActor* OwningActor = GetOwner();
	const UWorldCampaignProgressManager* ProgressManager = IsValid(OwningActor)
		                                                       ? OwningActor->FindComponentByClass<
			                                                       UWorldCampaignProgressManager>()
		                                                       : nullptr;
	if (IsValid(ProgressManager))
	{
		WorldCampaignState.Progress = ProgressManager->GetProgressState();
	}
	return WorldCampaignState;
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

	const bool bDidSave = UGameplayStatics::SaveGameToSlot(SavePayload, GetCampaignSaveSlotName(),
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
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), GetCampaignSaveSlotName() + SaveGameExtension);
}

FString UWorldStateAndSaveManager::GetBackupSaveFilePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), BackupDirectoryName,
	                       GetCampaignSaveSlotName() + SaveGameExtension);
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
