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

	// TrainingOptions -- Armored Cars (Mechanised Depot)
	Card_Train_Sdkfz251,
	Card_Train_Sdkfz250,
	Card_Train_Sdkfz250_37mm,
	Card_Train_Sdkfz251_PZIV,
	Card_Train_Sdkfz251_22,
	Card_Train_Puma,
	Card_Train_Panzerwerfer,
	Card_Train_Sdkfz_231,
	Card_Train_Sdkfz_232_3,

	// TrainingOptions -- Light Vehicles
	Card_Train_PzJager,
	Card_Train_PzI_Harvester,
	Card_Train_PzI_Scout,
	Card_Train_PzI_150MM,
	Card_Train_Pz38T,
	Card_Train_Pz38T_R,
	Card_Train_PzII_F,
	Card_Train_Sdkfz_140,

	// TrainingOptions -- Medium Vehicles
	Card_Train_PanzerIv,
	Card_Train_PzIII_J,
	Card_Train_PzIII_AA,
	Card_Train_PzIII_FLamm,
	Card_Train_PzIII_J_Commander,
	Card_Train_PzIII_M,
	Card_Train_PzIV_F1,
	Card_Train_PzIV_F1_Commander,
	Card_Train_PzIV_G,
	Card_Train_PzIV_H,
	Card_Train_Stug,
	Card_Train_Marder,
	Card_Train_PzIV_70,
	Card_Train_Brumbar,
	Card_Train_Hetzer,
	Card_Train_Jaguar,

	// TrainingOptions -- Heavy/Experimental Vehicles
	Card_Train_PantherD,
	Card_Train_PantherG,
	Card_Train_PanzerV_III,
	Card_Train_PanzerV_IV,
	Card_Train_PantherII,
	Card_Train_KeugelT38,
	Card_Train_JagdPanther,
	Card_Train_SturmTiger,
	Card_Train_Tiger,
	Card_Train_TigerH1,
	Card_Train_KingTiger,
	Card_Train_Tiger105,
	Card_Train_E25,
	Card_Train_JagdTiger,
	Card_Train_Maus,
	Card_Train_E100,

	// TrainingOptions -- Aircraft
	Card_Train_Bf109,
	Card_Train_Ju87,
	Card_Train_Me410,

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
	// Training Options -- Armored Cars (Mechanised Depot)
	case ERTSCard::Card_Train_Sdkfz251:
		return "Card_Train_Sdkfz251";
	case ERTSCard::Card_Train_Sdkfz250:
		return "Card_Train_Sdkfz250";
	case ERTSCard::Card_Train_Sdkfz250_37mm:
		return "Card_Train_Sdkfz250_37mm";
	case ERTSCard::Card_Train_Sdkfz251_PZIV:
		return "Card_Train_Sdkfz251_PZIV";
	case ERTSCard::Card_Train_Sdkfz251_22:
		return "Card_Train_Sdkfz251_22";
	case ERTSCard::Card_Train_Puma:
		return "Card_Train_Puma";
	case ERTSCard::Card_Train_Panzerwerfer:
		return "Card_Train_Panzerwerfer";
	case ERTSCard::Card_Train_Sdkfz_231:
		return "Card_Train_Sdkfz_231";
	case ERTSCard::Card_Train_Sdkfz_232_3:
		return "Card_Train_Sdkfz_232_3";
	// Training Options -- Light Vehicles
	case ERTSCard::Card_Train_PzJager:
		return "Card_Train_PzJager";
	case ERTSCard::Card_Train_PzI_Harvester:
		return "Card_Train_PzI_Harvester";
	case ERTSCard::Card_Train_PzI_Scout:
		return "Card_Train_PzI_Scout";
	case ERTSCard::Card_Train_PzII_F:
		return "Card_Train_PzII_F";
	case ERTSCard::Card_Train_PzI_150MM:
		return "Card_Train_PzI_150MM";
	case ERTSCard::Card_Train_Pz38T:
		return "Card_Train_Pz38T";
	case ERTSCard::Card_Train_Pz38T_R:
		return "Card_Train_Pz38T_R";
	case ERTSCard::Card_Train_Sdkfz_140:
		return "Card_Train_Sdkfz_140";
	// Training Options -- Medium Vehicles
	case ERTSCard::Card_Train_PanzerIv:
		return "Card_Train_PanzerIv";
	case ERTSCard::Card_Train_PzIII_J:
		return "Card_Train_PzIII_J";
	case ERTSCard::Card_Train_PzIII_AA:
		return "Card_Train_PzIII_AA";
	case ERTSCard::Card_Train_PzIII_FLamm:
		return "Card_Train_PzIII_FLamm";
	case ERTSCard::Card_Train_PzIII_J_Commander:
		return "Card_Train_PzIII_J_Commander";
	case ERTSCard::Card_Train_PzIII_M:
		return "Card_Train_PzIII_M";
	case ERTSCard::Card_Train_PzIV_F1:
		return "Card_Train_PzIV_F1";
	case ERTSCard::Card_Train_PzIV_F1_Commander:
		return "Card_Train_PzIV_F1_Commander";
	case ERTSCard::Card_Train_PzIV_G:
		return "Card_Train_PzIV_G";
	case ERTSCard::Card_Train_PzIV_H:
		return "Card_Train_PzIV_H";
	case ERTSCard::Card_Train_Stug:
		return "Card_Train_Stug";
	case ERTSCard::Card_Train_Marder:
		return "Card_Train_Marder";
	case ERTSCard::Card_Train_PzIV_70:
		return "Card_Train_PzIV_70";
	case ERTSCard::Card_Train_Brumbar:
		return "Card_Train_Brumbar";
	case ERTSCard::Card_Train_Hetzer:
		return "Card_Train_Hetzer";
	case ERTSCard::Card_Train_Jaguar:
		return "Card_Train_Jaguar";
	// Training Options -- Heavy/Experimental Vehicles
	case ERTSCard::Card_Train_PantherD:
		return "Card_Train_PantherD";
	case ERTSCard::Card_Train_PantherG:
		return "Card_Train_PantherG";
	case ERTSCard::Card_Train_PanzerV_III:
		return "Card_Train_PanzerV_III";
	case ERTSCard::Card_Train_PanzerV_IV:
		return "Card_Train_PanzerV_IV";
	case ERTSCard::Card_Train_PantherII:
		return "Card_Train_PantherII";
	case ERTSCard::Card_Train_KeugelT38:
		return "Card_Train_KeugelT38";
	case ERTSCard::Card_Train_JagdPanther:
		return "Card_Train_JagdPanther";
	case ERTSCard::Card_Train_SturmTiger:
		return "Card_Train_SturmTiger";
	case ERTSCard::Card_Train_Tiger:
		return "Card_Train_Tiger";
	case ERTSCard::Card_Train_TigerH1:
		return "Card_Train_TigerH1";
	case ERTSCard::Card_Train_KingTiger:
		return "Card_Train_KingTiger";
	case ERTSCard::Card_Train_Tiger105:
		return "Card_Train_Tiger105";
	case ERTSCard::Card_Train_E25:
		return "Card_Train_E25";
	case ERTSCard::Card_Train_JagdTiger:
		return "Card_Train_JagdTiger";
	case ERTSCard::Card_Train_Maus:
		return "Card_Train_Maus";
	case ERTSCard::Card_Train_E100:
		return "Card_Train_E100";
	// Training Options -- Aircraft
	case ERTSCard::Card_Train_Bf109:
		return "Card_Train_Bf109";
	case ERTSCard::Card_Train_Ju87:
		return "Card_Train_Ju87";
	case ERTSCard::Card_Train_Me410:
		return "Card_Train_Me410";
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
