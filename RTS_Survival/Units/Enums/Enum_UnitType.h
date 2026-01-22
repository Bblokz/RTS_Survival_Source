#pragma once

#include "CoreMinimal.h"
#include "Enum_UnitType.generated.h"

UENUM(BlueprintType)
enum class EAllUnitType : uint8
{
	// Only used to Display no Buttons
	UNType_None UMETA(DisplayName = "No Button Display"),
	UNType_Squad UMETA(DisplayName = "Infantry"),
	UNType_Harvester UMETA(DisplayName = "Harvester"),
	UNType_Tank UMETA(DisplayName = "Tank"),
	UNType_Nomadic UMETA(DisplayName = "Nomadic"),
	UNType_BuildingExpansion UMETA(DisplayName = "Building Expansion"),
	UNType_Aircraft UMETA(DisplayName = "Aircraft"),
};

// When adding a new subtype do not forget to update the GetNomadicSubtypeString function.
// Make sure the subtype name is also used as DataTable index matching exactly, to load the truck building/conversion
// buttons.
// IN ADDITION:
// -- Make sure a ETrainingOptions exists for this subtype.
// to load the name of the associated training option.
UENUM(BlueprintType)
enum class ENomadicSubtype : uint8
{
	Nomadic_None UMETA(DisplayName = "No Nomadic Type"),
	Nomadic_RusHq UMETA(DisplayName = "Rus HQ"),
	Nomadic_GerHq UMETA(DisplayName = "Ger HQ"),
	// Tier 1 -----------------
	Nomadic_GerRefinery UMETA(DisplayName = "Ger Refinery"),
	Nomadic_GerMetalVault UMETA(DisplayName = "Ger Metal Vault"),
	Nomadic_GerGammaFacility UMETA(DisplayName = "Ger Gamma Facility"),
	Nomadic_GerLightSteelForge UMETA(DisplayName = "Ger Light Factory"),
	Nomadic_GerBarracks UMETA(DisplayName = "Ger Barracks"),
	Nomadic_GerMechanizedDepot UMETA(DisplayName = "Ger Mechanized Depot"),
	// Tier 2 -----------------
	Nomadic_GerTrainingCentre UMETA(DisplayName = "Ger Training Centre"),
	Nomadic_GerMedTankFactory UMETA(DisplayName = "Ger Medium Tank Factory"),
	Nomadic_GerCommunicationCenter UMETA(DisplayName = "Ger Communication Center"),
	Nomadic_GerAirbase UMETA(DisplayName = "Ger Airbase"),
};


// also fill in the training option and URTSBlueprintFunctionLibrary::BP_ConvertTankSubtypeToTrainingOption
// so the subtype can be translated into a unit image.
UENUM(BlueprintType)
enum class ETankSubtype : uint8
{
	Tank_None UMETA(DisplayName = "No Tank Type"),
	// German armored cars.
	Tank_Sdkfz251 UMETA(DisplayName = "Ger ArmCar SdKfz 251"),
	Tank_Sdkfz250 UMETA(DisplayName = "Ger ArmCar SdKfz 250"),
	Tank_Sdkfz250_37mm UMETA(DisplayName = "Ger ArmCar SdKfz 250 37mm"),
	Tank_Sdkfz251_PZIV UMETA(DisplayName = "Ger ArmCar SdKfz 251 PzIV"),
	Tank_Sdkfz251_22 UMETA(DisplayName = "Ger ArmCar SdKfz 251 75mm"),
	Tank_Puma UMETA(DisplayName = "Ger ArmCar Puma"),
	Tank_Sdkfz_231 UMETA(DisplayName = "Ger SdKfz 231 8-rad 2cm"),
	Tank_Sdkfz_232_3 UMETA(DisplayName = "Ger SdKfz 232/3 8-rad 75mm Puma"),

