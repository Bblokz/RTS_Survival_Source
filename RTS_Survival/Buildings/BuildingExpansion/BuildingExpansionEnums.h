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
	case EBuildingExpansionType::BTX_RusResearchCenter:
		return FString("BTX_RusResearchCenter");
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
	case EBuildingExpansionType::BTX_GerHQRadar:                return TEXT("HQ Radar");
	case EBuildingExpansionType::BTX_GerHQPlatform:             return TEXT("HQ Platform");
	case EBuildingExpansionType::BTX_GerHQHarvester:            return TEXT("HQ Harvester Hub");
	case EBuildingExpansionType::BTX_GerHQRepairBay:            return TEXT("HQ Repair Bay");
	case EBuildingExpansionType::BTX_GerHQTower:                return TEXT("HQ GuardTower");

	case EBuildingExpansionType::BTX_GerBarracksRadixReactor:   return TEXT("Barracks Radix Reactor");
	case EBuildingExpansionType::BTX_GerBarrackFuelCell:        return TEXT("Barracks Fuel Cell");

	case EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor:return TEXT("Light Factory Radixite Reactor");
	case EBuildingExpansionType::BTX_GerLFactoryFuelStorage:    return TEXT("Light Factory Fuel Storage");

	case EBuildingExpansionType::BTX_20mmFlak:                  return TEXT("20mm FLAK");
	case EBuildingExpansionType::BXT_88mmFlak:                  return TEXT("88mm FLAK");
	case EBuildingExpansionType::BXT_QuadFlak:                  return TEXT("Quad FLAK");
	case EBuildingExpansionType::BTX_37mmFlak:                  return TEXT("37mm FLAK");
	case EBuildingExpansionType::BTX_Pak38:                     return TEXT("Pak 38");
	case EBuildingExpansionType::BTX_LeFH_150mm:                return TEXT("LeFH 150mm");

	case EBuildingExpansionType::BTX_Bunker05_WithTurrets:      return TEXT("Bunker 05 With Turrets");
	case EBuildingExpansionType::BTX_Bunker05:                  return TEXT("Bunker 05");
	case EBuildingExpansionType::BTX_Bunker02_ZIS:              return TEXT("Bunker 02 ZIS");
	case EBuildingExpansionType::BTX_Bunker02_204MM:            return TEXT("Bunker 02 204MM");
	case EBuildingExpansionType::BTX_RusBunkerMG:               return TEXT("Bunker MG");
	case EBuildingExpansionType::BTX_RusBoforsPosition:         return TEXT("Bofors Position");
	case EBuildingExpansionType::BTX_RusGuardTower:             return TEXT("Guard Tower");
	case EBuildingExpansionType::BTX_RusLongCamoBunker:         return TEXT("Long Camo Bunker");
	case EBuildingExpansionType::BTX_RusDomeBunker:             return TEXT("Dome Bunker");

	case EBuildingExpansionType::BTX_RusBarracks:               return TEXT("Barracks");
	case EBuildingExpansionType::BTX_RusFactory:                return TEXT("Factory");
	case EBuildingExpansionType::BTX_RusAmmoStorage:            return TEXT("Ammo Storage");
	case EBuildingExpansionType::BTX_RusResearchCenter:         return TEXT("Research Center");
	case EBuildingExpansionType::BTX_RusPlatformFactory:        return TEXT("Platform Factory");
	case EBuildingExpansionType::BTX_RusCoolingTowers:          return TEXT("Cooling Towers");
	case EBuildingExpansionType::BTX_RusFuelStorage1:           return TEXT("Fuel Storage 1");
	case EBuildingExpansionType::BTX_RusFuelStorage2:           return TEXT("Fuel Storage 2");
	case EBuildingExpansionType::BTX_Rus_SuppliesStorage:       return TEXT("Supplies Storage");

	case EBuildingExpansionType::BXT_SolarSmall:                return TEXT("Small Solar Panel");
	case EBuildingExpansionType::BXT_SolarLarge:                return TEXT("Large Solar Panel");

	case EBuildingExpansionType::BTX_RefConverter:              return TEXT("Refinery Converter");

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
};
