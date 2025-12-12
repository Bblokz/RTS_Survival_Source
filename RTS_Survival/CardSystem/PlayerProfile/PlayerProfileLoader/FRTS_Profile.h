// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"


struct FCardSaveData;
struct FNomadicBuildingLayoutData;
enum class ERTSCard : uint8;
class USavePlayerProfile;


class RTS_SURVIVAL_API FRTS_Profile
{
	FRTS_Profile();
	~FRTS_Profile();

public:
	/**
     * @brief Loads the player profile from the main save slot or backup if necessary.
     * @return A pointer to the loaded USavePlayerProfile, or the test profile if loading fails.
     */
    static USavePlayerProfile* LoadPlayerProfile();

	/**
    * @brief Validates and cleans up the player profile by removing duplicate cards and ensuring consistency.
    * @param Profile The player profile to validate.
    */
	static void ProfileValidator(USavePlayerProfile* Profile);

	static USavePlayerProfile* GetTestProfile();

	static bool SaveNewPlayerProfile(
		const TArray<ERTSCard>& PlayerAvailableCards,
		const TArray<FCardSaveData>& SelectedCards,
		int32 MaxCardsInUnits,
		int32 MaxCardsInTechResources,
		const TArray<FNomadicBuildingLayoutData>& TNomadicBuildingLayouts,
		const TArray<ERTSCard>& CardsInUnits,
		const TArray<ERTSCard>& CardsInTechResources);

	static void DebugPrintProfile(const USavePlayerProfile* InPlayerProfile, bool bUseErrorFunction = false);

private:

	/**
 * @brief Attempts to load the player profile from the specified save slot.
 * @param SlotName The name of the save slot.
 * @param UserIndex The user index.
 * @return A pointer to the loaded USavePlayerProfile, or nullptr if loading fails.
 */
	static USavePlayerProfile* LoadProfileFromSlot(const FString& SlotName, int32 UserIndex);

	/**
	 * @brief Attempts to restore the backup save to the main save location.
	 * @param BackupSavePath The path to the backup save file.
	 * @param MainSavePath The path to the main save file.
	 * @return True if the restore operation succeeded, false otherwise.
	 */
	static bool RestoreBackupSave(const FString& BackupSavePath, const FString& MainSavePath);

    /** The name of the backup save slot */
    static FString M_BackupSaveSlotName;

    // Update the CreateBackupSave function signature
    /**
     * @brief Creates a backup of the player profile by saving it to a backup slot.
     * @param InPlayerProfile The player profile to save as a backup.
     * @return True if the backup operation succeeded, false otherwise.
     */
    static bool CreateBackupSave(USavePlayerProfile* InPlayerProfile);
	/**
	 * @brief Ensures that the backup directory exists, creating it if necessary.
	 * @param BackupDirectory The path to the backup directory.
	 * @return True if the directory exists or was created successfully, false otherwise.
	 */
	static bool EnsureBackupDirectoryExists(const FString& BackupDirectory);
	
	static FString M_SaveSlotName;
	static int32 M_UserIndex;

	static TStrongObjectPtr<USavePlayerProfile> M_TestPlayerProfile;

	static void InitializeTestProfile();


	static bool SavePlayerProfile(USavePlayerProfile* InPlayerProfile);
};
