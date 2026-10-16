#pragma once

#include "CoreMinimal.h"
#include "BXPConstructionRules/BXPConstructionRules.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "BuildingExpansionEnums.generated.h"

class RTS_SURVIVAL_API ABuildingExpansion;
// Defines what kind of expansion is added to the building.
UENUM(BlueprintType)
enum class EBuildingExpansionType : uint8
{
	BXT_Invalid UMETA(DisplayName = "Invalid Type Expansion"),

	// For not initialized or invalid type expansions.
	//
	// Ger HQ Expansions.
	//
	BTX_GerHQRadar UMETA(DisplayName = "Ger HQ Radar Expansion"),
	BTX_GerHQPlatform UMETA(DisplayName = "Ger HQ Platform Expansion"),
	BTX_GerHQHarvester UMETA(DisplayName = "Ger HQ Harvester Expansion"),
	BTX_GerHQRepairBay UMETA(DisplayName = "Ger HQ Repair Bay Expansion"),
	BTX_GerHQTower UMETA(DisplayName = "Ger HQ Tower Expansion"),

	// Refinery Expansions.
	//
	BTX_RefConverter UMETA(DisplayName = "Refinery Converter Expansion"),

	// Barracks expansions.
	BTX_GerBarracksRadixReactor UMETA(DisplayName = "Ger Barracks Radixite Reactor Expansion"),
	BTX_GerBarrackFuelCell UMETA(DisplayName = "Ger Barracks Fuel Cell Expansion"),

	// Ger Light Tank Factory / Forge Expansions
	//
	BTX_GerLFactoryRadixiteReactor UMETA(DisplayName = "Ger Light Factory Radixite Reactor Expansion"),
	BTX_GerLFactoryFuelStorage UMETA(DisplayName = "Ger Light Factory Fuel Storage Expansion"),
	//
	// DEFENSIVE
	//
	BXT_GerFLakBunker UMETA(DisplayName = "Ger Flak Bunker Expansion"),
	BXT_88mmFlak UMETA(DisplayName = "88mm Flak Expansion"),
	BTX_20mmFlak UMETA(DisplayName = "20mm Flak Expansion"),
	BXT_QuadFlak UMETA(DisplayName = "Quad Flak Expansion"),
	BTX_37mmFlak UMETA(DisplayName = "37mm Flak Expansion"),
	BTX_Pak38 UMETA(DisplayName = "Pak 38 Expansion"),
	BTX_LeFH_150mm UMETA(DisplayName = "LeFH 150mm Expansion"),

	// Capture Bunkers
	BTX_Ger37MMCaptureBunker UMETA(DisplayName = "Ger 37MM Capture Bunker"),
	BTX_GerTurretCaptureBunker UMETA(DisplayName = "Ger Turret Capture Bunker"),


	// Rus Bunkers.
	BTX_Bunker05_WithTurrets UMETA(DisplayName = "Bunker 05 With Turret"),
	BTX_Bunker05 UMETA(DisplayName = "Bunker 05"),
	BTX_Bunker02_ZIS UMETA(DisplayName = "Bunker 02 ZIS"),
	BTX_Bunker02_204MM UMETA(DisplayName = "Bunker 02 204MM"),
	BTX_RusBunkerMG UMETA(DisplayName = "Rus Bunker MG"),
	BTX_RusBoforsPosition UMETA(DisplayName = "Rus Bofors Position"),
	BTX_RusGuardTower UMETA(DisplayName = "Rus Guard Tower"),
	BTX_RusLongCamoBunker UMETA(DisplayName = "Rus Long Camo Bunker"),
	BTX_RusDomeBunker UMETA(DisplayName = "Rus Dome Bunker"),
	BTX_RusWall,
	BTX_RusGate,

