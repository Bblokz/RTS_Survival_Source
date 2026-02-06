// Copyright (C) 2021-2025 Bas Blokzijl - All rights reserved.


#include "FRTS_Profile.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

FString FRTS_Profile::M_SaveSlotName = TEXT("PlayerProfile");
FString FRTS_Profile::M_BackupSaveSlotName = TEXT("PlayerProfile_Backup");
int32 FRTS_Profile::M_UserIndex = 0;
TStrongObjectPtr<USavePlayerProfile> FRTS_Profile::M_TestPlayerProfile = nullptr;
namespace ProfileHelpers
{
	static void NormalizePlayerCardSaveData(FPlayerCardSaveData& InOutCardSaveData)
	{
		// Mirrors the legacy ProfileValidator behaviour:
		// 1) Remove duplicate Tech/Train cards from PlayerAvailableCards (based on card name).
		{
			TSet<ERTSCard> SeenTechTrainCards;
			TArray<int32> IndicesToRemove;

			for (int32 CardIndex = 0; CardIndex < InOutCardSaveData.PlayerAvailableCards.Num(); ++CardIndex)
			{
				const ERTSCard Card = InOutCardSaveData.PlayerAvailableCards[CardIndex];
				const FString CardName = Global_GetCardAsString(Card);

				if (CardName.Contains(TEXT("Tech")) || CardName.Contains(TEXT("Train")))
				{
					if (SeenTechTrainCards.Contains(Card))
					{
						IndicesToRemove.Add(CardIndex);
						RTSFunctionLibrary::PrintToLog(
							FString::Printf(TEXT("Duplicate card '%s' found in PlayerAvailableCards and will be removed."), *CardName),
							false);
						continue;
					}

					SeenTechTrainCards.Add(Card);
				}
			}

			for (int32 RemoveIndex = IndicesToRemove.Num() - 1; RemoveIndex >= 0; --RemoveIndex)
			{
				InOutCardSaveData.PlayerAvailableCards.RemoveAt(IndicesToRemove[RemoveIndex]);
			}
		}

		// 2) Remove cards from PlayerAvailableCards that also appear in NomadicBuildingLayouts (remove first hit, legacy behaviour).
		for (const FNomadicBuildingLayoutData& LayoutData : InOutCardSaveData.NomadicBuildingLayouts)
		{
			for (const ERTSCard& Card : LayoutData.Cards)
			{
				const int32 IndexInAvailable = InOutCardSaveData.PlayerAvailableCards.Find(Card);
				if (IndexInAvailable == INDEX_NONE)
				{
					continue;
				}

				RTSFunctionLibrary::PrintToLog(
					FString::Printf(
						TEXT("Card '%s' found in both NomadicBuildingLayouts and PlayerAvailableCards. Removing from PlayerAvailableCards."),
						*Global_GetCardAsString(Card)),
					false);

				InOutCardSaveData.PlayerAvailableCards.RemoveAt(IndexInAvailable);
			}
		}
	}
}


FRTS_Profile::FRTS_Profile()
{
}

FRTS_Profile::~FRTS_Profile()
{
	M_TestPlayerProfile.Reset();
}

USavePlayerProfile* FRTS_Profile::LoadPlayerProfile()
{
    USavePlayerProfile* LoadedProfile = LoadProfileFromSlot(M_SaveSlotName, M_UserIndex);

    if (!LoadedProfile)
    {
        RTSFunctionLibrary::ReportError(TEXT("No saved profile found in LoadPlayerProfile."));

        // Attempt to load from backup slot
        LoadedProfile = LoadProfileFromSlot(M_BackupSaveSlotName, M_UserIndex);

        if (LoadedProfile)
        {
            RTSFunctionLibrary::PrintString(TEXT("Loaded profile from backup save."));
            ProfileValidator(LoadedProfile);
            return LoadedProfile;
        }
        else
        {
            RTSFunctionLibrary::ReportError(TEXT("No backup save found or failed to load from backup."));
        }

        // If all else fails, fall back to test profile
        RTSFunctionLibrary::PrintString(TEXT("Falling back to test profile."));
        return GetTestProfile();
    }

    ProfileValidator(LoadedProfile);
    return LoadedProfile;
}

