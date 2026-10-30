// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalArmyLibrary.h"

#include "RTS_Survival/CardSystem/PlayerProfile/PlayerProfileLoader/FRTS_Profile.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutDataAsset.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutSettings.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"

namespace SurvivalArmyLibraryConstants
{
	constexpr int32 DefaultSupplyCost = 10;
	constexpr ERTSFaction FallbackFaction = ERTSFaction::GerStrikeDivision;
}

namespace
{
	const FTrainingLayoutFaction* FindFactionLayout(const UTrainingLayoutDataAsset* TrainingLayout,
	                                                const ERTSFaction Faction)
	{
		if (not IsValid(TrainingLayout))
		{
			return nullptr;
		}
		if (const FTrainingLayoutFaction* FactionLayout = TrainingLayout->FactionLayouts.Find(Faction))
		{
			return FactionLayout;
		}
		// Same fallback as UTrainingLayoutDataAsset::FindBuildingLayout for factions without a layout.
		return TrainingLayout->FactionLayouts.Find(SurvivalArmyLibraryConstants::FallbackFaction);
	}

	TArray<FTrainingOption> GetFactionUnits(const ERTSFaction Faction)
	{
		TArray<FTrainingOption> FactionUnits;
		const FTrainingLayoutFaction* FactionLayout = FindFactionLayout(
			UTrainingLayoutSettings::Get()->LoadTrainingLayout(), Faction);
		if (not FactionLayout)
		{
			return FactionUnits;
		}
		for (const TPair<ENomadicSubtype, FTrainingLayoutBuilding>& BuildingLayout : FactionLayout->Buildings)
		{
			for (const FTrainingLayoutSubSection& SubSection : BuildingLayout.Value.SubSections)
			{
				for (const FTrainingLayoutSlot& LayoutSlot : SubSection.Slots)
				{
					const FTrainingOption Unit = LayoutSlot.ToTrainingOption();
					if (not Unit.IsNone())
					{
						FactionUnits.AddUnique(Unit);
					}
				}
			}
		}
		return FactionUnits;
	}

	const FSurvivalUnitSupplyCost* FindSupplyEntry(const USurvivalModeSettings* Settings, const FTrainingOption& Unit)
	{
		if (not IsValid(Settings))
		{
			return nullptr;
		}
		// Keyed as the spawner keys units: the table lists harvesters as UNType_Harvester, the layout as tanks.
		const FTrainingOption UnitKey = UPlayerUnitVariantManager::GetSpawnerKey(Unit);
		return Settings->Army.UnitSupplyCosts.FindByPredicate([&UnitKey](const FSurvivalUnitSupplyCost& Entry)
		{
			return UPlayerUnitVariantManager::GetSpawnerKey(Entry.Unit) == UnitKey;
		});
	}

	/** @return Saved variant per spawner key, only for variants the data asset implements; loads the profile once. */
	TMap<FTrainingOption, EUnitVariant> LoadProfileVariants()
	{
		TMap<FTrainingOption, EUnitVariant> ProfileVariants;
		const USavePlayerProfile* PlayerProfile = FRTS_Profile::LoadPlayerProfile();
		const UUnitVariantDataAsset* VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
		if (not IsValid(PlayerProfile) or not IsValid(VariantDataAsset))
		{
			return ProfileVariants;
		}
		for (const FUnitVariantSelection& Selection : PlayerProfile->M_UnitVariantSelections)
		{
			const FTrainingOption SpawnerKey = UPlayerUnitVariantManager::GetSpawnerKey(Selection.Unit);
			if (VariantDataAsset->GetIsVariantImplemented(SpawnerKey, Selection.Variant))
			{
				ProfileVariants.Add(SpawnerKey, Selection.Variant);
			}
		}
		return ProfileVariants;
	}

	EUnitVariant FindProfileVariant(const TMap<FTrainingOption, EUnitVariant>& ProfileVariants,
	                                const FTrainingOption& Unit)
	{
		const EUnitVariant* Variant = ProfileVariants.Find(UPlayerUnitVariantManager::GetSpawnerKey(Unit));
		return Variant ? *Variant : EUnitVariant::Base;
	}