	// Rus Buildings.
	BTX_RusBarracks UMETA(DisplayName = "Rus Barracks"),
	BTX_RusFactory UMETA(DisplayName = "Rus Factory"),
	BTX_GerMarderStugFactory UMETA(DisplayName = "Ger Marder Stug Factory"),
	BTX_RusAmmoStorage UMETA(DisplayName = "Rus Ammo Storage"),
	BTX_RusResearchCenter UMETA(DisplayName = "Rus Research Center"),
	BTX_RusPlatformFactory UMETA(DisplayName = "Rus Platform Factory"),
	BTX_RusCoolingTowers UMETA(DisplayName = "Rus Cooling Towers"),

	BTX_RusFuelStorage1 UMETA(DisplayName = "Rus Fuel Storage 1"),
	BTX_RusFuelStorage2 UMETA(DisplayName = "Rus Fuel Storage 2"),
	BTX_Rus_SuppliesStorage UMETA(DisplayName = "Rus Supplies Storage"),


	//
	// ENERGY
	//
	BXT_SolarSmall UMETA(DisplayName = "Small Solar Panel Expansion"),
	BXT_SolarLarge UMETA(DisplayName = "Large Solar Panel Expansion"),

	BTX_RusIndustrialRefinery UMETA(DisplayName = "Rus Industrial Refinery"),

	// Appended to preserve the serialized values of existing BXP types.
	BTX_RusAquirfierVertical UMETA(DisplayName = "Rus Aquirfier Vertical"),
	BTX_RusAquirfier UMETA(DisplayName = "Rus Aquirfier"),
	BTX_RusWarehouse UMETA(DisplayName = "Rus Warehouse"),
	BTX_RusChemicalPlant UMETA(DisplayName = "Rus Chemical Plant"),
	BTX_RusAntenna UMETA(DisplayName = "Rus Antenna"),
	BTX_RusMegaBunker UMETA(DisplayName = "Rus Mega Bunker"),
	// Turntable gun pit with a B-4 203mm howitzer that its crew reloads with the crane.
	BTX_RusB4HowitzerPit UMETA(DisplayName = "Rus B-4 Howitzer Pit"),
	// The same pit built by the (German) player from the HQ: a captured B-4 worked by its captured crew.
	BTX_GerCapturedB4Pit UMETA(DisplayName = "Ger Captured B-4 Pit"),
	// Dug-in Soviet tank turret on a concrete pillbox in a trench line (RedLine); one type for every turret.
	BTX_RusTurretPillbox UMETA(DisplayName = "Rus Turret Pillbox"),
	// German gun pits: a low sandbag ring the Pak fires over, and a higher one shared by the two howitzers; each
	// gun turns a full circle with its crew, who hand the rounds to the loader.
	BTX_GerPak40Pit UMETA(DisplayName = "Ger Pak 40 Pit"),
	BTX_GerLeFH18Pit UMETA(DisplayName = "Ger leFH 18 Pit"),
	BTX_GerSFH18Pit UMETA(DisplayName = "Ger sFH 18 Pit"),
	// Small Soviet sandbag pit with a DShK on its tripod and a two-man crew.
	BTX_RusDShKPit UMETA(DisplayName = "Rus DShK Pit"),
	// Soviet 45mm 53-K anti-tank gun in a low ring of sandbags, several bags thick.
	BTX_Rus53KPit UMETA(DisplayName = "Rus 53-K Pit"),
	// The B-4 pit with an ML-20 152mm gun-howitzer on the turntable, loaded with the same crane drill.
	BTX_RusML20Pit UMETA(DisplayName = "Rus ML-20 Pit"),
	// Soviet 100mm BS-3 field gun in a square sandbag pit with an entrance at the rear.
	BTX_RusBS3Pit UMETA(DisplayName = "Rus BS-3 Pit"),
	// German 3.7 cm Flak 37 in a sandbag pit; a man hands the clips to the loader, the gun turns a full circle.
	BTX_GerFlak37Pit UMETA(DisplayName = "Ger Flak 37 Pit"),
	// Small Soviet sandbag pit with a 125mm Ampulomet on its tripod: a gunner and a loader who feeds the ampoules.
	BTX_RusAmpulometPit UMETA(DisplayName = "Rus Ampulomet Pit"),
	// Small square sandbag pit for the 53-K whose gun only traverses 45 degrees either way: it covers one sector.
	BTX_Rus53KSectorPit UMETA(DisplayName = "Rus 53-K Sector Pit"),
	// Small German field defenses: a Raketenwerfer 43 "Puppchen" in a small bag ring, and a Panzerschreck team in a
	// foxhole; each a gunner and a loader who carries the rocket in.
	BTX_GerPuppchenPit UMETA(DisplayName = "Ger Puppchen Pit"),
	BTX_GerPanzerschreckPit UMETA(DisplayName = "Ger Panzerschreck Foxhole"),
	// Breakthrough doctrine: a concrete pillbox whose sandbagged roof is the pit of a Pak 40 refitted with rails.
	BTX_GerRailPakBunker UMETA(DisplayName = "Ger Rail Pak Bunker"),
};

