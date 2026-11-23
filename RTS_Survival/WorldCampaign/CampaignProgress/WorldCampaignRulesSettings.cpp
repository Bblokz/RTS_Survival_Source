// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"

namespace WorldCampaignRuleDefaults
{
	constexpr int32 StartingRadixite = 200;
	constexpr int32 StartingBlueprints = 2;
	constexpr int32 DepotRadixitePerTurn = 60;
	constexpr int32 DepotBlueprintsPerTurn = 1;
	constexpr int32 WatchPostRadixite = 100;
	constexpr int32 SupplyDepotRadixite = 150;
	constexpr int32 SupportBuildingRadixite = 250;
	constexpr int32 SupportBuildingBlueprints = 1;
}

FWorldCampaignRuleNumbers::FWorldCampaignRuleNumbers()
{
	using namespace WorldCampaignRuleDefaults;
	StartingResources.Radixite = StartingRadixite;
	StartingResources.Blueprints = StartingBlueprints;
	SupplyDepotIncomePerTurn.Radixite = DepotRadixitePerTurn;
	SupplyDepotIncomePerTurn.Blueprints = DepotBlueprintsPerTurn;

	FWorldMapResources WatchPostCost;
	WatchPostCost.Radixite = WatchPostRadixite;
	BuildingCosts.Add(EWorldCampaignBuilding::WatchPost, WatchPostCost);

	FWorldMapResources SupplyDepotCost;
	SupplyDepotCost.Radixite = SupplyDepotRadixite;
	BuildingCosts.Add(EWorldCampaignBuilding::SupplyDepot, SupplyDepotCost);

	FWorldMapResources SupportBuildingCost;
	SupportBuildingCost.Radixite = SupportBuildingRadixite;
	SupportBuildingCost.Blueprints = SupportBuildingBlueprints;
	BuildingCosts.Add(EWorldCampaignBuilding::Airfield, SupportBuildingCost);
	BuildingCosts.Add(EWorldCampaignBuilding::Battery, SupportBuildingCost);
}

const FWorldCampaignRuleNumbers& UWorldCampaignRulesSettings::GetNumbers()
{
	return GetDefault<UWorldCampaignRulesSettings>()->Numbers;
}