FPlayerCardSaveData FRTS_Profile::BuildPlayerCardSaveData(
	TArray<ERTSCard> PlayerAvailableCards,
	TArray<FCardSaveData> SelectedCards,
	const int32 MaxCardsInUnits,
	const int32 MaxCardsInTechResources,
	TArray<FNomadicBuildingLayoutData> NomadicBuildingLayouts,
	TArray<ERTSCard> CardsInUnits,
	TArray<ERTSCard> CardsInTechResources)
{
	FPlayerCardSaveData OutCardSaveData;
	OutCardSaveData.PlayerAvailableCards = MoveTemp(PlayerAvailableCards);
	OutCardSaveData.SelectedCards = MoveTemp(SelectedCards);
	OutCardSaveData.CardsInUnits = MoveTemp(CardsInUnits);
	OutCardSaveData.CardsInTechResources = MoveTemp(CardsInTechResources);
	OutCardSaveData.MaxCardsInUnits = MaxCardsInUnits;
	OutCardSaveData.MaxCardsInTechResources = MaxCardsInTechResources;
	OutCardSaveData.NomadicBuildingLayouts = MoveTemp(NomadicBuildingLayouts);

	ProfileHelpers::NormalizePlayerCardSaveData(OutCardSaveData);
	return OutCardSaveData;
}

void FRTS_Profile::ProfileValidator(USavePlayerProfile* Profile)
{
	if (!Profile)
	{
		RTSFunctionLibrary::PrintToLog(TEXT("ProfileValidator called with a null profile."));
		return;
	}

	// First Validation: Remove duplicates of "Tech" or "Train" cards in M_PLayerAvailableCards
	{
		TSet<ERTSCard> SeenTechTrainCards;
		TArray<ERTSCard>& AvailableCards = Profile->M_PLayerAvailableCards;
		TArray<int32> IndicesToRemove;

		for (int32 i = 0; i < AvailableCards.Num(); ++i)
		{
			const ERTSCard Card = AvailableCards[i];
			const FString CardName = Global_GetCardAsString(Card);

			if (CardName.Contains(TEXT("Tech")) || CardName.Contains(TEXT("Train")))
			{
				if (SeenTechTrainCards.Contains(Card))
				{
					// Duplicate found; mark for removal and notify
					IndicesToRemove.Add(i);
					const FString Message = FString::Printf(
						TEXT("Duplicate card '%s' found in M_PLayerAvailableCards and will be removed."),
						*CardName);
					RTSFunctionLibrary::PrintToLog(Message, false);
				}
				else
				{
					SeenTechTrainCards.Add(Card);
				}
			}
		}

		// Remove duplicates starting from the end to avoid index shifting
		for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
		{
			AvailableCards.RemoveAt(IndicesToRemove[i]);
		}
	}

	// Second Validation: Remove cards from M_PLayerAvailableCards that also appear in M_TNomadicBuildingLayouts
	TArray<ERTSCard>& AvailableCards = Profile->M_PLayerAvailableCards;

	for (const FNomadicBuildingLayoutData& LayoutData : Profile->M_TNomadicBuildingLayouts)
	{
		for (const ERTSCard& Card : LayoutData.Cards)
		{
			const int32 IndexInAvailable = AvailableCards.Find(Card);
			if (IndexInAvailable != INDEX_NONE)
			{
				const FString CardName = Global_GetCardAsString(Card);
				const FString Message = FString::Printf(
					TEXT(
						"Card '%s' found in both M_TNomadicBuildingLayouts and M_PLayerAvailableCards. Removing from M_PLayerAvailableCards."),
					*CardName);
				RTSFunctionLibrary::PrintToLog(Message, false);
				AvailableCards.RemoveAt(IndexInAvailable);
			}
		}
	}
}