static FString Global_GetBxpTypeEnumAsString(EBuildingExpansionType BxpType)
{
	switch (BxpType)
	{
	case EBuildingExpansionType::BXT_Invalid:
		return FString("BXT_Invalid");
	case EBuildingExpansionType::BXT_88mmFlak:
		return FString("BXT_88mmFlak");
	case EBuildingExpansionType::BTX_20mmFlak:
		return FString("BTX_20mmFlak");
	case EBuildingExpansionType::BXT_QuadFlak:
		return FString("BXT_QuadFlak");
	case EBuildingExpansionType::BTX_37mmFlak:
		return FString("BTX_37mmFlak");
	case EBuildingExpansionType::BTX_RusBunkerMG:
		return FString("BTX_RusBunkerMG");
	case EBuildingExpansionType::BTX_RusGuardTower:
		return FString("BTX_RusGuardTower");
	case EBuildingExpansionType::BTX_RusBoforsPosition:
		return FString("BTX_RusBoforsPosition");
	case EBuildingExpansionType::BTX_RusBarracks:
		return FString("BTX_RusBarracks");
	case EBuildingExpansionType::BTX_RusCoolingTowers:
		return FString("BTX_RusCoolingTowers");
	case EBuildingExpansionType::BTX_RusFuelStorage1:
		return FString("BTX_RusFuelStorage1");
	case EBuildingExpansionType::BTX_RusFuelStorage2:
		return FString("BTX_RusFuelStorage2");
	case EBuildingExpansionType::BTX_Rus_SuppliesStorage:
		return FString("BTX_Rus_SuppliesStorage");
	case EBuildingExpansionType::BTX_RusPlatformFactory:
		return FString("BTX_RusPlatformFactory");
	case EBuildingExpansionType::BTX_RusFactory:
		return FString("BTX_RusFactory");
	case EBuildingExpansionType::BTX_GerMarderStugFactory:
		return FString("BTX_GerMarderStugFactory");
	case EBuildingExpansionType::BTX_RusResearchCenter:
		return FString("BTX_RusResearchCenter");
	case EBuildingExpansionType::BTX_RusIndustrialRefinery:
		return FString("BTX_RusIndustrialRefinery");
	case EBuildingExpansionType::BTX_RusAquirfierVertical:
		return FString("BTX_RusAquirfierVertical");
	case EBuildingExpansionType::BTX_RusAquirfier:
		return FString("BTX_RusAquirfier");
	case EBuildingExpansionType::BTX_RusWarehouse:
		return FString("BTX_RusWarehouse");
	case EBuildingExpansionType::BTX_RusChemicalPlant:
		return FString("BTX_RusChemicalPlant");
	case EBuildingExpansionType::BTX_RusAntenna:
		return FString("BTX_RusAntenna");
	case EBuildingExpansionType::BXT_SolarSmall:
		return FString("BXT_SolarSmall");
	case EBuildingExpansionType::BXT_SolarLarge:
		return FString("BXT_SolarLarge");
	case EBuildingExpansionType::BTX_LeFH_150mm:
		return FString("BTX_LeFH_150mm");
	case EBuildingExpansionType::BTX_Pak38:
		return FString("BTX_Pak38");
	case EBuildingExpansionType::BTX_Bunker05:
		return FString("BTX_Bunker05");
	case EBuildingExpansionType::BTX_RusAmmoStorage:
		return FString("BTX_RusAmmoStorage");
	case EBuildingExpansionType::BTX_Bunker05_WithTurrets:
		return FString("BTX_Bunker05_WithTurrets");
	case EBuildingExpansionType::BTX_Bunker02_ZIS:
		return FString("BTX_Bunker02_ZIS");
	case EBuildingExpansionType::BTX_Bunker02_204MM:
		return FString("BTX_Bunker02_204MM");
	case EBuildingExpansionType::BTX_RusDomeBunker:
		return FString("BTX_RusDomeBunker");
	case EBuildingExpansionType::BTX_RusLongCamoBunker:
		return FString("BTX_RusLongCamoBunker");
	case EBuildingExpansionType::BTX_RusMegaBunker:
		return FString("BTX_RusMegaBunker");
	case EBuildingExpansionType::BTX_RusB4HowitzerPit:
		return FString("BTX_RusB4HowitzerPit");
	case EBuildingExpansionType::BTX_GerCapturedB4Pit:
		return FString("BTX_GerCapturedB4Pit");
	case EBuildingExpansionType::BTX_RusTurretPillbox:
		return FString("BTX_RusTurretPillbox");
	case EBuildingExpansionType::BTX_GerPak40Pit:
		return FString("BTX_GerPak40Pit");
	case EBuildingExpansionType::BTX_GerLeFH18Pit:
		return FString("BTX_GerLeFH18Pit");
	case EBuildingExpansionType::BTX_GerSFH18Pit:
		return FString("BTX_GerSFH18Pit");
	case EBuildingExpansionType::BTX_RusDShKPit:
		return FString("BTX_RusDShKPit");
	case EBuildingExpansionType::BTX_Rus53KPit:
		return FString("BTX_Rus53KPit");
	case EBuildingExpansionType::BTX_RusML20Pit:
		return FString("BTX_RusML20Pit");
	case EBuildingExpansionType::BTX_RusBS3Pit:
		return FString("BTX_RusBS3Pit");
	case EBuildingExpansionType::BTX_GerFlak37Pit:
		return FString("BTX_GerFlak37Pit");
	case EBuildingExpansionType::BTX_RusAmpulometPit:
		return FString("BTX_RusAmpulometPit");
	case EBuildingExpansionType::BTX_Rus53KSectorPit:
		return FString("BTX_Rus53KSectorPit");
	case EBuildingExpansionType::BTX_GerPuppchenPit:
		return FString("BTX_GerPuppchenPit");
	case EBuildingExpansionType::BTX_GerPanzerschreckPit:
		return FString("BTX_GerPanzerschreckPit");
	case EBuildingExpansionType::BTX_GerRailPakBunker:
		return FString("BTX_GerRailPakBunker");
	case EBuildingExpansionType::BTX_GerHQRadar:
		return FString("BTX_GerHQRadar");
	case EBuildingExpansionType::BTX_GerHQPlatform:
		return FString("BTX_GerHQPlatform");
	case EBuildingExpansionType::BTX_GerHQHarvester:
		return FString("BTX_GerHQHarvester");
	case EBuildingExpansionType::BTX_GerHQRepairBay:
		return FString("BTX_GerHQRepairBay");
	case EBuildingExpansionType::BTX_GerHQTower:
		return FString("BTX_GerHQTower");
	case EBuildingExpansionType::BTX_RefConverter:
		return FString("BTX_RefConverter");

	default:
		FString EnumString = UEnum::GetValueAsString(BxpType);
		EnumString.RemoveFromStart("EBuildingExpansionType::");
		return EnumString;
	}
}


