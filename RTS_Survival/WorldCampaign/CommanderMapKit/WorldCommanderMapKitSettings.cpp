// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitSettings.h"

#define LOCTEXT_NAMESPACE "WorldCommanderMapKit"

// PLACEHOLDER VALUES: every number below comes from the unreviewed proposal in
// Docs/Campaign/WorldMapCampaignDesign.md section 6 and awaits the owner's review. World distances are in world-map
// units and were not measured against the map.
namespace WorldCommanderMapKitPlaceholders
{
	constexpr float CheaperBuildings = 0.8f;
	constexpr float WeakenPercent = 20.f;
	constexpr int32 WeakenTurns = 1;
	constexpr int32 WeakenCooldown = 3;

	constexpr float AirfieldRange = 1.3f;
	constexpr float ReconRadius = 5000.f;
	constexpr int32 ReconCooldown = 3;

	constexpr float BatteryReduction = 1.5f;
	constexpr float FortificationsRemoved = 1.f;
	constexpr int32 BombardCooldown = 4;

	constexpr float DepotRadixite = 1.25f;
	constexpr float RadixitePerBlueprint = 100.f;
	constexpr int32 MaxBlueprintsPerConversion = 3;
	constexpr int32 ConvertCooldown = 3;

	constexpr float EnemyItemBlueprints = 1.5f;
	constexpr int32 SalvageRadixite = 150;
	constexpr int32 SalvageBlueprints = 1;
	constexpr int32 SalvageCooldown = 4;

	constexpr float DepotSupportRange = 4000.f;
	constexpr float RailStrikeDamage = 25.f;
	constexpr int32 RailStrikeCooldown = 4;

	constexpr float WatchPostRange = 1.5f;
	constexpr int32 MarkTurns = 3;
	constexpr int32 MarkCooldown = 2;

	constexpr float BarracksStrength = 0.75f;
	constexpr float BurnRadius = 5000.f;
	constexpr int32 BurnTurns = 2;
	constexpr int32 BurnCooldown = 4;

	constexpr float IgnoredFortifications = 1.f;
	constexpr float ForcedMarchSteps = 1.f;
	constexpr int32 ForcedMarchTurns = 1;
	constexpr int32 ForcedMarchCooldown = 3;

	constexpr float ExtraBuildNodes = 1.f;
	constexpr int32 FortifyTurns = 3;
	constexpr int32 FortifyCooldown = 4;

	constexpr float SideMissionRewards = 1.5f;
	constexpr float InfiltrateSquads = 1.f;
	constexpr int32 InfiltrateTurns = 1;
	constexpr int32 InfiltrateCooldown = 3;

	constexpr int32 SupplyDropRadixite = 300;
	constexpr int32 SupplyDropBlueprints = 2;
	constexpr int32 SupplyDropCooldown = 6;

	FWorldCommanderPassiveDefinition MakePassive(const EWorldCommanderPassive Type, const float Magnitude,
	                                             const EWorldCampaignBuilding Building = EWorldCampaignBuilding::None)
	{
		FWorldCommanderPassiveDefinition Passive;
		Passive.Type = Type;
		Passive.Magnitude = Magnitude;
		Passive.Building = Building;
		return Passive;
	}

	FWorldCommanderMapAbilityDefinition MakeAbility(const EWorldCommanderMapAbility Type, const float Magnitude,
	                                                const float Radius, const int32 DurationTurns,
	                                                const int32 CooldownTurns)
	{
		FWorldCommanderMapAbilityDefinition Ability;
		Ability.Type = Type;
		Ability.Magnitude = Magnitude;
		Ability.Radius = Radius;
		Ability.DurationTurns = DurationTurns;
		Ability.CooldownTurns = CooldownTurns;
		return Ability;
	}

	FWorldCommanderMapKitDefinition MakeKit(const FText& PassiveName, const FText& PassiveDescription,
	                                        const FWorldCommanderPassiveDefinition& Passive, const FText& AbilityName,
	                                        const FText& AbilityDescription,
	                                        const FWorldCommanderMapAbilityDefinition& Ability)
	{
		FWorldCommanderMapKitDefinition Kit;
		Kit.PassiveDisplayName = PassiveName;
		Kit.PassiveDescription = PassiveDescription;
		Kit.Passive = Passive;
		Kit.AbilityDisplayName = AbilityName;
		Kit.AbilityDescription = AbilityDescription;
		Kit.Ability = Ability;
		return Kit;
	}

	FWorldMapResources MakeResources(const int32 Radixite, const int32 Blueprints)
	{
		FWorldMapResources Resources;
		Resources.Radixite = Radixite;
		Resources.Blueprints = Blueprints;
		return Resources;
	}
}

FWorldCommanderMapKitTable::FWorldCommanderMapKitTable()
{
	AddFieldCommanderKits();
	AddSupportCommanderKits();
	AddAssaultCommanderKits();
}