USavePlayerProfile* FRTS_Profile::GetTestProfile()
{
	if (!M_TestPlayerProfile.IsValid())
	{
		InitializeTestProfile();
	}
	return M_TestPlayerProfile.Get();
}

USavePlayerProfile* FRTS_Profile::LoadProfileFromSlot(const FString& SlotName, const int32 UserIndex)
{
	if (USaveGame* SaveGame = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex))
	{
		if (USavePlayerProfile* LoadedProfile = Cast<USavePlayerProfile>(SaveGame))
		{
			return LoadedProfile;
		}
		RTSFunctionLibrary::ReportError(TEXT("Loaded save game is not of type USavePlayerProfile."));
	}
	return nullptr;
}

bool FRTS_Profile::RestoreBackupSave(const FString& BackupSavePath, const FString& MainSavePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.FileExists(*BackupSavePath))
	{
		// Restore the backup save to the main save location
		if (PlatformFile.CopyFile(*MainSavePath, *BackupSavePath))
		{
			return true;
		}
		else
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to restore backup save."));
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(TEXT("Backup save file does not exist."));
	}
	return false;
}


bool FRTS_Profile::SavePlayerProfile(USavePlayerProfile* InPlayerProfile)
{
    if (!InPlayerProfile)
    {
        RTSFunctionLibrary::ReportError(TEXT("Invalid player profile provided to SavePlayerProfile."));
        return false;
    }

    // Attempt to save the game to the main slot
    const bool bSuccess = UGameplayStatics::SaveGameToSlot(InPlayerProfile, M_SaveSlotName, M_UserIndex);
    if (!bSuccess)
    {
        RTSFunctionLibrary::ReportError(TEXT("Failed to save player profile in SavePlayerProfile."));
        return false;
    }

    RTSFunctionLibrary::PrintString(TEXT("Player profile saved successfully."));

    // Create a backup of the saved game
    if (CreateBackupSave(InPlayerProfile))
    {
        RTSFunctionLibrary::PrintString(TEXT("Backup save created successfully."));
    }
    else
    {
        RTSFunctionLibrary::ReportError(TEXT("Failed to create backup save."));
        return false;
    }

    return true;
}

bool FRTS_Profile::CreateBackupSave(USavePlayerProfile* InPlayerProfile)
{
    if (!InPlayerProfile)
    {
        RTSFunctionLibrary::ReportError(TEXT("Invalid player profile provided to CreateBackupSave."));
        return false;
    }

    // Save the game to the backup slot
    const bool bSuccess = UGameplayStatics::SaveGameToSlot(InPlayerProfile, M_BackupSaveSlotName, M_UserIndex);
    if (!bSuccess)
    {
        RTSFunctionLibrary::ReportError(TEXT("Failed to save backup profile in CreateBackupSave."));
        return false;
    }

    return true;
}


bool FRTS_Profile::EnsureBackupDirectoryExists(const FString& BackupDirectory)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*BackupDirectory))
	{
		if (!PlatformFile.CreateDirectory(*BackupDirectory))
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to create backup directory."));
			return false;
		}
	}
	return true;
}


