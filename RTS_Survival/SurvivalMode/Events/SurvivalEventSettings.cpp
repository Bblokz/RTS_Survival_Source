// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalEventSettings.h"

#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"

DEFINE_LOG_CATEGORY(LogSurvivalEvents);

namespace SurvivalEventSettingsDefaults
{
	const TCHAR* const FuelConvoyAsset = TEXT("/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_Fuel.DA_Convoy_Fuel");
	const TCHAR* const PartsConvoyAsset = TEXT(
		"/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_VehicleParts.DA_Convoy_VehicleParts");
	// SM_Desert_01 oasis basin (the POI planner's exclusion: centroid, radius 6 150, + 3 000 rim).
	const FVector2D OasisCentre(16000.0, -3525.0);
	constexpr float OasisRadiusWithRim = 9150.f;
	constexpr int32 MediumTierFromRound = 5;
	constexpr int32 HeavyTierFromRound = 10;
}

USurvivalEventSettings::USurvivalEventSettings()
{
	using namespace SurvivalEventSettingsDefaults;
	FSurvivalConvoyEventEntry Fuel;
	Fuel.Definition = TSoftObjectPtr<UConvoyDefinition>(FSoftObjectPath(FuelConvoyAsset));
	Convoys.Add(Fuel);
	FSurvivalConvoyEventEntry Parts;
	Parts.Definition = TSoftObjectPtr<UConvoyDefinition>(FSoftObjectPath(PartsConvoyAsset));
	Convoys.Add(Parts);

	auto AddTierStep = [this](const int32 FromRound, const int32 GuardTier)
	{
		FSurvivalConvoyTierStep& Step = GuardTierByRound.AddDefaulted_GetRef();
		Step.FromRound = FromRound;
		Step.GuardTier = GuardTier;
	};
	AddTierStep(1, 0);
	AddTierStep(MediumTierFromRound, 1);
	AddTierStep(HeavyTierFromRound, 2);

	TierBonusByDifficulty.Add(ERTSGameDifficulty::Hard, 2);
	TierBonusByDifficulty.Add(ERTSGameDifficulty::Brutal, 3);
	TierBonusByDifficulty.Add(ERTSGameDifficulty::Ironman, 4);

	CargoMultiplierByDifficulty.Add(ERTSGameDifficulty::NewToRTS, 0.9f);
	CargoMultiplierByDifficulty.Add(ERTSGameDifficulty::Normal, 1.f);
	CargoMultiplierByDifficulty.Add(ERTSGameDifficulty::Hard, 1.1f);
	CargoMultiplierByDifficulty.Add(ERTSGameDifficulty::Brutal, 1.2f);
	CargoMultiplierByDifficulty.Add(ERTSGameDifficulty::Ironman, 1.3f);

	FSurvivalEventExclusionZone Oasis;
	Oasis.Name = TEXT("Oasis");
	Oasis.Centre = OasisCentre;
	Oasis.Radius = OasisRadiusWithRim;
	Route.ExclusionZones.Add(Oasis);
}

int32 USurvivalEventSettings::GetGuardTierForRound(const int32 Round, const ERTSGameDifficulty Difficulty) const
{
	// Harder games reach each tier this many rounds earlier.
	const int32* Bonus = TierBonusByDifficulty.Find(Difficulty);
	const int32 EffectiveRound = Round + (Bonus != nullptr ? *Bonus : 0);
	int32 Tier = 0;
	for (const FSurvivalConvoyTierStep& Step : GuardTierByRound)
	{
		if (EffectiveRound >= Step.FromRound)
		{
			Tier = FMath::Max(Tier, Step.GuardTier);
		}
	}
	return Tier;
}

float USurvivalEventSettings::GetCargoMultiplier(const ERTSGameDifficulty Difficulty) const
{
	const float* Multiplier = CargoMultiplierByDifficulty.Find(Difficulty);
	return Multiplier != nullptr ? *Multiplier : 1.f;
}
