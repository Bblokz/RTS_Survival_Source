// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildingSettings.h"

#define LOCTEXT_NAMESPACE "WorldBuildingSettings"

namespace WorldBuildingSettingsDefaults
{
	// The enemy-side strategic support areas default to 3000 units; player support is in the same range.
	constexpr float WatchPostRevealRadius = 3000.f;
	constexpr float WatchPostRevealFalloff = 600.f;
	constexpr float AirfieldSupportRadius = 4500.f;
	constexpr int32 AirfieldStrengthReductionPercent = 15;
	constexpr float BatterySupportRadius = 3000.f;
	constexpr int32 BatteryStrengthReductionPercent = 20;
}

UWorldBuildingSettings::UWorldBuildingSettings()
{
	using namespace WorldBuildingSettingsDefaults;

	FWorldBuildingEffectDefinition WatchPost;
	WatchPost.DisplayName = LOCTEXT("WatchPostName", "Watch Post");
	WatchPost.Description = LOCTEXT("WatchPostDescription", "Reveals the map around it.");
	WatchPost.FowRevealRadius = WatchPostRevealRadius;
	WatchPost.FowRevealFalloff = WatchPostRevealFalloff;
	BuildingEffects.Add(EWorldCampaignBuilding::WatchPost, WatchPost);

	FWorldBuildingEffectDefinition SupplyDepot;
	SupplyDepot.DisplayName = LOCTEXT("SupplyDepotName", "Supply Depot");
	SupplyDepot.Description = LOCTEXT("SupplyDepotDescription",
	                                  "Delivers radixite and blueprints at the start of every turn.");
	SupplyDepot.bPaysSupplyDepotIncome = true;
	BuildingEffects.Add(EWorldCampaignBuilding::SupplyDepot, SupplyDepot);

	FWorldBuildingEffectDefinition Airfield;
	Airfield.DisplayName = LOCTEXT("AirfieldName", "Airfield");
	Airfield.Description = LOCTEXT("AirfieldDescription", "Air support weakens enemy positions in a wide radius.");
	Airfield.EnemyStrengthReductionRadius = AirfieldSupportRadius;
	Airfield.EnemyStrengthReductionPercent = AirfieldStrengthReductionPercent;
	Airfield.EnemyStrengthReductionReason = LOCTEXT("AirfieldReason", "Our air support");
	BuildingEffects.Add(EWorldCampaignBuilding::Airfield, Airfield);

	FWorldBuildingEffectDefinition Battery;
	Battery.DisplayName = LOCTEXT("BatteryName", "Artillery Battery");
	Battery.Description = LOCTEXT("BatteryDescription", "Artillery weakens enemy positions close by.");
	Battery.EnemyStrengthReductionRadius = BatterySupportRadius;
	Battery.EnemyStrengthReductionPercent = BatteryStrengthReductionPercent;
	Battery.EnemyStrengthReductionReason = LOCTEXT("BatteryReason", "Our artillery support");
	BuildingEffects.Add(EWorldCampaignBuilding::Battery, Battery);
}

const FWorldBuildingEffectDefinition* UWorldBuildingSettings::FindBuildingEffect(const EWorldCampaignBuilding Building)
{
	const UWorldBuildingSettings* BuildingSettings = Get();
	if (BuildingSettings == nullptr)
	{
		return nullptr;
	}

	return BuildingSettings->BuildingEffects.Find(Building);
}

#undef LOCTEXT_NAMESPACE