	FText GetEntryDisplayName(const FTrainingOption& Unit, const EUnitVariant Variant)
	{
		if (Variant != EUnitVariant::Base)
		{
			const UUnitVariantDataAsset* VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
			if (IsValid(VariantDataAsset))
			{
				const FUnitVariantDefinition* Definition = VariantDataAsset->FindVariantDefinition(
					UPlayerUnitVariantManager::GetSpawnerKey(Unit), Variant);
				if (Definition and not Definition->DisplayName.IsEmpty())
				{
					return Definition->DisplayName;
				}
			}
		}
		return FText::FromString(Unit.GetDisplayName());
	}
}

TArray<FSurvivalArmyEntry> USurvivalArmyLibrary::BuildArmyRoster(const USurvivalModeSettings* Settings,
                                                                 const ERTSFaction Faction,
                                                                 const ARTSAsyncSpawner* AsyncSpawner)
{
	TArray<FSurvivalArmyEntry> Roster;
	const ERTSFaction RosterFaction = Faction == ERTSFaction::NotInitialised
		                                  ? ERTSFaction::GerBreakthroughDoctrine
		                                  : Faction;
	const FSurvivalArmyEntry HQEntry = BuildHQEntry(Settings, RosterFaction);
	Roster.Add(HQEntry);
	if (not IsValid(Settings))
	{
		return Roster;
	}
	const TMap<FTrainingOption, EUnitVariant> ProfileVariants = LoadProfileVariants();

	for (const FTrainingOption& Unit : GetFactionUnits(RosterFaction))
	{
		if (Unit == HQEntry.Unit
			or not Settings->Army.AllowedUnitTypes.Contains(Unit.UnitType)
			or Settings->Army.ExcludedUnits.Contains(Unit))
		{
			continue;
		}
		if (AsyncSpawner and AsyncSpawner->GetBaseTrainingOptionClass(
			UPlayerUnitVariantManager::GetSpawnerKey(Unit)).IsNull())
		{
			UE_LOG(LogTemp, Log, TEXT("Survival roster: %s left out, the spawner has no class for it."),
			       *Unit.GetTrainingName());
			continue;
		}
		FSurvivalArmyEntry Entry;
		Entry.Unit = Unit;
		Entry.Variant = FindProfileVariant(ProfileVariants, Unit);
		Entry.SupplyCost = GetSupplyCost(Settings, Unit, Entry.Variant);
		const FSurvivalUnitSupplyCost* SupplyEntry = FindSupplyEntry(Settings, Unit);
		Entry.MaxCopies = SupplyEntry ? SupplyEntry->MaxCopies : 0;
		Entry.DisplayName = GetEntryDisplayName(Unit, EUnitVariant::Base);
		// The table name beats the generated enum name; the screens show the variant on its own line.
		if (SupplyEntry and not SupplyEntry->DisplayName.IsEmpty())
		{
			Entry.DisplayName = SupplyEntry->DisplayName;
		}
		Roster.Add(Entry);
	}
	return Roster;
}

FSurvivalArmyEntry USurvivalArmyLibrary::BuildHQEntry(const USurvivalModeSettings* Settings, const ERTSFaction Faction)
{
	FSurvivalArmyEntry HQEntry;
	HQEntry.Unit = FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq));
	if (IsValid(Settings))
	{
		if (const FTrainingOption* FactionHQ = Settings->Army.HQPerFaction.Find(Faction))
		{
			if (not FactionHQ->IsNone())
			{
				HQEntry.Unit = *FactionHQ;
			}
		}
	}
	HQEntry.Variant = GetProfileVariant(HQEntry.Unit);
	HQEntry.SupplyCost = 0;
	HQEntry.MaxCopies = 1;
	HQEntry.bIsHQ = true;
	HQEntry.DisplayName = GetEntryDisplayName(HQEntry.Unit, HQEntry.Variant);
	return HQEntry;
}

