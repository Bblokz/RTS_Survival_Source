#pragma once


#include "CoreMinimal.h"

#include "BuildingUISettings.generated.h"

/**
 * @brief Struct that holds the settings for the bottom panel building UI.
 */
USTRUCT(Blueprintable)
struct FBuildingUISettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 ConstructBuildingSwitchIndex = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 CancelBuildingSwitchIndex = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 ConvertToVehicleSwitchIndex = 2;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 CancelVehicleConversionSwitchIndex = 3;
};
