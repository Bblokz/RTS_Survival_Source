// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"

#include "Engine/Texture2D.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechWheel.h"

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

namespace WorldTechWheelEffectDefaults
{
	const TCHAR* const EffectFolder = TEXT("/Game/RTS_Survival/Blueprints/TechTree/GerTechs/");
	const TCHAR* const UnitClassSpawnerPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner.RTS_BP_RTSAsyncSpawner_C");

	struct FEffectAsset
	{
		ETechnology Technology;
		// Folder and asset name under EffectFolder.
		const TCHAR* AssetPath;
	};

	// Panzerjaeger and Long Range Calibration have no effect asset yet.
	const FEffectAsset EffectAssets[] = {
		{ETechnology::Tech_LightCalibreApcr, TEXT("AutoCannonAPCR/TEBP_Ger_AutoCannon_APCR_Rounds")},
		{ETechnology::Tech_LightTank_RadixiteArmor, TEXT("LightTankRadixite/TEBP_Ger_LightRadixiteArmor")},
		{ETechnology::Tech_LightTank_TrackImprovements, TEXT("LightTankTracks/TEBP_Ger_LightTracks")},
		{ETechnology::Tech_Barricades, TEXT("Barricades/TEBP_Ger_Barriers")},
		{ETechnology::Tech_Spalliners, TEXT("Spall_Liners/TEBP_Ger_Spall_Liners")},
		{ETechnology::Tech_AutoLoader, TEXT("AutoLoader/TEBP_Autoloader")},
		{ETechnology::Tech_ScavengingSpeed, TEXT("ScavengerUpgrades/TEBP_Ger_ScavengingSpeed")},
		{ETechnology::Tech_ScavengingReward, TEXT("ScavengerUpgrades/TEBP_Ger_ScavReward")},
		{ETechnology::Tech_HarvesterCapacity, TEXT("HarvesterUpgrades/TEBP_HarvesterCargo")},
		{ETechnology::Tech_HarvesterSpeed, TEXT("HarvesterUpgrades/TEBP_HarvestSpeed")},
		{ETechnology::Tech_RepairUpgrade, TEXT("RepairUpgrade/TEBP_Ger_RepairUpgrade")},
		{ETechnology::Tech_AutoCannonDamage, TEXT("AutoCannonDmg/TEBP_Ger_AutoCannonDamage")},
		{ETechnology::Tech_FlameRange, TEXT("FlameTechs/BPTE_Ger_FlameRange")},
		{ETechnology::Tech_FlameDamage, TEXT("FlameTechs/BPTE_Ger_FlameDamageColor")},
		{ETechnology::Tech_Tungsten88, TEXT("Tungsten88/TEBP_Ger_Tungsten88")},
		{ETechnology::Tech_StereoRangefinder, TEXT("StereoRangefinder/TEBP_Ger_StereoRangefinder")},
		{ETechnology::Tech_Assault_ShellHoist, TEXT("ShellHoist/TEBP_Ger_ShellHoist")},
		{ETechnology::Tech_HalfTrack_ArmoredBay, TEXT("ArmoredTroopBay/TEBP_Ger_ArmoredTroopBay")},
		{ETechnology::Tech_Flak88_GroundRole, TEXT("Flak88GroundRole/TEBP_Ger_Flak88GroundRole")},
		{ETechnology::Tech_Infantry_Kit, TEXT("InfantryKit/TEBP_Ger_InfantryKit")},
		{ETechnology::Tech_Sniper_Ghillie, TEXT("GhillieSuits/TEBP_Ger_GhillieSuits")},
		{ETechnology::Tech_Pioneer_Entrenching, TEXT("EntrenchingTools/TEBP_Ger_EntrenchingTools")},
	};
}

UWorldCampaignTechSettings::UWorldCampaignTechSettings()
{
	using namespace WorldTechWheelEffectDefaults;
	for (const FEffectAsset& EffectAsset : EffectAssets)
	{
		const FString AssetName = FPaths::GetCleanFilename(EffectAsset.AssetPath);
		const FString ClassPath = FString(EffectFolder) + EffectAsset.AssetPath + TEXT(".") + AssetName + TEXT("_C");
		WheelEffectClasses.Add(EffectAsset.Technology, TSoftClassPtr<UTechnologyEffect>(FSoftObjectPath(ClassPath)));
	}

	UnitClassSpawnerClass = TSoftClassPtr<AActor>(FSoftObjectPath(UnitClassSpawnerPath));
}