	// German Light tanks.
	Tank_PzJager UMETA(DisplayName = "Ger Light PzJager"),
	Tank_PzI_Harvester UMETA(DisplayName = "Ger Light Pz I Harvester"),
	Tank_PzI_Scout UMETA(DisplayName = "Ger Light Pz I Scout"),
	Tank_PzI_15cm UMETA(DisplayName = "Ger Light Pz I 15cm"),
	Tank_Pz38t UMETA(DisplayName = "Ger Light Pz 38t"),
	Tank_PzII_F UMETA(DisplayName = "Ger Light Pz II F"),
	Tank_Sdkfz_140 UMETA(DisplayName = "Ger Light SdKfz 140"),
	// German Medium tanks.
	Tank_PanzerIv UMETA(DisplayName = "Ger Medium Panzer IV"),
	Tank_PzIII_J UMETA(DisplayName = "Ger Medium Pz III J"),
	Tank_PzIII_AA UMETA(DisplayName = "Ger Medium Pz III AA"),
	Tank_PzIII_FLamm UMETA(DisplayName = "Ger Medium Pz III Flamm"),
	Tank_PzIII_J_Commander UMETA(DisplayName = "Ger Medium Pz III J Commander"),
	Tank_PzIII_M UMETA(DisplayName = "Ger Medium Pz III M"),
	Tank_PzIV_F1 UMETA(DisplayName = "Ger Medium Pz IV F1"),
	Tank_PzIV_F1_Commander UMETA(DisplayName = "Ger Medium Pz IV F1 Commander"),
	Tank_PzIV_G UMETA(DisplayName = "Ger Medium Pz IV G"),
	Tank_PzIV_H UMETA(DisplayName = "Ger Medium Pz IV H"),
	Tank_Stug UMETA(DisplayName = "Ger Medium Stug"),
	Tank_Marder UMETA(DisplayName = "Ger Medium Marder"),
	Tank_PzIV_70 UMETA(DisplayName = "Ger Medium Pz IV 70"),
	Tank_Brumbar UMETA(DisplayName = "Ger Medium Brumbar"),
	Tank_Hetzer UMETA(DisplayName = "Ger Medium Hetzer"),
	Tank_Jaguar UMETA(DisplayName = "Ger Medium Jaguar 35mmAuto"),
	// German Heavy tanks.
	Tank_PantherD UMETA(DisplayName = "Ger Heavy Panther D"),
	Tank_PantherG UMETA(DisplayName = "Ger Heavy Panther G"),
	Tank_PanzerV_III UMETA(DisplayName = "Ger Heavy Panzer V III"),
	Tank_PanzerV_IV UMETA(DisplayName = "Ger Heavy Panzer V IV"),
	Tank_PantherII UMETA(DisplayName = "Ger Heavy Panther II"),
	Tank_KeugelT38 UMETA(DisplayName = "Ger Heavy Keugel T38"),
	Tank_JagdPanther UMETA(DisplayName = "Ger Heavy JagdPanther"),
	Tank_SturmTiger UMETA(DisplayName = "Ger Heavy SturmTiger"),
	Tank_Tiger UMETA(DisplayName = "Ger Heavy Tiger"),
	Tank_TigerH1 UMETA(DisplayName = "Ger Heavy Tiger H1"),
	Tank_KingTiger UMETA(DisplayName = "Ger Heavy King Tiger"),
	Tank_Tiger105 UMETA(DisplayName = "Ger Heavy Tiger 105"),
	Tank_E25 UMETA(DisplayName = "Ger Heavy E25"),
	Tank_JagdTiger UMETA(DisplayName = "Ger Heavy JagdTiger"),
	Tank_Maus UMETA(DisplayName = "Ger Heavy Maus"),
	Tank_E100 UMETA(DisplayName = "Ger Heavy E100"),
	// Russian armored cars.
	// 45 mm armored car.
	Tank_Ba12 UMETA(DisplayName = "Rus ArmCar BA-12"),
	// has 23 mm autocannon.
	Tank_Ba14 UMETA(DisplayName = "Rus ArmCar BA-14"),
	// Russian light tanks.
	Tank_BT7 UMETA(DisplayName = "Rus Light BT-7"),
	Tank_T26 UMETA(DisplayName = "Rus Light T-26"),
	Tank_BT7_4 UMETA(DisplayName = "Rus Light BT-7-4"),
	Tank_T70 UMETA(DisplayName = "Rus Light T-70"),
	// Russian medium tanks.
	Tank_T34_85 UMETA(DisplayName = "Rus Medium T-34/85"),
	Tank_T34_100 UMETA(DisplayName = "Rus Medium T-34/100"),
	Tank_T34_76 UMETA(DisplayName = "Rus Medium T-34/76"),
	Tank_T34E UMETA(DisplayName = "Rus Medium T-34E"),
	// Russian heavy tanks.
	Tank_T35 UMETA(DisplayName = "Rus Heavy T-35"),
	Tank_KV_1 UMETA(DisplayName = "Rus Heavy KV-1"),
	Tank_KV_2 UMETA(DisplayName = "Rus Heavy KV-2"),
	Tank_KV_1E UMETA(DisplayName = "Rus Heavy KV-1E"),
	Tank_T28 UMETA(DisplayName = "Rus Heavy T-28"),
	Tank_IS_1 UMETA(DisplayName = "Rus Heavy IS-1"),
	Tank_KV_IS UMETA(DisplayName = "Rus Heavy KV-IS"),
	Tank_IS_2 UMETA(DisplayName = "Rus Heavy IS-2"),
	Tank_IS_3 UMETA(DisplayName = "Rus Heavy IS-3"),
	Tank_KV_5 UMETA(DisplayName = "Rus Heavy KV-5"),
};


