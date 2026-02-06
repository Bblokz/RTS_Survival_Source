#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/CardRarity/CardRarity.h"

#include "ERTSCard.generated.h"
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
	MechanicalDepotTrain UMETA(DisplayName = "MechanicalDepotTrain"),
	ForgeTrain UMETA(DisplayName = "ForgeTrain"),
	T2FactoryTrain UMETA(DisplayName = "T2FactoryTrain"),
	AirbaseTrain UMETA(DisplayName = "AirbaseTrain"),
	ExperimentalFactoryTrain UMETA(DisplayName = "ExperimentalFactoryTrain"),
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
	case ECardType::T2FactoryTrain:
		return "FactoryTrain";	
	case ECardType::AirbaseTrain:
		return "AirbaseTrain";
	case ECardType::ExperimentalFactoryTrain:
		return "ExperimentalFactoryTrain";
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
	Card_Train_FemaleSniper,
	Card_Train_Vultures,
	Card_Train_GerSniperTeam,
	Card_Train_FeuerSturm,
	Card_Train_SturmPionieren,
	Card_Train_PanzerGrenadieren,
	Card_Train_SturmKommandos,
	Card_Train_LightBringers,


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
	// Use the built-in UEnum functionality to get the string representation of the enum value
	static const UEnum* EnumPtr = StaticEnum<ERTSCard>();
	if (!EnumPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ERTSCard enum not found!"));
		return FString("Unknown Card");
	}

	// Retrieve the name of the enum value as a string
	return EnumPtr->GetNameStringByValue(static_cast<int64>(Card));
}