const FWorldCampaignTechTable& UWorldCampaignTechSettings::GetTechTable()
{
	return GetDefault<UWorldCampaignTechSettings>()->TechTable;
}

const FWorldTechWheel& UWorldCampaignTechSettings::GetWheel()
{
	const UWorldCampaignTechSettings* const Settings = GetDefault<UWorldCampaignTechSettings>();
	if (not Settings->bUseGeneratedDefaultWheel && Settings->CustomWheel.Nodes.Num() > 0)
	{
		return Settings->CustomWheel;
	}
	static const FWorldTechWheel GeneratedWheel = WorldCampaignTechWheel::MakeDefaultWheel();
	return GeneratedWheel;
}

const FWorldTechWheelPresentationTable& UWorldCampaignTechSettings::GetWheelPresentation()
{
	return GetDefault<UWorldCampaignTechSettings>()->WheelPresentation;
}

const FWorldTechWheelNode* FWorldTechWheel::FindNode(const FName NodeId) const
{
	if (NodeId.IsNone())
	{
		return nullptr;
	}
	return Nodes.FindByPredicate([NodeId](const FWorldTechWheelNode& Node)
	{
		return Node.NodeId == NodeId;
	});
}

const FWorldTechWheelNode* FWorldTechWheel::FindNodeByTechnology(const ETechnology Technology) const
{
	if (Technology == ETechnology::Tech_NONE)
	{
		return nullptr;
	}
	return Nodes.FindByPredicate([Technology](const FWorldTechWheelNode& Node)
	{
		return Node.GetIsTechnologyNode() && Node.Technology == Technology;
	});
}

FName FWorldTechWheel::GetStartNodeId() const
{
	const FWorldTechWheelNode* const StartNode = Nodes.FindByPredicate([](const FWorldTechWheelNode& Node)
	{
		return Node.Kind == EWorldTechWheelNodeKind::Start;
	});
	return StartNode != nullptr ? StartNode->NodeId : NAME_None;
}

int32 FWorldTechWheel::GetTechnologyNodeCount() const
{
	int32 TechnologyNodeCount = 0;
	for (const FWorldTechWheelNode& Node : Nodes)
	{
		if (Node.GetIsTechnologyNode())
		{
			TechnologyNodeCount++;
		}
	}
	return TechnologyNodeCount;
}

namespace WorldTechWheelPresentationDefaults
{
	const TCHAR* const IconFolder = TEXT("/Game/RTS_Survival/Textures/GameUI/TechTree/GerTech/");
}