void FRTS_Profile::DebugPrintProfile(const USavePlayerProfile* InPlayerProfile, bool bUseErrorFunction)
{
	if (!InPlayerProfile)
	{
		RTSFunctionLibrary::ReportError(TEXT("DebugPrintProfile called with invalid player profile."));
		return;
	}

	FString ProfileDebugString;

	// Start building the debug string
	ProfileDebugString += TEXT("=== Player Profile Debug Information ===\n\n");

	// M_PLayerAvailableCards
	ProfileDebugString += TEXT("Player Available Cards:\n");
	for (const ERTSCard& Card : InPlayerProfile->M_PLayerAvailableCards)
	{
		FString CardString = Global_GetCardAsString(Card);
		ProfileDebugString += FString::Printf(TEXT(" - %s\n"), *CardString);
	}
	ProfileDebugString += TEXT("\n");

	// M_SelectedCards
	ProfileDebugString += TEXT("Selected Cards:\n");
	for (const FCardSaveData& CardData : InPlayerProfile->M_SelectedCards)
	{
		FString ResourceString = "No resources";
		if (CardData.Resources.ResourceCosts.Num() > 0)
		{
			ResourceString = "Resources: ";
			for (const auto& Resource : CardData.Resources.ResourceCosts)
			{
				ResourceString += FString::Printf(TEXT("%s: %d, "), *Global_GetResourceTypeAsString(Resource.Key),
				                                  Resource.Value);
			}
		}
		ProfileDebugString += TEXT(" - Card Save Data:\n");
		ProfileDebugString += FString::Printf(TEXT("   CardType: %d\n"), static_cast<int32>(CardData.CardType));
		ProfileDebugString += FString::Printf(TEXT("   Resources: %s\n"), *ResourceString);
		ProfileDebugString +=
			FString::Printf(TEXT("   Technology: %s\n"), *Global_GetTechAsString(CardData.Technology));
		ProfileDebugString += FString::Printf(
			TEXT("   TrainingOption: %s\n"), *CardData.TrainingOption.GetTrainingName());
		ProfileDebugString += FString::Printf(
			TEXT("   UnitToCreate: %s\n"), *CardData.UnitToCreate.GetTrainingName());
	}
	ProfileDebugString += TEXT("\n");

	// M_CardsInUnits
	ProfileDebugString += TEXT("Cards In Units:\n");
	for (const ERTSCard& Card : InPlayerProfile->M_CardsInUnits)
	{
		FString CardString = Global_GetCardAsString(Card);
		ProfileDebugString += FString::Printf(TEXT(" - %s\n"), *CardString);
	}
	ProfileDebugString += TEXT("\n");

	// M_CardsInTechResources
	ProfileDebugString += TEXT("Cards In Tech Resources:\n");
	for (const ERTSCard& Card : InPlayerProfile->M_CardsInTechResources)
	{
		FString CardString = Global_GetCardAsString(Card);
		ProfileDebugString += FString::Printf(TEXT(" - %s\n"), *CardString);
	}
	ProfileDebugString += TEXT("\n");

	// M_MaxCardsInUnits
	ProfileDebugString += FString::Printf(TEXT("Max Cards In Units: %d\n"), InPlayerProfile->M_MaxCardsInUnits);

	// M_MaxCardsInTechResources
	ProfileDebugString += FString::Printf(
		TEXT("Max Cards In Tech Resources: %d\n"), InPlayerProfile->M_MaxCardsInTechResources);
	ProfileDebugString += TEXT("\n");

	// M_TNomadicBuildingLayouts
	ProfileDebugString += TEXT("Nomadic Building Layouts:\n");
	for (const FNomadicBuildingLayoutData& LayoutData : InPlayerProfile->M_TNomadicBuildingLayouts)
	{
		ProfileDebugString += TEXT(" - Layout Data:\n");
		ProfileDebugString += FString::Printf(
			TEXT("   BuildingType: %d\n"), static_cast<int32>(LayoutData.BuildingType));
		ProfileDebugString += FString::Printf(TEXT("   Slots: %d\n"), LayoutData.Slots);
		ProfileDebugString += TEXT("   Cards:\n");
		for (const ERTSCard& Card : LayoutData.Cards)
		{
			FString CardString = Global_GetCardAsString(Card);
			ProfileDebugString += FString::Printf(TEXT("     - %s\n"), *CardString);
		}
	}
	ProfileDebugString += TEXT("\n");

	// Output the debug string
	if (bUseErrorFunction)
	{
		RTSFunctionLibrary::ReportError(ProfileDebugString);
	}
	else
	{
		RTSFunctionLibrary::PrintToLog(ProfileDebugString, false);
	}
}

