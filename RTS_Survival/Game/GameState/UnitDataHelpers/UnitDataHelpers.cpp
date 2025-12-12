#include "UnitDataHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

using namespace DeveloperSettings::GameBalance;

// ---------------------- Infantry ----------------------

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetResistancePresetOfType(
	const EResistancePresetType PresetType, const float MaxHealth)
{
	switch (PresetType)
	{
	case EResistancePresetType::BasicInfantryArmor:
		return GetIBasicInfantryResistances(MaxHealth);
	case EResistancePresetType::HeavyInfantryArmor:
		return GetIHeavyInfantryArmorResistances(MaxHealth);
	case EResistancePresetType::CommandoInfantryArmor:
		return GetCommandoInfantryArmorResistances(MaxHealth);
	case EResistancePresetType::HazmatInfantryArmor:
		return GetIHazmatInfantryResistances(MaxHealth);
	case EResistancePresetType::ArmoredCar:
		return GetIArmoredCarResistances(MaxHealth);
	case EResistancePresetType::LightArmor:
		return GetILightArmorResistances(MaxHealth);
	case EResistancePresetType::MediumArmor:
		return GetIMediumArmorResistances(MaxHealth);
	case EResistancePresetType::HeavyArmor:
		return GetIHeavyArmorResistances(MaxHealth);
	case EResistancePresetType::BasicBuildingArmor:
		return GetIBasicBuildingArmorResistances(MaxHealth);
	case EResistancePresetType::ReinforcedBuildingArmor:
		return GetIReinforcedArmorResistances(MaxHealth);
	default:
		RTSFunctionLibrary::ReportError(
			"FUnitResistanceDataHelpers::GetResistancePresetOfType: No matching preset found for given EResistancePresetType"
			"\n Type as string: " + UEnum::GetValueAsString(PresetType));
	// Return a default-constructed FResistanceAndDamageReductionData if no match is found
		return FResistanceAndDamageReductionData();
	}
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIBasicInfantryResistances(const float MaxHealth)
{
	using namespace UnitResistances::BasicInfantry;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::HeavyInfantryArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetCommandoInfantryArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::CommandoInfantryArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIHazmatInfantryResistances(const float MaxHealth)
{
	using namespace UnitResistances::HazmatInfantry;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetArmoredIHazmatInfantryResistances(
	const float MaxHealth)
{
	using namespace UnitResistances::ArmoredHazmatInfantry;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

// ---------------------- Vehicles ----------------------

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIArmoredCarResistances(const float MaxHealth)
{
	using namespace UnitResistances::ArmoredCar;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetILightArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::LightArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIMediumArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::MediumArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIHeavyArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::HeavyArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::SuperHeavyArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

// ---------------------- Buildings ----------------------

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::BasicBuildingArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(const float MaxHealth)
{
	using namespace UnitResistances::ReinforcedArmor;
	FResistanceAndDamageReductionData Resistances;
	Resistances.LaserAndRadiationDamageMlt = LaserRadiationMlt;
	Resistances.FireResistanceData.MaxFireThreshold = MaxHealth * FireThresholdMaxHPMlt;
	Resistances.FireResistanceData.FireRecoveryPerSecond = FireRecoveryPerSec;
	Resistances.TargetTypeIcon = TargetTypeIcon;
	Resistances.AllDamageReductionSettings = DamageReduction;
	return Resistances;
}

FResistanceAndDamageReductionData FUnitResistanceDataHelpers::GetUnitResistanceData(
	const AActor* Unit, bool& bOutFoundValidData)
{
	FResistanceAndDamageReductionData Data;
	bOutFoundValidData = false;
	if (not IsValid(Unit))
	{
		return Data;
	}
	URTSComponent* RTSComp = Unit->FindComponentByClass<URTSComponent>();
	if (not RTSComp)
	{
		return Data;
	}
	ACPPGameState* GS = Unit->GetWorld()->GetGameState<ACPPGameState>();
	if (not GS)
	{
		return Data;
	}
	switch (RTSComp->GetUnitTrainingOption().UnitType)
	{
	case EAllUnitType::UNType_None:
		return Data;
	case EAllUnitType::UNType_Squad:
		break;
	// Falls Through.
	case EAllUnitType::UNType_Harvester:
	case EAllUnitType::UNType_Tank:
		Data = GS->GetTankDataOfPlayer(1, RTSComp->GetSubtypeAsTankSubtype()).ResistancesAndDamageMlt;
		bOutFoundValidData = true;
		break;
	case EAllUnitType::UNType_Nomadic:
		{
			bool bIsExpanded = false;
			if (const ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Unit))
			{
				bIsExpanded = not Nomadic->GetIsInTruckMode();
			}
			auto NomadicData = GS->GetNomadicDataOfPlayer(1, RTSComp->GetSubtypeAsNomadicSubtype());
			Data = bIsExpanded
				       ? NomadicData.Building_ResistancesAndDamageMlt
				       : NomadicData.Vehicle_ResistancesAndDamageMlt;
			bOutFoundValidData = true;
		}
		break;
	case EAllUnitType::UNType_BuildingExpansion:
		Data = GS->GetPlayerBxpData(RTSComp->GetSubtypeAsBxpSubtype()).ResistancesAndDamageMlt;
		bOutFoundValidData = true;
		break;
	case EAllUnitType::UNType_Aircraft:
		Data = GS->GetAircraftDataOfPlayer(RTSComp->GetSubtypeAsAircraftSubtype(), 1).ResistancesAndDamageMlt;
		bOutFoundValidData = true;
		break;
	}
	return Data;
}
