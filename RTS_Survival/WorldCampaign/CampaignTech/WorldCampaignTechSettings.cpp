// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"

namespace WorldCampaignTechDefaults
{
	// Prices rise with the row so deeper techs cost more than their roots. Placeholder numbers until tuned.
	constexpr int32 RootRowBlueprints = 1;
	constexpr int32 BlueprintsPerRow = 1;
	constexpr int32 RootRowRadixite = 50;
	constexpr int32 RadixitePerRow = 50;
}

FWorldCampaignTechTable::FWorldCampaignTechTable()
{
	using Branch = EWorldCampaignTechBranch;

	// Infantry (barracks): squads, their kit and field work.
	AddDefinition(ETechnology::Tech_Infantry_Kit, Branch::Infantry, 0, {});
	AddDefinition(ETechnology::Tech_Pioneer_Entrenching, Branch::Infantry, 0, {});
	AddDefinition(ETechnology::Tech_Sniper_Ghillie, Branch::Infantry, 1, {ETechnology::Tech_Infantry_Kit});
	AddDefinition(ETechnology::Tech_RepairUpgrade, Branch::Infantry, 1, {ETechnology::Tech_Pioneer_Entrenching});
	AddDefinition(ETechnology::Tech_FlameRange, Branch::Infantry, 2, {});
	AddDefinition(ETechnology::Tech_FlameDamage, Branch::Infantry, 3, {ETechnology::Tech_FlameRange});

	// Light armour (light steel forge, mechanised depot): light tanks, half-tracks, tank hunters.
	AddDefinition(ETechnology::Tech_LightTank_TrackImprovements, Branch::LightArmour, 0, {});
	AddDefinition(ETechnology::Tech_LightCalibreApcr, Branch::LightArmour, 0, {});
	AddDefinition(ETechnology::Tech_LightTank_RadixiteArmor, Branch::LightArmour, 1,
	              {ETechnology::Tech_LightTank_TrackImprovements});
	AddDefinition(ETechnology::Tech_HalfTrack_ArmoredBay, Branch::LightArmour, 1, {});
	AddDefinition(ETechnology::Tech_AutoLoader, Branch::LightArmour, 2, {ETechnology::Tech_LightCalibreApcr});
	AddDefinition(ETechnology::Tech_PzJager, Branch::LightArmour, 2, {ETechnology::Tech_LightCalibreApcr});

	// Heavy armour (medium tank and experimental factories): assault guns, 88 mm guns, heavy tanks.
	AddDefinition(ETechnology::Tech_Spalliners, Branch::HeavyArmour, 0, {});
	AddDefinition(ETechnology::Tech_StereoRangefinder, Branch::HeavyArmour, 1, {ETechnology::Tech_Spalliners});
	AddDefinition(ETechnology::Tech_Assault_ShellHoist, Branch::HeavyArmour, 1, {ETechnology::Tech_Spalliners});
	AddDefinition(ETechnology::Tech_LongRangeCalibration, Branch::HeavyArmour, 2,
	              {ETechnology::Tech_StereoRangefinder});
	AddDefinition(ETechnology::Tech_Tungsten88, Branch::HeavyArmour, 2, {ETechnology::Tech_StereoRangefinder});

	// Air (airbase): no aircraft tech exists yet; the anti-air guns stand in until one does.
	AddDefinition(ETechnology::Tech_AutoCannonDamage, Branch::Air, 0, {});
	AddDefinition(ETechnology::Tech_Flak88_GroundRole, Branch::Air, 1, {ETechnology::Tech_AutoCannonDamage});

	// Economy (HQ, refinery, harvesters): resource income and base defences.
	AddDefinition(ETechnology::Tech_ScavengingSpeed, Branch::Economy, 0, {});
	AddDefinition(ETechnology::Tech_HarvesterSpeed, Branch::Economy, 0, {});
	AddDefinition(ETechnology::Tech_ScavengingReward, Branch::Economy, 1, {ETechnology::Tech_ScavengingSpeed});
	AddDefinition(ETechnology::Tech_HarvesterCapacity, Branch::Economy, 1, {ETechnology::Tech_HarvesterSpeed});
	AddDefinition(ETechnology::Tech_Barricades, Branch::Economy, 1, {});
}

void FWorldCampaignTechTable::AddDefinition(const ETechnology Technology, const EWorldCampaignTechBranch Branch,
                                            const int32 Row, const TArray<ETechnology>& Prerequisites)
{
	using namespace WorldCampaignTechDefaults;
	FWorldCampaignTechDefinition Definition;
	Definition.Branch = Branch;
	Definition.Row = Row;
	Definition.Cost.Blueprints = RootRowBlueprints + BlueprintsPerRow * Row;
	Definition.Cost.Radixite = RootRowRadixite + RadixitePerRow * Row;
	Definition.Prerequisites = Prerequisites;
	Technologies.Add(Technology, Definition);
}

const FWorldCampaignTechTable& UWorldCampaignTechSettings::GetTechTable()
{
	return GetDefault<UWorldCampaignTechSettings>()->TechTable;
}