void FRTS_Profile::InitializeTestProfile()
{
	TArray<ERTSCard> AvailableCards = {
		ERTSCard::Card_Ger_Pz38T, ERTSCard::Card_Ger_Pz38T,
		ERTSCard::Card_Ger_PzII_F, ERTSCard::Card_Ger_PzII_F,
		ERTSCard::Card_Ger_Scavengers, ERTSCard::Card_Ger_Scavengers,
		ERTSCard::Card_Ger_JagerKar98, ERTSCard::Card_Ger_JagerKar98,
		ERTSCard::Card_Ger_SteelFist, ERTSCard::Card_Ger_SteelFist,
		ERTSCard::Card_Ger_PZII_L, ERTSCard::Card_Ger_PZII_L,
		ERTSCard::Card_Ger_PZIII_M, ERTSCard::Card_Ger_PZIII_M,
		ERTSCard::Card_Ger_Sdkfz_140, ERTSCard::Card_Ger_Sdkfz_140,
		ERTSCard::Card_Ger_Hetzer, ERTSCard::Card_Ger_Hetzer,
		ERTSCard::Card_Ger_Marder, ERTSCard::Card_Ger_Marder,
		ERTSCard::Card_Ger_PzJager, ERTSCard::Card_Ger_PzJager,
		ERTSCard::Card_Ger_Pz1_150MM, ERTSCard::Card_Ger_Pz1_150MM,
		ERTSCard::Card_Ger_GammaTruck, ERTSCard::Card_Ger_GammaTruck,
		ERTSCard::Card_Ger_BarracksTruck, ERTSCard::Card_Ger_BarracksTruck,
		ERTSCard::Card_Ger_ForgeTruck, ERTSCard::Card_Ger_ForgeTruck,
		ERTSCard::Card_Ger_MechanicalDepotTruck, ERTSCard::Card_Ger_MechanicalDepotTruck,
		ERTSCard::Card_Ger_RefineryTruck, ERTSCard::Card_Ger_RefineryTruck,
		ERTSCard::Card_Ger_Tech_PzJager,
		ERTSCard::Card_Resource_Radixite, ERTSCard::Card_Resource_Radixite,
		ERTSCard::Card_Resource_Metal, ERTSCard::Card_Resource_Metal,
		ERTSCard::Card_Resource_VehicleParts, ERTSCard::Card_Resource_VehicleParts,
		ERTSCard::Card_Blueprint_Weapon, ERTSCard::Card_Blueprint_Weapon,
		ERTSCard::Card_Blueprint_Vehicle, ERTSCard::Card_Blueprint_Vehicle,
		ERTSCard::Card_Blueprint_Construction, ERTSCard::Card_Blueprint_Construction,
		ERTSCard::Card_Blueprint_Energy, ERTSCard::Card_Blueprint_Energy,
		ERTSCard::Card_RadixiteMetal, ERTSCard::Card_RadixiteMetal,
		ERTSCard::Card_RadixiteVehicleParts, ERTSCard::Card_RadixiteVehicleParts,
		ERTSCard::Card_High_Radixite, ERTSCard::Card_High_Radixite,
		ERTSCard::Card_High_Metal, ERTSCard::Card_High_Metal,
		ERTSCard::Card_High_VehicleParts, ERTSCard::Card_High_VehicleParts,
		ERTSCard::Card_AllBasicResources, ERTSCard::Card_AllBasicResources,
		ERTSCard::Card_SuperHigh_Radixite, ERTSCard::Card_SuperHigh_Radixite,
		ERTSCard::Card_SuperHigh_Metal, ERTSCard::Card_SuperHigh_Metal,
		ERTSCard::Card_SuperHigh_VehicleParts, ERTSCard::Card_SuperHigh_VehicleParts,
		ERTSCard::Card_High_Blueprint_Weapon, ERTSCard::Card_High_Blueprint_Weapon,
		ERTSCard::Card_High_Blueprint_Vehicle, ERTSCard::Card_High_Blueprint_Vehicle,
		ERTSCard::Card_High_Blueprint_Construction, ERTSCard::Card_High_Blueprint_Construction,
		ERTSCard::Card_High_Blueprint_Energy, ERTSCard::Card_High_Blueprint_Energy,
		ERTSCard::Card_EasterEgg,
		ERTSCard::Card_Ger_Tech_RadixiteArmor,
	};
	TArray<FCardSaveData> SavedCards;
	FCardSaveData CardData;
	// We can only save tehcnology and resource cards here as we do not have access to the

	TArray<ERTSCard> CardsInUnits = {ERTSCard::Card_Ger_Scavengers};
	TArray<ERTSCard> CardsInTechResources = {ERTSCard::Card_Resource_Radixite};
	int32 MaxCardsInUnits = 4;
	int32 MaxCardsInTechResources = 6;

	FNomadicBuildingLayoutData BarracksLayout;
	BarracksLayout.BuildingType = ENomadicLayoutBuildingType::Building_Barracks;
	BarracksLayout.Cards = {
		ERTSCard::Card_Train_Scavengers, ERTSCard::Card_Train_JagerKar98, ERTSCard::Card_Train_SteelFist
	};
	BarracksLayout.Slots = 5;

	FNomadicBuildingLayoutData ForgeLayout;
	ForgeLayout.BuildingType = ENomadicLayoutBuildingType::Building_Forge;
	ForgeLayout.Cards = {
		ERTSCard::Card_Train_Pz38T, ERTSCard::Card_Train_PzII_F, ERTSCard::Card_Train_PzI_150MM,
		ERTSCard::Card_Train_PzJager, ERTSCard::Card_Train_Marder, ERTSCard::Card_Train_Hetzer
	};
	ForgeLayout.Slots = 7;


	const auto TestProfile = TStrongObjectPtr<USavePlayerProfile>(NewObject<USavePlayerProfile>());
	if (!TestProfile || !TestProfile.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create new player TEST profile in FRTS_Profile."
			"\n NULL or INVALID TStrongObjectPtr<USavePlayerProfile>"));
		return;
	}
	TestProfile->Initialize(AvailableCards, SavedCards, MaxCardsInUnits, MaxCardsInTechResources,
	                        {BarracksLayout, ForgeLayout}, CardsInUnits, CardsInTechResources);
	M_TestPlayerProfile = TestProfile;
}


bool FRTS_Profile::SaveNewPlayerProfile(
	const TArray<ERTSCard>& PlayerAvailableCards,
	const TArray<FCardSaveData>& SelectedCards,
	int32 MaxCardsInUnits,
	int32 MaxCardsInTechResources,
	const TArray<FNomadicBuildingLayoutData>& TNomadicBuildingLayouts,
	const TArray<ERTSCard>& CardsInUnits,
	const TArray<ERTSCard>& CardsInTechResources)
{
	// Create a new USavePlayerProfile object
	USavePlayerProfile* NewProfile = Cast<USavePlayerProfile>(
		UGameplayStatics::CreateSaveGameObject(USavePlayerProfile::StaticClass()));

	if (!NewProfile)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create new player profile in SaveNewPlayerProfile."));
		return false;
	}

	// Initialize the profile with the provided data
	NewProfile->Initialize(
		PlayerAvailableCards,
		SelectedCards,
		MaxCardsInUnits,
		MaxCardsInTechResources,
		TNomadicBuildingLayouts,
		CardsInUnits,
		CardsInTechResources);

	DebugPrintProfile(NewProfile, true);

	// Save the profile using the existing SavePlayerProfile function
	SavePlayerProfile(NewProfile);
	return true;
}