void FWorldCommanderMapKitTable::AddFieldCommanderKits()
{
	using namespace WorldCommanderMapKitPlaceholders;
	Commanders.Add(ERTSCommander::BalancedCommander, MakeKit(
		LOCTEXT("FieldPassive", "Quartermaster's Ledger"),
		LOCTEXT("FieldPassiveDesc", "Buildings cost less."),
		MakePassive(EWorldCommanderPassive::BuildingCostMultiplier, CheaperBuildings),
		LOCTEXT("FieldAbility", "Pinning Attack"),
		LOCTEXT("FieldAbilityDesc", "Lower one target's strength for one turn."),
		MakeAbility(EWorldCommanderMapAbility::WeakenTarget, WeakenPercent, 0.f, WeakenTurns, WeakenCooldown)));

	Commanders.Add(ERTSCommander::LuftwaffeLiaison, MakeKit(
		LOCTEXT("LuftwaffePassive", "Forward Airstrips"),
		LOCTEXT("LuftwaffePassiveDesc", "Airfields have a larger range."),
		MakePassive(EWorldCommanderPassive::BuildingRangeMultiplier, AirfieldRange, EWorldCampaignBuilding::Airfield),
		LOCTEXT("LuftwaffeAbility", "Recon Flight"),
		LOCTEXT("LuftwaffeAbilityDesc", "Reveal a region."),
		MakeAbility(EWorldCommanderMapAbility::RevealRegion, 0.f, ReconRadius, 0, ReconCooldown)));

	Commanders.Add(ERTSCommander::BarrageMarshal, MakeKit(
		LOCTEXT("BarragePassive", "Registered Fire"),
		LOCTEXT("BarragePassiveDesc", "Batteries lower strength more."),
		MakePassive(EWorldCommanderPassive::BuildingStrengthReductionMultiplier, BatteryReduction,
		            EWorldCampaignBuilding::Battery),
		LOCTEXT("BarrageAbility", "Bombard"),
		LOCTEXT("BarrageAbilityDesc", "Remove one fortification from a target."),
		MakeAbility(EWorldCommanderMapAbility::RemoveFortification, FortificationsRemoved, 0.f, 0,
		            BombardCooldown)));

	FWorldCommanderPassiveDefinition HeraldPassive = MakePassive(EWorldCommanderPassive::BuildingIncomeMultiplier,
	                                                             DepotRadixite, EWorldCampaignBuilding::SupplyDepot);
	HeraldPassive.ResourceScope = EWorldMapResourceScope::RadixiteOnly;
	FWorldCommanderMapAbilityDefinition HeraldAbility = MakeAbility(
		EWorldCommanderMapAbility::ConvertRadixiteToBlueprints, RadixitePerBlueprint, 0.f, 0, ConvertCooldown);
	HeraldAbility.Resources = MakeResources(0, MaxBlueprintsPerConversion);
	Commanders.Add(ERTSCommander::RadixiteHerald, MakeKit(
		LOCTEXT("HeraldPassive", "Rich Seams"),
		LOCTEXT("HeraldPassiveDesc", "More radixite from supply depots."),
		HeraldPassive,
		LOCTEXT("HeraldAbility", "Transmute"),
		LOCTEXT("HeraldAbilityDesc", "Convert radixite to blueprints."),
		HeraldAbility));
}

void FWorldCommanderMapKitTable::AddSupportCommanderKits()
{
	using namespace WorldCommanderMapKitPlaceholders;
	FWorldCommanderPassiveDefinition ScrapPassive = MakePassive(EWorldCommanderPassive::OperationRewardMultiplier,
	                                                            EnemyItemBlueprints);
	ScrapPassive.OperationTarget = EWorldOperationTarget::EnemyItem;
	ScrapPassive.ResourceScope = EWorldMapResourceScope::BlueprintsOnly;
	// The proposal refunds part of a mission's losses; no loss data reaches the map yet, so Salvage grants a flat
	// amount until it does.
	FWorldCommanderMapAbilityDefinition ScrapAbility = MakeAbility(EWorldCommanderMapAbility::GrantResources, 0.f,
	                                                               0.f, 0, SalvageCooldown);
	ScrapAbility.Resources = MakeResources(SalvageRadixite, SalvageBlueprints);
	Commanders.Add(ERTSCommander::ScrapWarlord, MakeKit(
		LOCTEXT("ScrapPassive", "Strip the Wrecks"),
		LOCTEXT("ScrapPassiveDesc", "Extra blueprints from destroyed enemy items."),
		ScrapPassive,
		LOCTEXT("ScrapAbility", "Salvage"),
		LOCTEXT("ScrapAbilityDesc", "Recover resources from the last fight."),
		ScrapAbility));

	Commanders.Add(ERTSCommander::RailEngineer, MakeKit(
		LOCTEXT("RailPassive", "Rail Spurs"),
		LOCTEXT("RailPassiveDesc", "Supply depots also support missions in range."),
		MakePassive(EWorldCommanderPassive::BuildingSupportsMissionsInRange, DepotSupportRange,
		            EWorldCampaignBuilding::SupplyDepot),
		LOCTEXT("RailAbility", "Rail Strike"),
		LOCTEXT("RailAbilityDesc", "Strike an enemy division."),
		MakeAbility(EWorldCommanderMapAbility::DamageDivision, RailStrikeDamage, 0.f, 0, RailStrikeCooldown)));

	Commanders.Add(ERTSCommander::LightBringer, MakeKit(
		LOCTEXT("LightPassive", "Searchlights"),
		LOCTEXT("LightPassiveDesc", "Watch posts reveal further."),
		MakePassive(EWorldCommanderPassive::BuildingRangeMultiplier, WatchPostRange,
		            EWorldCampaignBuilding::WatchPost),
		LOCTEXT("LightAbility", "Mark Target"),
		LOCTEXT("LightAbilityDesc", "A target's strength is shown in full detail."),
		MakeAbility(EWorldCommanderMapAbility::RevealTargetStrength, 0.f, 0.f, MarkTurns, MarkCooldown)));

	FWorldCommanderPassiveDefinition FeuersturmPassive = MakePassive(
		EWorldCommanderPassive::EnemyItemStrengthMultiplier, BarracksStrength);
	FeuersturmPassive.EnemyItem = EMapEnemyItem::Barracks;
	Commanders.Add(ERTSCommander::Feuersturm, MakeKit(
		LOCTEXT("FeuersturmPassive", "Scorched Barracks"),
		LOCTEXT("FeuersturmPassiveDesc", "Enemy barracks nearby count for less."),
		FeuersturmPassive,
		LOCTEXT("FeuersturmAbility", "Burn the Supply Route"),
		LOCTEXT("FeuersturmAbilityDesc", "Stop enemy strength growth around a target for some turns."),
		MakeAbility(EWorldCommanderMapAbility::StallStrengthGrowth, 0.f, BurnRadius, BurnTurns, BurnCooldown)));
}

