#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/CardRarity/CardRarity.h"
// When adding a new type for layouts, make sure to also add a new NOmadicLayoutBuildingType in the NomadicLayoutBuilding.h
// Make sure that the new cards in the datatable have the correct CardType assigned.
UENUM(BlueprintType)
enum class ECardType : uint8
{
	Invalid UMETA(DisplayName = "Invalid"),
	StartingUnit UMETA(DisplayName = "Unit"),
	Technology UMETA(DisplayName = "Technology"),
	Resource UMETA(DisplayName = "Resource"),
	BarracksTrain UMETA(DisplayName = "BarracksTrain"),
	ForgeTrain UMETA(DisplayName = "ForgeTrain"),
	MechanicalDepotTrain UMETA(DisplayName = "MechanicalDepotTrain"),
	Empty UMETA(DisplayName = "Empty")
};

static inline FString Global_GetCardTypeAsString(const ECardType Type)
{
	switch (Type)
	{
	case ECardType::Invalid:
		return "Invalid";
	case ECardType::Empty:
		return "Empty";
	case ECardType::StartingUnit:
		return "StartingUnit";
	case ECardType::Technology:
		return "Technology";
	case ECardType::Resource:
		return "Resource";
	case ECardType::BarracksTrain:
		return "BarracksTrain";
	case ECardType::ForgeTrain:
		return "ForgeTrain";
	case ECardType::MechanicalDepotTrain:
		return "MechanicalDepotTrain";
	}
	return "Unknown";
}

// Make sure to fill out the GetCardAsString function in the cpp file.
// For Tech cards, make sure the name contains Tech.
// For Training cards, make sure the name contains Train.
// This is to ensure the profile check functions work properly.
UENUM(BlueprintType)
enum class ERTSCard : uint8
{
	Card_INVALID,
	Card_Empty,
	// Ger Unit Cards
	Card_Ger_Pz38T,
	Card_Ger_PzII_F,
	Card_Ger_Scavengers,
	Card_Ger_JagerKar98,
	Card_Ger_SteelFist,
	Card_Ger_PZII_L,
	Card_Ger_PZIII_M,
	Card_Ger_Sdkfz_140,
	Card_Ger_Hetzer,
	Card_Ger_Marder,
	Card_Ger_PzJager,
	Card_Ger_Pz1_150MM,
	Card_Ger_GammaTruck,
	Card_Ger_BarracksTruck,
	Card_Ger_ForgeTruck,
	Card_Ger_MechanicalDepotTruck,
	Card_Ger_RefineryTruck,

	// Training_Options -- Infantry
	Card_Train_Scavengers,
	Card_Train_JagerKar98,
	Card_Train_SteelFist,

	// TrainingOptions -- Light Vehicles
	Card_Train_PzII_F,
	Card_Train_Pz38T,
	Card_Train_Sdkfz_140,
	Card_Train_PzI_150MM,
	Card_Train_PzJager,
	Card_Train_Hetzer,
	Card_Train_Marder,

	// Ger Technology Cards
	Card_Ger_Tech_PzJager,
	Card_Ger_Tech_RadixiteArmor,

	// Resource Cards
	Card_Resource_Radixite,
	Card_Resource_Metal,
	Card_Resource_VehicleParts,
	Card_Blueprint_Weapon,
	Card_Blueprint_Vehicle,
	Card_Blueprint_Construction,
	Card_Blueprint_Energy,
	Card_RadixiteMetal,
	Card_RadixiteVehicleParts,
	Card_High_Radixite,
	Card_High_Metal,
	Card_High_VehicleParts,
	Card_AllBasicResources,
	Card_SuperHigh_Radixite,
	Card_SuperHigh_Metal,
	Card_SuperHigh_VehicleParts,
	Card_High_Blueprint_Weapon,
	Card_High_Blueprint_Vehicle,
	Card_High_Blueprint_Construction,
	Card_High_Blueprint_Energy,

	Card_EasterEgg
};