static FString Global_GetBxpDisplayString(const EBuildingExpansionType BxpType)
{
	switch (BxpType)
	{
	case EBuildingExpansionType::BTX_GerHQRadar: return TEXT("HQ Radar");
	case EBuildingExpansionType::BTX_GerHQPlatform: return TEXT("HQ Platform");
	case EBuildingExpansionType::BTX_GerHQHarvester: return TEXT("HQ Harvester Hub");
	case EBuildingExpansionType::BTX_GerHQRepairBay: return TEXT("HQ Repair Bay");
	case EBuildingExpansionType::BTX_GerHQTower: return TEXT("HQ GuardTower");

	case EBuildingExpansionType::BTX_GerBarracksRadixReactor: return TEXT("Barracks Radix Reactor");
	case EBuildingExpansionType::BTX_GerBarrackFuelCell: return TEXT("Barracks Fuel Cell");

	case EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor: return TEXT("Light Factory Radixite Reactor");
	case EBuildingExpansionType::BTX_GerLFactoryFuelStorage: return TEXT("Light Factory Fuel Storage");

	case EBuildingExpansionType::BTX_20mmFlak: return TEXT("20mm FLAK");
	case EBuildingExpansionType::BXT_GerFLakBunker: return TEXT("FLAK Bunker");
	case EBuildingExpansionType::BXT_88mmFlak: return TEXT("88mm FLAK");
	case EBuildingExpansionType::BXT_QuadFlak: return TEXT("Quad FLAK");
	case EBuildingExpansionType::BTX_37mmFlak: return TEXT("37mm FLAK");
	case EBuildingExpansionType::BTX_Pak38: return TEXT("Pak 38");
	case EBuildingExpansionType::BTX_LeFH_150mm: return TEXT("LeFH 150mm");

	case EBuildingExpansionType::BTX_Bunker05_WithTurrets: return TEXT("Bunker 05 With Turrets");
	case EBuildingExpansionType::BTX_Bunker05: return TEXT("Bunker 05");
	case EBuildingExpansionType::BTX_Bunker02_ZIS: return TEXT("Bunker 02 ZIS");
	case EBuildingExpansionType::BTX_Bunker02_204MM: return TEXT("Bunker 02 204MM");
	case EBuildingExpansionType::BTX_RusBunkerMG: return TEXT("Bunker MG");
	case EBuildingExpansionType::BTX_RusBoforsPosition: return TEXT("Bofors Position");
	case EBuildingExpansionType::BTX_RusGuardTower: return TEXT("Guard Tower");
	case EBuildingExpansionType::BTX_RusLongCamoBunker: return TEXT("Long Camo Bunker");
	case EBuildingExpansionType::BTX_RusDomeBunker: return TEXT("Dome Bunker");
	case EBuildingExpansionType::BTX_RusMegaBunker: return TEXT("Mega Bunker");
	case EBuildingExpansionType::BTX_RusB4HowitzerPit: return TEXT("B-4 Howitzer Pit");
	case EBuildingExpansionType::BTX_GerCapturedB4Pit: return TEXT("Captured B-4 Pit");
	case EBuildingExpansionType::BTX_RusTurretPillbox: return TEXT("Turret Pillbox");
	case EBuildingExpansionType::BTX_GerPak40Pit: return TEXT("Pak 40 Pit");
	case EBuildingExpansionType::BTX_GerLeFH18Pit: return TEXT("leFH 18 Pit");
	case EBuildingExpansionType::BTX_GerSFH18Pit: return TEXT("sFH 18 Pit");
	case EBuildingExpansionType::BTX_RusDShKPit: return TEXT("DShK Pit");
	case EBuildingExpansionType::BTX_Rus53KPit: return TEXT("53-K Pit");
	case EBuildingExpansionType::BTX_RusML20Pit: return TEXT("ML-20 Pit");
	case EBuildingExpansionType::BTX_RusBS3Pit: return TEXT("BS-3 Pit");
	case EBuildingExpansionType::BTX_GerFlak37Pit: return TEXT("Flak 37 Pit");
	case EBuildingExpansionType::BTX_RusAmpulometPit: return TEXT("Ampulomet Pit");
	case EBuildingExpansionType::BTX_Rus53KSectorPit: return TEXT("53-K Sector Pit");
	case EBuildingExpansionType::BTX_GerPuppchenPit: return TEXT("Puppchen Pit");
	case EBuildingExpansionType::BTX_GerPanzerschreckPit: return TEXT("Panzerschreck Foxhole");
	case EBuildingExpansionType::BTX_GerRailPakBunker: return TEXT("Rail Pak Bunker");

	case EBuildingExpansionType::BTX_RusBarracks: return TEXT("Barracks");
	case EBuildingExpansionType::BTX_RusFactory: return TEXT("Factory");
	case EBuildingExpansionType::BTX_GerMarderStugFactory: return TEXT("Marder Stug Factory");
	case EBuildingExpansionType::BTX_RusAmmoStorage: return TEXT("Ammo Storage");
	case EBuildingExpansionType::BTX_RusResearchCenter: return TEXT("Research Center");
	case EBuildingExpansionType::BTX_RusIndustrialRefinery: return TEXT("Industrial Refinery");
	case EBuildingExpansionType::BTX_RusPlatformFactory: return TEXT("Platform Factory");
	case EBuildingExpansionType::BTX_RusCoolingTowers: return TEXT("Cooling Towers");
	case EBuildingExpansionType::BTX_RusFuelStorage1: return TEXT("Fuel Storage 1");
	case EBuildingExpansionType::BTX_RusFuelStorage2: return TEXT("Fuel Storage 2");
	case EBuildingExpansionType::BTX_Rus_SuppliesStorage: return TEXT("Supplies Storage");
	case EBuildingExpansionType::BTX_RusAquirfierVertical: return TEXT("Aquirfier Vertical");
	case EBuildingExpansionType::BTX_RusAquirfier: return TEXT("Aquirfier");
	case EBuildingExpansionType::BTX_RusWarehouse: return TEXT("Warehouse");
	case EBuildingExpansionType::BTX_RusChemicalPlant: return TEXT("Chemical Plant");
	case EBuildingExpansionType::BTX_RusAntenna: return TEXT("Antenna");

	case EBuildingExpansionType::BXT_SolarSmall: return TEXT("Small Solar Panel");
	case EBuildingExpansionType::BXT_SolarLarge: return TEXT("Large Solar Panel");

	case EBuildingExpansionType::BTX_RefConverter: return TEXT("Refinery Converter");

	default:
		break;
	}

	// --- Auto-format fallback for unlisted enum values ---
	FString EnumString = UEnum::GetValueAsString(BxpType);
	EnumString.RemoveFromStart(TEXT("EBuildingExpansionType::"));

	// Strip boilerplate prefixes like BTX_, BXT_, Ger, Rus
	EnumString.RemoveFromStart(TEXT("BTX_"));
	EnumString.RemoveFromStart(TEXT("BXT_"));
	EnumString.RemoveFromStart(TEXT("Ger"));
	EnumString.RemoveFromStart(TEXT("Rus"));

	// Insert spaces before uppercase letters or digits (e.g. "HQRepairBay" → "HQ Repair Bay")
	FString Formatted;
	for (int32 i = 0; i < EnumString.Len(); ++i)
	{
		const TCHAR Char = EnumString[i];
		if (i > 0)
		{
			const TCHAR PrevChar = EnumString[i - 1];
			const bool bInsertSpace =
				(FChar::IsUpper(Char) && !FChar::IsUpper(PrevChar)) ||
				(FChar::IsDigit(Char) && !FChar::IsDigit(PrevChar));
			if (bInsertSpace)
			{
				Formatted.AppendChar(TEXT(' '));
			}
		}
		Formatted.AppendChar(Char);
	}

	return Formatted.TrimStartAndEnd();
}