void FWorldCommanderMapKitTable::AddAssaultCommanderKits()
{
	using namespace WorldCommanderMapKitPlaceholders;
	Commanders.Add(ERTSCommander::Stahllaeufer, MakeKit(
		LOCTEXT("StahlPassive", "Steel Stride"),
		LOCTEXT("StahlPassiveDesc", "Operations ignore one fortification."),
		MakePassive(EWorldCommanderPassive::IgnoreFortifications, IgnoredFortifications),
		LOCTEXT("StahlAbility", "Forced March"),
		LOCTEXT("StahlAbilityDesc", "This turn, launch on a target one step outside the fog."),
		MakeAbility(EWorldCommanderMapAbility::ForcedMarch, ForcedMarchSteps, 0.f, ForcedMarchTurns,
		            ForcedMarchCooldown)));

	Commanders.Add(ERTSCommander::Festungsbaumeister, MakeKit(
		LOCTEXT("FestungPassive", "Fortress Plans"),
		LOCTEXT("FestungPassiveDesc", "One extra build node per country."),
		MakePassive(EWorldCommanderPassive::ExtraBuildNodesPerCountry, ExtraBuildNodes),
		LOCTEXT("FestungAbility", "Fortify"),
		LOCTEXT("FestungAbilityDesc", "A building cannot be destroyed for some turns."),
		MakeAbility(EWorldCommanderMapAbility::ProtectBuilding, 0.f, 0.f, FortifyTurns, FortifyCooldown)));

	FWorldCommanderPassiveDefinition SturmPassive = MakePassive(EWorldCommanderPassive::OperationRewardMultiplier,
	                                                            SideMissionRewards);
	SturmPassive.OperationTarget = EWorldOperationTarget::Mission;
	SturmPassive.bExcludeStoryMissions = true;
	Commanders.Add(ERTSCommander::Sturmkommandant, MakeKit(
		LOCTEXT("SturmPassive", "Raider's Share"),
		LOCTEXT("SturmPassiveDesc", "Side missions pay more."),
		SturmPassive,
		LOCTEXT("SturmAbility", "Infiltrate"),
		LOCTEXT("SturmAbilityDesc", "This turn's mission starts with extra infantry."),
		MakeAbility(EWorldCommanderMapAbility::ReinforceNextMission, InfiltrateSquads, 0.f, InfiltrateTurns,
		            InfiltrateCooldown)));

	FWorldCommanderMapAbilityDefinition SkyDropAbility = MakeAbility(EWorldCommanderMapAbility::GrantResources, 0.f,
	                                                                 0.f, 0, SupplyDropCooldown);
	SkyDropAbility.Resources = MakeResources(SupplyDropRadixite, SupplyDropBlueprints);
	Commanders.Add(ERTSCommander::SkyDropQuartermaster, MakeKit(
		LOCTEXT("SkyDropPassive", "Prefabricated Kits"),
		LOCTEXT("SkyDropPassiveDesc", "Buildings finish in the same turn."),
		MakePassive(EWorldCommanderPassive::InstantBuildings, 1.f),
		LOCTEXT("SkyDropAbility", "Supply Drop"),
		LOCTEXT("SkyDropAbilityDesc", "Resources now, on a long cooldown."),
		SkyDropAbility));
}

const FWorldCommanderMapKitTable& UWorldCommanderMapKitSettings::GetKitTable()
{
	return GetDefault<UWorldCommanderMapKitSettings>()->KitTable;
}

#undef LOCTEXT_NAMESPACE
