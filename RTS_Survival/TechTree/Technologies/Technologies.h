#pragma once

#include "CoreMinimal.h"

#include "Technologies.generated.h"
// How to add a new technology:
// 1. Create a new enum value in ETechnology and define a matching name in GLOBAL_GetTechAsString.
// 2. Create a new class that inherits from UTechnologyEffect and implement the ApplyTechnologyEffect method,
// the OnApplyEffectToActor method and the GetTargetActorClasses method.
// 3. Add a new data entry in the DT_[faction]_Technologies datatable.
// 4. Derive a TEBP_Technology from the created cpp class and setup the properties.
// 5. VERY IMPORTANT: in the blueprint player controller class, add a new entry to the map from ETechnology to TEBP_Technology.
UENUM(Blueprintable, BlueprintType)
enum class ETechnology: uint8
{
	Tech_NONE,
	Tech_PzJager,
	Tech_LightCalibreApcr,
	Tech_LightTank_RadixiteArmor,
	Tech_LightTank_TrackImprovements,
	Tech_Barricades,
	Tech_LongRangeCalibration,
};

static FString Global_GetTechAsString(const ETechnology Tech)
{
	switch (Tech)
	{
	case ETechnology::Tech_PzJager:
		return "Tech_PzJager";
	case ETechnology::Tech_LightCalibreApcr:
		return "Tech_LightCalibreApcr";
	case ETechnology::Tech_LightTank_RadixiteArmor:
		return "Tech_LightTank_RadixiteArmor";
	case ETechnology::Tech_LightTank_TrackImprovements:
		return "Tech_LightTank_TrackImprovements";
	case ETechnology::Tech_Barricades:
		return "Tech_Barricades";
	case ETechnology::Tech_LongRangeCalibration:
		return "Tech_LongRangeCalibration";
	default: break;
	}
	return "Tech_NONE";
}

static FString Global_GetTechDisplayName(const ETechnology Tech)
{
	switch (Tech)
	{
		case ETechnology::Tech_PzJager:
			return "PzJager";
		case ETechnology::Tech_LightCalibreApcr:
			return "Light Calibre APCR";
		case ETechnology::Tech_LightTank_RadixiteArmor:
			return "Light Tank Radixite Armor";
		case ETechnology::Tech_LightTank_TrackImprovements:
			return "Light Tank Track Improvements";
		case ETechnology::Tech_Barricades:
			return "Barricades";
		case ETechnology::Tech_LongRangeCalibration:
			return "Long Range Calibration";
		default:
			return "None";
	}
}