static bool Global_GetIsHeavyTank(const ETankSubtype TankSubtype)
{
	switch (TankSubtype)
	{
	case ETankSubtype::Tank_PantherD:
	case ETankSubtype::Tank_PantherG:
	case ETankSubtype::Tank_PanzerV_III:
	case ETankSubtype::Tank_PanzerV_IV:
	case ETankSubtype::Tank_PantherII:
	case ETankSubtype::Tank_KeugelT38:
	case ETankSubtype::Tank_JagdPanther:
	case ETankSubtype::Tank_SturmTiger:
	case ETankSubtype::Tank_Tiger:
	case ETankSubtype::Tank_TigerH1:
	case ETankSubtype::Tank_KingTiger:
	case ETankSubtype::Tank_Tiger105:
	case ETankSubtype::Tank_E25:
	case ETankSubtype::Tank_JagdTiger:
	case ETankSubtype::Tank_Maus:
	case ETankSubtype::Tank_E100:
	case ETankSubtype::Tank_T35:
	case ETankSubtype::Tank_KV_1:
	case ETankSubtype::Tank_KV_2:
	case ETankSubtype::Tank_KV_1E:
	case ETankSubtype::Tank_T28:
	case ETankSubtype::Tank_IS_1:
	case ETankSubtype::Tank_KV_IS:
	case ETankSubtype::Tank_IS_2:
	case ETankSubtype::Tank_IS_3:
	case ETankSubtype::Tank_KV_5:
		return true;
	default:
		return false;
	}
}

static bool Global_GetIsMediumTank(const ETankSubtype TankSubtype)
{
	switch (TankSubtype)
	{
	case ETankSubtype::Tank_PanzerIv:
	case ETankSubtype::Tank_PzIII_J:
	case ETankSubtype::Tank_PzIII_AA:
	case ETankSubtype::Tank_PzIII_FLamm:
	case ETankSubtype::Tank_PzIII_J_Commander:
	case ETankSubtype::Tank_PzIII_M:
	case ETankSubtype::Tank_PzIV_F1:
	case ETankSubtype::Tank_PzIV_F1_Commander:
	case ETankSubtype::Tank_PzIV_G:
	case ETankSubtype::Tank_PzIV_H:
	case ETankSubtype::Tank_Stug:
	case ETankSubtype::Tank_Marder:
	case ETankSubtype::Tank_PzIV_70:
	case ETankSubtype::Tank_Brumbar:
	case ETankSubtype::Tank_Hetzer:
	case ETankSubtype::Tank_Jaguar:
	case ETankSubtype::Tank_T34_85:
	case ETankSubtype::Tank_T34_100:
	case ETankSubtype::Tank_T34_76:
	case ETankSubtype::Tank_T34E:
		return true;
	default:
		return false;
	}
}