// Defines the state of the building expansion.
UENUM(BlueprintType)
enum class EBuildingExpansionStatus : uint8
{
	// This expansion is not unlocked yet for the owning building.
	BXS_NotUnlocked UMETA(DisplayName = "Is Not Unlocked"),
	// The expansion is unlocked for the owning building but not built yet nor is the button for placement clicked.
	BXS_UnlockedButNotBuild UMETA(DisplayName = "Is Unlocked But Not Build"),
	// The expansion is looking for a placement location and has not started construction yet.
	BXS_LookingForPlacement UMETA(DisplayName = "Is Looking For Placement"),
	// The expansion is being built/constructed.
	BXS_BeingBuild UMETA(DisplayName = "Is Being Built/Constructed"),
	// The expansion is built and ready to be used.
	BXS_Built UMETA(DisplayName = "Is Built"),
	// The expansion is being packed up.
	BXS_BeingPackedUp UMETA(DisplayName = "Is Being Packed Up"),
	// The expansion is packed up and ready to be moved.
	BXS_PackedUp UMETA(DisplayName = "Is Packed Up"),
	// The expansion is looking for a location to unpack.
	BXS_LookingForUnpackingLocation UMETA(DisplayName = "Is Looking For Unpacking Location"),
	// The expansion is being unpacked.
	BXS_BeingUnpacked UMETA(DisplayName = "Is Being Unpacked"),
};

