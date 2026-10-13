// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TrainingLayoutSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/CardSystem/PlayerProfile/PlayerProfileLoader/FRTS_Profile.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Slate/SlateBrushAsset.h"
#include "TrainingLayoutDataAsset.h"
#include "TrainingLayoutSettings.h"

namespace TrainingLayoutCards
{
	bool GetIsTrainingCardType(const ECardType CardType)
	{
		switch (CardType)
		{
		case ECardType::BarracksTrain:
		case ECardType::MechanicalDepotTrain:
		case ECardType::ForgeTrain:
		case ECardType::T2FactoryTrain:
		case ECardType::AirbaseTrain:
		case ECardType::ExperimentalFactoryTrain:
			return true;
		default:
			return false;
		}
	}
}

UTrainingLayoutSubsystem* UTrainingLayoutSubsystem::Get(const UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	return World->GetSubsystem<UTrainingLayoutSubsystem>();
}

bool UTrainingLayoutSubsystem::BuildSubSectionsForBuilding(
	const ENomadicSubtype Building,
	TArray<FTrainingSubSectionState>& OutSubSections)
{
	OutSubSections.Reset();
	const UTrainingLayoutDataAsset* TrainingLayout = GetTrainingLayout();
	if (not TrainingLayout)
	{
		return false;
	}

	const FTrainingLayoutBuilding* BuildingLayout = TrainingLayout->FindBuildingLayout(
		FRTS_Statics::GetPlayerFaction(this), Building);
	if (not BuildingLayout || BuildingLayout->SubSections.IsEmpty())
	{
		return false;
	}

	const int32 SubSectionCount = FMath::Min(BuildingLayout->SubSections.Num(),
	                                         TrainingLayoutConstants::MaxSubSectionsPerBuilding);
	int32 FlatOptionIndex = 0;
	for (int32 SubSectionIndex = 0; SubSectionIndex < SubSectionCount; ++SubSectionIndex)
	{
		const FTrainingLayoutSubSection& LayoutSubSection = BuildingLayout->SubSections[SubSectionIndex];
		FTrainingSubSectionState& SubSectionState = OutSubSections.AddDefaulted_GetRef();
		SubSectionState.DisplayName = LayoutSubSection.DisplayName;
		SubSectionState.FirstFlatOptionIndex = FlatOptionIndex;
		// Every sub section fills the whole grid; missing slots at the end are empty.
		SubSectionState.Options.SetNum(TrainingLayoutConstants::SlotsPerSubSection);

		const int32 SlotCount = FMath::Min(LayoutSubSection.Slots.Num(), TrainingLayoutConstants::SlotsPerSubSection);
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			const FTrainingOption Option = LayoutSubSection.Slots[SlotIndex].ToTrainingOption();
			if (GetIsOptionUnlocked(Option, BuildingLayout->bRequiresUnitCards))
			{
				SubSectionState.Options[SlotIndex] = Option;
			}
		}
		FlatOptionIndex += SubSectionState.Options.Num();
	}
	return true;
}

const TArray<FTrainingQuickNavEntry>& UTrainingLayoutSubsystem::GetQuickNavEntries()
{
	if (const UTrainingLayoutDataAsset* TrainingLayout = GetTrainingLayout())
	{
		return TrainingLayout->QuickNavBuildings;
	}
	static const TArray<FTrainingQuickNavEntry> NoQuickNavEntries;
	return NoQuickNavEntries;
}

bool UTrainingLayoutSubsystem::GetQuickNavIconBrush(const FTrainingQuickNavEntry& QuickNavEntry,
                                                    FSlateBrush& OutIconBrush)
{
	if (UTexture2D* IconOverride = QuickNavEntry.IconOverride.LoadSynchronous())
	{
		OutIconBrush = FSlateBrush();
		OutIconBrush.SetResourceObject(IconOverride);
		return true;
	}
	const FTrainingOption BuildingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(QuickNavEntry.Building));
	return GetIconBrushFromTrainingOptionTable(BuildingOption, OutIconBrush);
}

const UTrainingLayoutDataAsset* UTrainingLayoutSubsystem::GetTrainingLayout()
{
	if (M_TrainingLayout)
	{
		return M_TrainingLayout;
	}
	M_TrainingLayout = UTrainingLayoutSettings::Get()->LoadTrainingLayout();
	return M_TrainingLayout;
}

void UTrainingLayoutSubsystem::LoadCardUnlocksIfNeeded()
{
	if (bM_HasLoadedCardUnlocks)
	{
		return;
	}
	bM_HasLoadedCardUnlocks = true;

	const USavePlayerProfile* PlayerProfile = FRTS_Profile::LoadPlayerProfile();
	if (not IsValid(PlayerProfile))
	{
		RTSFunctionLibrary::ReportError("Could not load the player profile to find the unlocked training options."
			"\n No unit that needs a unit card can be trained."
			"\n See UTrainingLayoutSubsystem::LoadCardUnlocksIfNeeded.");
		return;
	}

	for (const FCardSaveData& SelectedCard : PlayerProfile->M_SelectedCards)
	{
		if (not TrainingLayoutCards::GetIsTrainingCardType(SelectedCard.CardType))
		{
			continue;
		}
		M_OptionsUnlockedByCards.Add(SelectedCard.TrainingOption);
	}
}

bool UTrainingLayoutSubsystem::GetIsOptionUnlocked(const FTrainingOption& Option,
                                                   const bool bBuildingRequiresUnitCards)
{
	if (Option.IsNone() || not bBuildingRequiresUnitCards)
	{
		return true;
	}
	const UTrainingLayoutDataAsset* TrainingLayout = GetTrainingLayout();
	if (not TrainingLayout || TrainingLayout->bUnlockAllUnitsImmediately)
	{
		return true;
	}
	LoadCardUnlocksIfNeeded();
	return M_OptionsUnlockedByCards.Contains(Option);
}

bool UTrainingLayoutSubsystem::GetIconBrushFromTrainingOptionTable(const FTrainingOption& Option,
                                                                   FSlateBrush& OutIconBrush)
{
	const UTrainingLayoutDataAsset* TrainingLayout = GetTrainingLayout();
	if (not TrainingLayout)
	{
		return false;
	}
	const UDataTable* IconTable = TrainingLayout->TrainingOptionIconTable.LoadSynchronous();
	if (not IsValid(IconTable) || not IconTable->GetRowStruct())
	{
		return false;
	}

	const FName RowName(*UTrainingOptionLibrary::GetTrainingOptionName(Option.UnitType, Option.SubtypeValue));
	const uint8* RowData = IconTable->FindRowUnchecked(RowName);
	if (not RowData)
	{
		return false;
	}

	// The row is a blueprint struct, so its slate brush member is found by type instead of by name.
	for (TFieldIterator<FObjectPropertyBase> PropertyIt(IconTable->GetRowStruct()); PropertyIt; ++PropertyIt)
	{
		const USlateBrushAsset* BrushAsset = Cast<USlateBrushAsset>(
			PropertyIt->GetObjectPropertyValue_InContainer(RowData));
		if (not IsValid(BrushAsset))
		{
			continue;
		}
		OutIconBrush = BrushAsset->Brush;
		return true;
	}
	return false;
}