static bool Global_GetIsLightTank(const ETankSubtype TankSubtype)
{
	switch (TankSubtype)
	{
	case ETankSubtype::Tank_PzJager:
	case ETankSubtype::Tank_PzI_Scout:
	case ETankSubtype::Tank_PzI_15cm:
	case ETankSubtype::Tank_Pz38t:
	case ETankSubtype::Tank_PzII_F:
	case ETankSubtype::Tank_Sdkfz_140:
	case ETankSubtype::Tank_BT7:
	case ETankSubtype::Tank_T26:
	case ETankSubtype::Tank_BT7_4:
	case ETankSubtype::Tank_T70:
		return true;
	default:
		return false;
	}
}

static bool Global_GetIsArmoredCar(const ETankSubtype TankSubtype)
{
	switch (TankSubtype)
	{
	case ETankSubtype::Tank_Sdkfz251:
	case ETankSubtype::Tank_Sdkfz250:
	case ETankSubtype::Tank_Sdkfz250_37mm:
	case ETankSubtype::Tank_Sdkfz251_PZIV:
	case ETankSubtype::Tank_Sdkfz251_22:
	case ETankSubtype::Tank_Puma:
	case ETankSubtype::Tank_Sdkfz_231:
	case ETankSubtype::Tank_Sdkfz_232_3:
	case ETankSubtype::Tank_Ba12:
	case ETankSubtype::Tank_Ba14:
		return true;
	default:
		return false;
	}
}

UENUM(BlueprintType)
enum class ESquadSubtype: uint8
{
	Squad_None UMETA(DisplayName = "No Squad Type"),
	// German barracks squads.
	Squad_Ger_Scavengers UMETA(DisplayName = "Ger Scavengers"),
	Squad_Ger_JagerTruppKar98k UMETA(DisplayName = "Ger Jager Trupp Kar98k"),
	Squad_Ger_SteelFistAssaultSquad UMETA(DisplayName = "Ger Steel Fist Assault Squad"),
	Squad_Ger_LMGSquad UMETA(DisplayName = "Ger LMG Squad"),
	Squad_Ger_Gebirgsjagerin UMETA(DisplayName = "Ger Gebirgsjägerin Squad"),
	// Requires armory.
	Squad_Ger_Vultures UMETA(DisplayName = "Ger Vultures"),
	// Requires armoryl.
	Squad_Ger_SniperTeam UMETA(DisplayName = "Ger Sniper Team"),

	// German T2 Squads
	// Cyborg with fuel tanks.
	Squad_Ger_FeuerSturm UMETA(DisplayName = "Ger Feuer Sturm Squad (Cyborg with fuel tanks)"),
	// Cyborg with engineer pack.
	Squad_Ger_SturmPionieren UMETA(DisplayName = "Ger Sturm Pionieren Squad (Cyborg with engineer pack)"),
	// Cyborg with extra rocket ammo.
	Squad_Ger_PanzerGrenadiere UMETA(DisplayName = "Ger Panzer Grenadiere Squad (Cyborg with extra rocket ammo)"),

	// Cyborg with heavy armor.
	Squad_Ger_IronStorm UMETA(DisplayName = "Ger Elite Eis (Cyborg leg armor)"),
	// Dystopian with MG
	Squad_Ger_SturmKommandos UMETA(DisplayName = "Ger Elite Sturm Kommandos Squad (Dystopian with MG)"),
	// Dystopian with Laser guns
	Squad_Ger_LightBringers UMETA(DisplayName = "Ger Elite Light Bringers Squad (Dystopian with Laser guns)"),

	// Ger mech walkers,
	Squad_Ger_RedSpine UMETA(DisplayName = "Ger Red Spine Squad (Light mech walkers Etasphere01)"),
	Squad_Ger_FelsKrieger UMETA(DisplayName = "Ger Fels Krieger Squad (Medium mech walkers Etasphere07)"),
	Squad_Ger_KR_12_Dachs UMETA(DisplayName = "Ger KR-12 Dachs Squad (Heay armored Mech walkers Etasphere04"),

