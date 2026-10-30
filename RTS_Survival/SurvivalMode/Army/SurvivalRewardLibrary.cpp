// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalRewardLibrary.h"

#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyLibrary.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"

namespace SurvivalRewardLibraryConstants
{
	// Themes rolled for the generated options; Authored replaces one of them by chance.
	const TArray<ESurvivalRewardTheme> GeneratedThemes = {
		ESurvivalRewardTheme::Infantry, ESurvivalRewardTheme::Armour, ESurvivalRewardTheme::Support,
		ESurvivalRewardTheme::Combined
	};
}

int32 FSurvivalRewardLibrary::GetRewardSupply(const USurvivalModeSettings* Settings, const int32 RoundNumber)
{
	if (not IsValid(Settings))
	{
		return 16;
	}
	const FSurvivalRewardSettings& Rewards = Settings->Rewards;
	const int32 Supply = Rewards.FirstRewardSupply + Rewards.RewardSupplyPerRound * FMath::Max(0, RoundNumber - 1);
	return FMath::Min(Supply, Rewards.MaxRewardSupply);
}

FText FSurvivalRewardLibrary::GetThemeTitle(const ESurvivalRewardTheme Theme)
{
	switch (Theme)
	{
	case ESurvivalRewardTheme::Infantry:
		return NSLOCTEXT("SurvivalReward", "ThemeInfantry", "INFANTRY DETACHMENT");
	case ESurvivalRewardTheme::Armour:
		return NSLOCTEXT("SurvivalReward", "ThemeArmour", "ARMOURED COLUMN");
	case ESurvivalRewardTheme::Support:
		return NSLOCTEXT("SurvivalReward", "ThemeSupport", "SUPPORT GROUP");
	case ESurvivalRewardTheme::Combined:
		return NSLOCTEXT("SurvivalReward", "ThemeCombined", "COMBINED ARMS");
	case ESurvivalRewardTheme::Authored:
		return NSLOCTEXT("SurvivalReward", "ThemeAuthored", "SPECIAL DELIVERY");
	}
	return FText::GetEmpty();
}

bool FSurvivalRewardLibrary::GetDoesEntryFitTheme(const USurvivalModeSettings* Settings,
                                                  const FSurvivalArmyEntry& Entry, const ESurvivalRewardTheme Theme)
{
	if (Entry.bIsHQ)
	{
		return false;
	}
	const bool bIsSquad = Entry.Unit.UnitType == EAllUnitType::UNType_Squad;
	// The training layout lists harvesters as tanks.
	const bool bIsHarvester = Entry.Unit.UnitType == EAllUnitType::UNType_Harvester
		or (Entry.Unit.UnitType == EAllUnitType::UNType_Tank and StaticEnum<ETankSubtype>()->GetNameStringByValue(
			Entry.Unit.SubtypeValue).Contains(TEXT("Harvester")));
	// Team weapons and walkers are the squads a drop aircraft has to land for.
	const bool bIsHeavySquad = bIsSquad and Settings->Deployment.SquadsDeliveredByLandingAircraft.Contains(
		static_cast<ESquadSubtype>(Entry.Unit.SubtypeValue));
	switch (Theme)
	{
	case ESurvivalRewardTheme::Infantry:
		return bIsSquad and not bIsHeavySquad;
	case ESurvivalRewardTheme::Armour:
		return Entry.Unit.UnitType == EAllUnitType::UNType_Tank and not bIsHarvester;
	case ESurvivalRewardTheme::Support:
		return bIsHeavySquad or bIsHarvester or Entry.Unit.UnitType == EAllUnitType::UNType_Nomadic;
	case ESurvivalRewardTheme::Combined:
		// Trucks and harvesters are left to the support sets so a combined set is always fighting units.
		return Entry.Unit.UnitType != EAllUnitType::UNType_Nomadic and not bIsHarvester;
	case ESurvivalRewardTheme::Authored:
	default:
		return false;
	}
}

FSurvivalRewardOption FSurvivalRewardLibrary::RollThemedOption(const USurvivalModeSettings* Settings,
                                                               const TArray<FSurvivalArmyEntry>& Roster,
                                                               const ESurvivalRewardTheme Theme,
                                                               const int32 SupplyWorth, FRandomStream& Random)
{
	FSurvivalRewardOption Option;
	Option.Theme = Theme;
	Option.Title = GetThemeTitle(Theme);

	TArray<const FSurvivalArmyEntry*> Candidates;
	for (const FSurvivalArmyEntry& Entry : Roster)
	{
		if (GetDoesEntryFitTheme(Settings, Entry, Theme) and Entry.SupplyCost > 0)
		{
			Candidates.Add(&Entry);
		}
	}
	int32 SupplyLeft = SupplyWorth;
	TMap<FTrainingOption, int32> Copies;
	const int32 MaxUnits = FMath::Max(1, Settings->Rewards.MaxUnitsPerSet);
	while (Option.Units.Num() < MaxUnits)
	{
		TArray<const FSurvivalArmyEntry*> Affordable;
		for (const FSurvivalArmyEntry* Candidate : Candidates)
		{
			const int32 UsedCopies = Copies.FindRef(Candidate->Unit);
			if (Candidate->SupplyCost <= SupplyLeft
				and (Candidate->MaxCopies <= 0 or UsedCopies < Candidate->MaxCopies))
			{
				Affordable.Add(Candidate);
			}
		}
		if (Affordable.IsEmpty())
		{
			break;
		}
		// Favour the expensive end so a set spends its worth on a few strong units instead of many cheap ones.
		Affordable.Sort([](const FSurvivalArmyEntry& A, const FSurvivalArmyEntry& B)
		{
			return A.SupplyCost > B.SupplyCost;
		});
		const int32 PickRange = FMath::Max(1, FMath::CeilToInt(Affordable.Num() * 0.6f));
		const FSurvivalArmyEntry* Picked = Affordable[Random.RandRange(0, PickRange - 1)];
		Option.Units.Add(*Picked);
		Option.TotalSupply += Picked->SupplyCost;
		SupplyLeft -= Picked->SupplyCost;
		Copies.FindOrAdd(Picked->Unit)++;
	}
	return Option;
}