FWorldTechWheelPresentationTable::FWorldTechWheelPresentationTable()
{
	// Infantry.
	AddPresentation(ETechnology::Tech_Infantry_Kit, NSLOCTEXT("WorldTechWheel", "InfantryKitName", "M42 Kit"),
	                NSLOCTEXT("WorldTechWheel", "InfantryKitText",
	                          "New kit and webbing for Jager, Panzergrenadiere, Gebirgsjager and LMG squads: +15% health."),
	                TEXT("T_TechIcon_InfantryKit"));
	AddPresentation(ETechnology::Tech_Pioneer_Entrenching,
	                NSLOCTEXT("WorldTechWheel", "EntrenchingName", "Entrenching Tools"),
	                NSLOCTEXT("WorldTechWheel", "EntrenchingText",
	                          "Sturmpioniere and scavengers build field defences 35% faster."),
	                TEXT("T_TechIcon_EntrenchingTools"));
	AddPresentation(ETechnology::Tech_Sniper_Ghillie, NSLOCTEXT("WorldTechWheel", "GhillieName", "Ghillie Suits"),
	                NSLOCTEXT("WorldTechWheel", "GhillieText", "Sniper teams and Gebirgsjager gain +33% health."),
	                TEXT("T_TechIcon_GhillieSuits"));
	AddPresentation(ETechnology::Tech_FlameRange, NSLOCTEXT("WorldTechWheel", "FlameRangeName", "Flame Range"),
	                NSLOCTEXT("WorldTechWheel", "FlameRangeText", "Flamethrowers reach further."),
	                TEXT("T_TechIcon_FlameCone"));
	AddPresentation(ETechnology::Tech_FlameDamage, NSLOCTEXT("WorldTechWheel", "FlameDamageName", "Radixite Flame"),
	                NSLOCTEXT("WorldTechWheel", "FlameDamageText",
	                          "Radixite-laced fuel burns hotter: flamethrowers deal more damage."),
	                TEXT("T_TechIcon_RadixiteFlame"));

	// Light armour.
	AddPresentation(ETechnology::Tech_LightTank_TrackImprovements,
	                NSLOCTEXT("WorldTechWheel", "TracksName", "Track Improvements"),
	                NSLOCTEXT("WorldTechWheel", "TracksText", "Light tanks drive and turn faster."),
	                TEXT("T_Ger_Tech_LightTracksImprovement"));
	AddPresentation(ETechnology::Tech_LightCalibreApcr, NSLOCTEXT("WorldTechWheel", "ApcrName", "Light APCR"),
	                NSLOCTEXT("WorldTechWheel", "ApcrText",
	                          "Light guns fire tungsten-cored shot: more armour penetration."),
	                TEXT("T_Ger_Tech_LightAPCR"));
	AddPresentation(ETechnology::Tech_LightTank_RadixiteArmor,
	                NSLOCTEXT("WorldTechWheel", "RadixiteArmourName", "Radixite Armour"),
	                NSLOCTEXT("WorldTechWheel", "RadixiteArmourText",
	                          "Radixite plates bolted onto the light tanks: more armour."),
	                TEXT("T_Ger_Tech_LightRadixite"));
	AddPresentation(ETechnology::Tech_AutoLoader, NSLOCTEXT("WorldTechWheel", "AutoLoaderName", "Autoloader"),
	                NSLOCTEXT("WorldTechWheel", "AutoLoaderText", "Fits an autoloader to the guns that take one."),
	                TEXT("T_TechIcon_AutoLoader"));
	AddPresentation(ETechnology::Tech_PzJager, NSLOCTEXT("WorldTechWheel", "PzJagerName", "Panzerj\u00e4ger"),
	                NSLOCTEXT("WorldTechWheel", "PzJagerText", "Improves the tank hunters."),
	                TEXT("T_TechIcon_PzJager"));

	// Heavy armour.
	AddPresentation(ETechnology::Tech_Spalliners, NSLOCTEXT("WorldTechWheel", "SpallName", "Spall Liners"),
	                NSLOCTEXT("WorldTechWheel", "SpallText", "Liners inside the hull protect the crew from spall."),
	                TEXT("T_TechIcon_Spall_Liners"));
	AddPresentation(ETechnology::Tech_StereoRangefinder,
	                NSLOCTEXT("WorldTechWheel", "StereoName", "Stereo Rangefinder"),
	                NSLOCTEXT("WorldTechWheel", "StereoText",
	                          "A stereoscopic rangefinder for the tank destroyers: +10% range and +15% accuracy."),
	                TEXT("T_TechIcon_StereoRangefinder"));
	AddPresentation(ETechnology::Tech_Assault_ShellHoist, NSLOCTEXT("WorldTechWheel", "ShellHoistName", "Shell Hoist"),
	                NSLOCTEXT("WorldTechWheel", "ShellHoistText",
	                          "A powered shell hoist for the heavy assault guns: the main gun reloads faster."),
	                TEXT("T_TechIcon_ShellHoist"));
	AddPresentation(ETechnology::Tech_LongRangeCalibration,
	                NSLOCTEXT("WorldTechWheel", "CalibrationName", "Long-Range Calibration"),
	                NSLOCTEXT("WorldTechWheel", "CalibrationText", "Guns are calibrated to hit at longer range."),
	                TEXT("T_TechIcon_Long-RangeCalibration"));
	AddPresentation(ETechnology::Tech_Tungsten88, NSLOCTEXT("WorldTechWheel", "TungstenName", "Tungsten 8.8"),
	                NSLOCTEXT("WorldTechWheel", "TungstenText",
	                          "Tungsten-cored rounds for the long 8.8 cm guns: better armour penetration. The cores need a "
	                          "steady supply of ore."),
	                TEXT("T_TechIcon_Tungsten88"));

	// Support (the Air branch).
	AddPresentation(ETechnology::Tech_AutoCannonDamage,
	                NSLOCTEXT("WorldTechWheel", "AutoCannonName", "Autocannon Damage"),
	                NSLOCTEXT("WorldTechWheel", "AutoCannonText", "Autocannons deal more damage."),
	                TEXT("T_TechIcon_AutoCannonDmg"));
	AddPresentation(ETechnology::Tech_Flak88_GroundRole,
	                NSLOCTEXT("WorldTechWheel", "Flak88Name", "Flak 88 Ground Role"),
	                NSLOCTEXT("WorldTechWheel", "Flak88Text",
	                          "The 8.8 cm Flak can switch to armour-piercing ground fire: +20% penetration against tanks."),
	                TEXT("T_TechIcon_Flak88GroundRole"));
	AddPresentation(ETechnology::Tech_HalfTrack_ArmoredBay,
	                NSLOCTEXT("WorldTechWheel", "TroopBayName", "Armoured Troop Bay"),
	                NSLOCTEXT("WorldTechWheel", "TroopBayText",
	                          "Half-tracks gain +20% hull armour, and their passengers bail out when one is destroyed."),
	                TEXT("T_TechIcon_ArmoredTroopBay"));
	AddPresentation(ETechnology::Tech_RepairUpgrade, NSLOCTEXT("WorldTechWheel", "RepairName", "Repair Upgrade"),
	                NSLOCTEXT("WorldTechWheel", "RepairText", "Field repairs work faster."),
	                TEXT("T_TechIcon_RepairUpgrade"));

	// Economy.
	AddPresentation(ETechnology::Tech_ScavengingSpeed,
	                NSLOCTEXT("WorldTechWheel", "ScavengingSpeedName", "Scavenging Speed"),
	                NSLOCTEXT("WorldTechWheel", "ScavengingSpeedText", "Scavengers strip wrecks faster."),
	                TEXT("T_TechIcon_FasterScavenging"));
	AddPresentation(ETechnology::Tech_HarvesterSpeed,
	                NSLOCTEXT("WorldTechWheel", "HarvesterSpeedName", "Harvester Speed"),
	                NSLOCTEXT("WorldTechWheel", "HarvesterSpeedText", "Harvesters work faster."),
	                TEXT("T_TechIcon_FasterHarvesterAnim"));
	AddPresentation(ETechnology::Tech_ScavengingReward,
	                NSLOCTEXT("WorldTechWheel", "ScavengeRewardName", "Scavenge Reward"),
	                NSLOCTEXT("WorldTechWheel", "ScavengeRewardText", "Every scavenged wreck yields more."),
	                TEXT("T_TechIcon_ScavengeReward"));
	AddPresentation(ETechnology::Tech_HarvesterCapacity,
	                NSLOCTEXT("WorldTechWheel", "HarvesterCargoName", "Harvester Cargo"),
	                NSLOCTEXT("WorldTechWheel", "HarvesterCargoText", "Harvesters carry more per trip."),
	                TEXT("T_TechIcon_HarvesterCargo"));
	AddPresentation(ETechnology::Tech_Barricades, NSLOCTEXT("WorldTechWheel", "BarricadesName", "Barricades"),
	                NSLOCTEXT("WorldTechWheel", "BarricadesText", "Base buildings are fortified: more health."),
	                TEXT("T_TechIcon_BarricadesBuildingUpgrade"));
}

void FWorldTechWheelPresentationTable::AddPresentation(const ETechnology Technology, const FText& DisplayName,
                                                       const FText& Description, const TCHAR* IconAssetName)
{
	const FString AssetName(IconAssetName);
	FWorldTechWheelTechPresentation Presentation;
	Presentation.DisplayName = DisplayName;
	Presentation.Description = Description;
	Presentation.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		WorldTechWheelPresentationDefaults::IconFolder + AssetName + TEXT(".") + AssetName));
	Technologies.Add(Technology, Presentation);
}