	// Russian barracks squads.
	Squad_Rus_HazmatEngineers UMETA(DisplayName = "Rus Hazmat Engineers"),
	Squad_Rus_Mosin UMETA(DisplayName = "Rus Mosin Squad"),
	Squad_Rus_Okhotnik UMETA(DisplayName = "Rus PTRS Squad (hunter)"),
	// Russian t2 squads.
	Squad_Rus_RedHammer UMETA(DisplayName = "Rus Red Hammer Squad (LightGreen Hazmat PPSH and armored)"),
	Squad_Rus_ToxicGuard UMETA(DisplayName = "Rus Toxic Guard Squad (LightGreen Hazmat with FlameThrower)"),
	// Russian armory unlock.
	Squad_Rus_GhostsOfStalingrad UMETA(DisplayName = "Rus Female Sniper"),
	Squad_Rus_Kvarc77 UMETA(DisplayName = "Rus Laser Cyborg Squad"),
	Squad_Rus_Tucha12T UMETA(DisplayName = "Rus Fedrov Cyborg Squad"),
	Squad_Rus_CortexOfficer UMETA(DisplayName = "Rus Cortex Officer: heavily armored cyborg"),
};

// For translating training options of aircraft into a row index for the training data it is important that the enum value
// starts with Aircraft_.
UENUM(BlueprintType)
enum class EAircraftSubtype : uint8
{
	Aircarft_None UMETA(DisplayName = "No Aircraft Type"),
	Aircraft_Bf109 UMETA(DisplayName = "Aircraft Bf 109"),
	Aircraft_Ju87 UMETA(DisplayName = "Aircraft Ju 87"),
	Aircraft_Me410 UMETA(DisplayName = "Aircraft Me 410"),
};


static FString Global_GetUnitTypeString(const EAllUnitType UnitType)
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_None:
		return FString("No Button Display");
	case EAllUnitType::UNType_Squad:
		return FString("Infantry");
	case EAllUnitType::UNType_Harvester:
		return FString("Harvester");
	case EAllUnitType::UNType_Tank:
		return FString("Tank");
	case EAllUnitType::UNType_Nomadic:
		return FString("Nomadic");
	case EAllUnitType::UNType_BuildingExpansion:
		return FString("Building Expansion");
	case EAllUnitType::UNType_Aircraft:
		return FString("Aircraft");
	default:
		return FString("Unknown Unit Type");
	}
}

static FString Global_GetNomadicDisplayName(const ENomadicSubtype NomadicSubType)
{
	switch (NomadicSubType)
	{
	case ENomadicSubtype::Nomadic_None:
		return "None";
	case ENomadicSubtype::Nomadic_RusHq:
		return "HQ";
	case ENomadicSubtype::Nomadic_GerHq:
		return "HQ";
	case ENomadicSubtype::Nomadic_GerRefinery:
		return "Refinery";
	case ENomadicSubtype::Nomadic_GerMetalVault:
		return "Metal Vault";
	case ENomadicSubtype::Nomadic_GerGammaFacility:
		return "GammaFacility";
	case ENomadicSubtype::Nomadic_GerLightSteelForge:
		return "Light Forge";
	case ENomadicSubtype::Nomadic_GerMedTankFactory:
		return "Medium Tank Factory";
	case ENomadicSubtype::Nomadic_GerCommunicationCenter:
		return "Communication Center";
	case ENomadicSubtype::Nomadic_GerBarracks:
		return "Barracks";
	case ENomadicSubtype::Nomadic_GerMechanizedDepot:
		return "Mechanized Depot";
	case ENomadicSubtype::Nomadic_GerTrainingCentre:
		return "Training Centre";
	case ENomadicSubtype::Nomadic_GerAirbase:
		return "Airbase";
		break;
	}
	return "None";
}

