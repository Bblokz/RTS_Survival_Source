#pragma once

#include "CoreMinimal.h"


class UResourceConverterComponent;
class RTS_SURVIVAL_API ANomadicVehicle;

struct FNomadicResourceConversionState
{
	// Used by FResourceConversionHelper to sync all resource conversion components of this nomadic vehicle.
	// conversion starts by default.
	bool bM_ResourceConversionEnabled = true;
};


struct FResourceConversionHelper
{
	static void OnBxpCreatedOrDestroyed(ANomadicVehicle* NomadicVehicle);
	static void OnNomadicExpanded(ANomadicVehicle* NomadicVehicle, const bool bIsExpanded);
	static void ManuallySetConvertersEnabled(const bool bEnabled, ANomadicVehicle* NomadicVehicle);

private:
	/** @return Whether we have any conversion components on this nomadic vehicles.
	 * @post All the conversion components on the bxps of the nomadic vehicle have their enabled updated
	 * as determined by the vehicle*/
	static bool UpdateAllConvertersWithConversionEnabled(ANomadicVehicle* NomadicVehicle,
	                                                     const bool bConversionEnabled);
	static void UpdateConverterAbility(ANomadicVehicle* NomadicVehicle, const bool bConversionEnabled,
	                                   const bool bHasAnyConverterComponents);
	static bool IsValidNomadic(const ANomadicVehicle* NomadicVehicle);
	static UResourceConverterComponent* GetConverter(const AActor* Owner);
	static void RemoveAllConversionAbilities(ANomadicVehicle* NomadicVehicle);
	// setter helpers.
	static void SetHasConversionEnabled(ANomadicVehicle* Nomadic, const bool bHasConversionEnabled);
	// Getter helpers.
	static bool GetHasConversionEnabled(const ANomadicVehicle* Nomadic);
	// Ability helpers.
	static void RemoveEnableConversionAbility(ANomadicVehicle* NomadicVehicle);
	static void RemoveDisableConversionAbility(ANomadicVehicle* NomadicVehicle);
};