bool FSurvivalRewardLibrary::TryBuildAuthoredOption(const USurvivalModeSettings* Settings,
                                                    const TArray<FSurvivalArmyEntry>& Roster,
                                                    const int32 RoundNumber, FRandomStream& Random,
                                                    FSurvivalRewardOption& OutOption)
{
	TArray<const FSurvivalAuthoredRewardSet*> Eligible;
	float TotalChance = 0.f;
	for (const FSurvivalAuthoredRewardSet& Set : Settings->Rewards.AuthoredSets)
	{
		if (RoundNumber >= Set.MinRound and not Set.Units.IsEmpty())
		{
			Eligible.Add(&Set);
			TotalChance += Set.OfferChance;
		}
	}
	if (Eligible.IsEmpty() or Random.FRand() >= FMath::Min(1.f, TotalChance))
	{
		return false;
	}
	float Roll = Random.FRandRange(0.f, TotalChance);
	const FSurvivalAuthoredRewardSet* Chosen = Eligible.Last();
	for (const FSurvivalAuthoredRewardSet* Set : Eligible)
	{
		Roll -= Set->OfferChance;
		if (Roll <= 0.f)
		{
			Chosen = Set;
			break;
		}
	}
	OutOption.Theme = ESurvivalRewardTheme::Authored;
	OutOption.Title = Chosen->Title.IsEmpty() ? GetThemeTitle(ESurvivalRewardTheme::Authored) : Chosen->Title;
	for (const FTrainingOption& Unit : Chosen->Units)
	{
		// Shown as the roster shows the unit (selected variant, cost); units outside the roster still come.
		const FSurvivalArmyEntry* RosterEntry = Roster.FindByPredicate([&Unit](const FSurvivalArmyEntry& Entry)
		{
			return Entry.Unit == Unit;
		});
		FSurvivalArmyEntry Entry;
		if (RosterEntry)
		{
			Entry = *RosterEntry;
		}
		else
		{
			Entry.Unit = Unit;
			Entry.Variant = USurvivalArmyLibrary::GetProfileVariant(Unit);
			Entry.SupplyCost = USurvivalArmyLibrary::GetSupplyCost(Settings, Unit, Entry.Variant);
			Entry.DisplayName = FText::FromString(Unit.GetDisplayName());
		}
		OutOption.Units.Add(Entry);
		OutOption.TotalSupply += Entry.SupplyCost;
	}
	return true;
}

TArray<FSurvivalRewardOption> FSurvivalRewardLibrary::BuildRewardOptions(const USurvivalModeSettings* Settings,
                                                                         const ERTSFaction Faction,
                                                                         const int32 RoundNumber,
                                                                         FRandomStream& Random,
                                                                         const ARTSAsyncSpawner* AsyncSpawner)
{
	TArray<FSurvivalRewardOption> Options;
	if (not IsValid(Settings))
	{
		return Options;
	}
	const TArray<FSurvivalArmyEntry> Roster = USurvivalArmyLibrary::BuildArmyRoster(
		Settings, Faction, AsyncSpawner);
	const int32 SupplyWorth = GetRewardSupply(Settings, RoundNumber);
	const int32 OptionCount = FMath::Max(1, Settings->Rewards.OptionsPerReward);

	TArray<ESurvivalRewardTheme> Themes = SurvivalRewardLibraryConstants::GeneratedThemes;
	for (int32 Index = Themes.Num() - 1; Index > 0; --Index)
	{
		Themes.Swap(Index, Random.RandRange(0, Index));
	}
	FSurvivalRewardOption AuthoredOption;
	const bool bHasAuthored = TryBuildAuthoredOption(Settings, Roster, RoundNumber, Random, AuthoredOption);
	if (bHasAuthored)
	{
		Options.Add(AuthoredOption);
	}
	for (const ESurvivalRewardTheme Theme : Themes)
	{
		if (Options.Num() >= OptionCount)
		{
			break;
		}
		FSurvivalRewardOption Option = RollThemedOption(Settings, Roster, Theme, SupplyWorth, Random);
		if (not Option.Units.IsEmpty())
		{
			Options.Add(Option);
		}
	}
	// A theme without affordable units (no armour under the first reward's worth) is replaced by combined sets.
	int32 Attempts = 0;
	while (Options.Num() < OptionCount and Attempts++ < OptionCount * 2)
	{
		FSurvivalRewardOption Option = RollThemedOption(Settings, Roster, ESurvivalRewardTheme::Combined, SupplyWorth,
		                                                Random);
		if (Option.Units.IsEmpty())
		{
			break;
		}
		Options.Add(Option);
	}
	return Options;
}