static FString Global_GetCardAsString(const ERTSCard Card)
{
	switch (Card)
	{
	case ERTSCard::Card_INVALID:
		return "Card_INVALID";
	case ERTSCard::Card_Empty:
		return "Card_Empty";
	// Ger Unit Cards
	case ERTSCard::Card_Ger_Pz38T:
		return "Card_Ger_Pz38T";
	case ERTSCard::Card_Ger_PzII_F:
		return "Card_Ger_PzII_F";
	case ERTSCard::Card_Ger_Scavengers:
		return "Card_Ger_Scavengers";
	case ERTSCard::Card_Ger_JagerKar98:
		return "Card_Ger_JagerKar98";
	case ERTSCard::Card_Ger_SteelFist:
		return "Card_Ger_SteelFist";
	case ERTSCard::Card_Ger_PZII_L:
		return "Card_Ger_PZII_L";
	case ERTSCard::Card_Ger_PZIII_M:
		return "Card_Ger_PZIII_M";
	case ERTSCard::Card_Ger_Sdkfz_140:
		return "Card_Ger_Sdkfz_140";
	case ERTSCard::Card_Ger_Hetzer:
		return "Card_Ger_Hetzer";
	case ERTSCard::Card_Ger_Marder:
		return "Card_Ger_Marder";
	case ERTSCard::Card_Ger_PzJager:
		return "Card_Ger_PzJager";
	case ERTSCard::Card_Ger_Pz1_150MM:
		return "Card_Ger_Pz1_150MM";
	case ERTSCard::Card_Ger_GammaTruck:
		return "Card_Ger_GammaTruck";
	case ERTSCard::Card_Ger_BarracksTruck:
		return "Card_Ger_BarracksTruck";
	case ERTSCard::Card_Ger_ForgeTruck:
		return "Card_Ger_ForgeTruck";
	case ERTSCard::Card_Ger_MechanicalDepotTruck:
		return "Card_Ger_MechanicalDepotTruck";
	case ERTSCard::Card_Ger_RefineryTruck:
		return "Card_Ger_RefineryTruck";
	// Training Options -- Infantry
	case ERTSCard::Card_Train_Scavengers:
		return "Card_Train_Scavengers";
	case ERTSCard::Card_Train_JagerKar98:
		return "Card_Train_JagerKar98";
	case ERTSCard::Card_Train_SteelFist:
		return "Card_Train_SteelFist";
	// Training Options -- Light Vehicles
	case ERTSCard::Card_Train_PzII_F:
		return "Card_Train_PzII_F";
	case ERTSCard::Card_Train_Pz38T:
		return "Card_Train_Pz38T";
	case ERTSCard::Card_Train_Sdkfz_140:
		return "Card_Train_Sdkfz_140";
	case ERTSCard::Card_Train_PzI_150MM:
		return "Card_Train_PzI_150MM";
	case ERTSCard::Card_Train_PzJager:
		return "Card_Train_PzJager";
	case ERTSCard::Card_Train_Hetzer:
		return "Card_Train_Hetzer";
	case ERTSCard::Card_Train_Marder:
		return "Card_Train_Marder";
	// Ger Technology Cards
	case ERTSCard::Card_Ger_Tech_PzJager:
		return "Card_Ger_Tech_PzJager";
	case ERTSCard::Card_Ger_Tech_RadixiteArmor:
		return "Card_Ger_Tech_RadixiteArmor";
	// Resource Cards
	case ERTSCard::Card_Resource_Radixite:
		return "Card_Resource_Radixite";
	case ERTSCard::Card_Resource_Metal:
		return "Card_Resource_Metal";
	case ERTSCard::Card_Resource_VehicleParts:
		return "Card_Resource_VehicleParts";
	case ERTSCard::Card_Blueprint_Weapon:
		return "Card_Blueprint_Weapon";
	case ERTSCard::Card_Blueprint_Vehicle:
		return "Card_Blueprint_Vehicle";
	case ERTSCard::Card_Blueprint_Construction:
		return "Card_Blueprint_Construction";
	case ERTSCard::Card_Blueprint_Energy:
		return "Card_Blueprint_Energy";
	case ERTSCard::Card_RadixiteMetal:
		return "Card_RadixiteMetal";
	case ERTSCard::Card_RadixiteVehicleParts:
		return "Card_RadixiteVehicleParts";
	case ERTSCard::Card_High_Radixite:
		return "Card_High_Radixite";
	case ERTSCard::Card_High_Metal:
		return "Card_High_Metal";
	case ERTSCard::Card_High_VehicleParts:
		return "Card_High_VehicleParts";
	case ERTSCard::Card_AllBasicResources:
		return "Card_AllBasicResources";
	case ERTSCard::Card_SuperHigh_Radixite:
		return "Card_SuperHigh_Radixite";
	case ERTSCard::Card_SuperHigh_Metal:
		return "Card_SuperHigh_Metal";
	case ERTSCard::Card_SuperHigh_VehicleParts:
		return "Card_SuperHigh_VehicleParts";
	case ERTSCard::Card_High_Blueprint_Weapon:
		return "Card_High_Blueprint_Weapon";
	case ERTSCard::Card_High_Blueprint_Vehicle:
		return "Card_High_Blueprint_Vehicle";
	case ERTSCard::Card_High_Blueprint_Construction:
		return "Card_High_Blueprint_Construction";
	case ERTSCard::Card_High_Blueprint_Energy:
		return "Card_High_Blueprint_Energy";
	case ERTSCard::Card_EasterEgg:
		return "Card_EasterEgg";
	default:
		return "Unknown Card";
	}
}