static FString Global_GetSquadDisplayName(const ESquadSubtype SquadSubType)
{
	switch (SquadSubType)
	{
	case ESquadSubtype::Squad_None:
		return "None";
		break;
	case ESquadSubtype::Squad_Ger_Scavengers:
		return "Scavengers";
		break;
	case ESquadSubtype::Squad_Ger_JagerTruppKar98k:
		return "Jager";
		break;
	case ESquadSubtype::Squad_Ger_Gebirgsjagerin:
		return "Gebirgsjägerin";
	case ESquadSubtype::Squad_Ger_SteelFistAssaultSquad:
		return "Steel Fist";
		break;
	case ESquadSubtype::Squad_Ger_LMGSquad:
		return "LMG Squad";
		break;
	case ESquadSubtype::Squad_Ger_Vultures:
		return "Vultures";
		break;
	case ESquadSubtype::Squad_Rus_Mosin:
		return "Mosin Squad";
	case ESquadSubtype::Squad_Ger_IronStorm:
		return "Iron Storm Squad";
	case ESquadSubtype::Squad_Rus_RedHammer:
		return "RedHammer Squad";
	case ESquadSubtype::Squad_Rus_ToxicGuard:
		return "Toxic Guard Squad";
	case ESquadSubtype::Squad_Rus_GhostsOfStalingrad:
		return "Ghost of Stalingrad";
	case ESquadSubtype::Squad_Rus_Tucha12T:
		return "Tucha 12T Squad";
	case ESquadSubtype::Squad_Rus_HazmatEngineers:
		return "Hazmat Engineers";
	case ESquadSubtype::Squad_Rus_Okhotnik:
		return "Okhotnik Squad";
	case ESquadSubtype::Squad_Rus_Kvarc77:
		return "Kvarc 77 Squad";
	case ESquadSubtype::Squad_Ger_SniperTeam:
		return "Sniper Team";
	case ESquadSubtype::Squad_Ger_FeuerSturm:
		return "Feuer Sturm Squad";
	case ESquadSubtype::Squad_Ger_SturmPionieren:
		return "Sturm Pionieren";
	case ESquadSubtype::Squad_Ger_PanzerGrenadiere:
		return "Panzer Grenadiers";
	case ESquadSubtype::Squad_Ger_SturmKommandos:
		return "Sturm Kommandos";
	case ESquadSubtype::Squad_Ger_LightBringers:
		return "Light Bringers";
	case ESquadSubtype::Squad_Rus_CortexOfficer:
		return "Cortex Officer";
	default: ;
	}
	return "Unkown squad";
}

static FString Global_GetTankDisplayName(ETankSubtype TankSubtype)
{
	// Use StaticEnum to retrieve the UEnum* for ETankSubtype
	static UEnum* EnumPtr = StaticEnum<ETankSubtype>();

	if (!EnumPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ETankSubtype enum not found!"));
		return FString("Unknown");
	}

	// Retrieve the enum name as a string (e.g., "ETankSubtype::Tank_Sdkfz251")
	FString EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(TankSubtype));

	// Define the prefix to remove (e.g., "ETankSubtype::Tank_")
	FString FullPrefix = FString::Printf(TEXT("%s::Tank_"), *EnumPtr->GetName());

	// Check if EnumName starts with the full prefix
	if (EnumName.StartsWith(FullPrefix))
	{
		EnumName.RemoveAt(0, FullPrefix.Len());
	}
	else
	{
		// Fallback: Attempt to remove only "Tank_" if the full prefix isn't found
		FString PartialPrefix = TEXT("Tank_");
		if (EnumName.StartsWith(PartialPrefix))
		{
			EnumName.RemoveAt(0, PartialPrefix.Len());
		}
	}

	return EnumName;
}

static FString Global_GetAircraftDisplayName(const EAircraftSubtype AircraftType)
{
	// Retrieve the UEnum for EAircraftType
	static UEnum* EnumPtr = StaticEnum<EAircraftSubtype>();
	if (!EnumPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("EAircraftType enum not found!"));
		return FString("Unknown");
	}

	// Special-case the "None" entry if desired
	if (AircraftType == EAircraftSubtype::Aircarft_None)
	{
		return FString("None");
	}

	// Get the raw enum entry name (e.g. "Aircraft_Bf109")
	FString EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(AircraftType));

	// Remove the full class-qualified prefix if present ("EAircraftType::Aircraft_")
	FString FullPrefix = FString::Printf(TEXT("%s::Aircraft_"), *EnumPtr->GetName());
	if (EnumName.StartsWith(FullPrefix))
	{
		EnumName.RemoveAt(0, FullPrefix.Len());
	}
	else if (EnumName.StartsWith(TEXT("Aircraft_")))
	{
		// Fallback: strip just "Aircraft_"
		EnumName.RemoveAt(0, FString(TEXT("Aircraft_")).Len());
	}

	return EnumName;
}