// Persists player conversion intent while a packed expansion has no live actor or converter component.
enum class EBuildingExpansionResourceConversionState : uint8
{
	Uninitialized,
	Enabled,
	Disabled
};

static FString Global_GetBxpStatusString(const EBuildingExpansionStatus Status)
{
	switch (Status)
	{
	case EBuildingExpansionStatus::BXS_NotUnlocked:
		return FString("BXS_NotUnlocked");
	case EBuildingExpansionStatus::BXS_UnlockedButNotBuild:
		return FString("BXS_UnlockedButNotBuild");
	case EBuildingExpansionStatus::BXS_LookingForPlacement:
		return FString("BXS_LookingForPlacement");
	case EBuildingExpansionStatus::BXS_BeingBuild:
		return FString("BXS_BeingBuild");
	case EBuildingExpansionStatus::BXS_Built:
		return FString("BXS_Built");
	case EBuildingExpansionStatus::BXS_BeingPackedUp:
		return FString("BXS_BeingPackedUp");
	case EBuildingExpansionStatus::BXS_PackedUp:
		return FString("BXS_PackedUp");
	case EBuildingExpansionStatus::BXS_LookingForUnpackingLocation:
		return FString("BXS_LookingForUnpackingLocation");
	case EBuildingExpansionStatus::BXS_BeingUnpacked:
		return FString("BXS_BeingUnpacked");
	}
	return FString("BXS_Invalid");
}

USTRUCT()
struct RTS_SURVIVAL_API FBuildingExpansionItem
{
	GENERATED_BODY()

	UPROPERTY()
	ABuildingExpansion* Expansion = nullptr;

	EBuildingExpansionType ExpansionType = EBuildingExpansionType::BXT_Invalid;

	EBuildingExpansionStatus ExpansionStatus = EBuildingExpansionStatus::BXS_UnlockedButNotBuild;

	// Originally saved in the bxp options on the owner they are propagated on option click
	// to the async spawner and once the async bxp spawn is complete they are propagated to the bxp owner to save
	// for a specific item.
	FBxpConstructionRules BxpConstructionRules;

	EBuildingExpansionResourceConversionState ResourceConversionState =
		EBuildingExpansionResourceConversionState::Uninitialized;
};