int32 USurvivalArmyLibrary::GetSupplyCost(const USurvivalModeSettings* Settings, const FTrainingOption& Unit,
                                          const EUnitVariant Variant)
{
	if (not IsValid(Settings))
	{
		return SurvivalArmyLibraryConstants::DefaultSupplyCost;
	}
	if (const FSurvivalUnitSupplyCost* SupplyEntry = FindSupplyEntry(Settings, Unit))
	{
		if (const int32* VariantSupply = SupplyEntry->VariantSupply.Find(Variant))
		{
			return FMath::Max(0, *VariantSupply);
		}
		return SupplyEntry->BaseSupply;
	}
	if (const int32* FallbackSupply = Settings->Army.FallbackSupplyPerUnitType.Find(Unit.UnitType))
	{
		return FMath::Max(0, *FallbackSupply);
	}
	return SurvivalArmyLibraryConstants::DefaultSupplyCost;
}

int32 USurvivalArmyLibrary::GetTotalSupply(const TArray<FSurvivalArmyPick>& Picks)
{
	int32 TotalSupply = 0;
	for (const FSurvivalArmyPick& Pick : Picks)
	{
		if (not Pick.Entry.bIsHQ)
		{
			TotalSupply += Pick.Entry.SupplyCost;
		}
	}
	return TotalSupply;
}

bool USurvivalArmyLibrary::GetIsArmyValid(const USurvivalModeSettings* Settings, const TArray<FSurvivalArmyPick>& Picks,
                                          FText& OutReason)
{
	if (not IsValid(Settings))
	{
		OutReason = NSLOCTEXT("Survival", "NoSettings", "The survival settings are missing.");
		return false;
	}
	int32 HQCount = 0;
	TSet<int32> UsedSlots;
	TMap<FTrainingOption, int32> CopiesPerUnit;
	for (const FSurvivalArmyPick& Pick : Picks)
	{
		if (Pick.SlotIndex < 0 or Pick.SlotIndex >= Settings->Army.SlotCount)
		{
			OutReason = NSLOCTEXT("Survival", "BadSlot", "A unit is in a slot that does not exist.");
			return false;
		}
		if (UsedSlots.Contains(Pick.SlotIndex))
		{
			OutReason = NSLOCTEXT("Survival", "DoubleSlot", "Two units share one slot.");
			return false;
		}
		UsedSlots.Add(Pick.SlotIndex);
		if (Pick.Entry.bIsHQ)
		{
			++HQCount;
			if (Pick.SlotIndex != 0)
			{
				OutReason = NSLOCTEXT("Survival", "HQSlot", "The HQ must be in the first slot.");
				return false;
			}
			continue;
		}
		int32& Copies = CopiesPerUnit.FindOrAdd(Pick.Entry.Unit);
		++Copies;
		if (Pick.Entry.MaxCopies > 0 and Copies > Pick.Entry.MaxCopies)
		{
			OutReason = FText::Format(NSLOCTEXT("Survival", "TooManyCopies", "At most {0} x {1}."),
			                          FText::AsNumber(Pick.Entry.MaxCopies), Pick.Entry.DisplayName);
			return false;
		}
	}
	if (HQCount != 1)
	{
		OutReason = NSLOCTEXT("Survival", "NoHQ", "The army needs its HQ.");
		return false;
	}
	const int32 TotalSupply = GetTotalSupply(Picks);
	if (TotalSupply > Settings->Army.SupplyBudget)
	{
		OutReason = FText::Format(NSLOCTEXT("Survival", "OverBudget", "Supply {0} / {1}: remove units first."),
		                          FText::AsNumber(TotalSupply), FText::AsNumber(Settings->Army.SupplyBudget));
		return false;
	}
	return true;
}

EUnitVariant USurvivalArmyLibrary::GetProfileVariant(const FTrainingOption& Unit)
{
	return FindProfileVariant(LoadProfileVariants(), Unit);
}
