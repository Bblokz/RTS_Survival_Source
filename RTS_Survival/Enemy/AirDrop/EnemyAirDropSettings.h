// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "EnemyAirDropSettings.generated.h"

struct FEnemyAirDropUnits;

/**
 * @brief A map's tunable air drop as an asset: when it comes and how many vehicles per difficulty. Set it on a placed
 * AEnemyAirDrop (Settings Asset): the actor then takes its units and its first-drop delay from here, so the numbers
 * are tuned in the asset, not on the actor in the level. The delay counts from StartAirDrop (usually a mission stage).
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UEnemyAirDropSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UEnemyAirDropSettings();

	// Seconds from StartAirDrop to the drop.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float DelaySeconds = 90.f;

	// Each vehicle of the drop picks one of these at random.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicles")
	TArray<FEnemyPlacementUnit> VehicleOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicles", DisplayName = "Vehicles (New To RTS)",
		meta = (ClampMin = "0"))
	int32 CountNewToRTS = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicles", DisplayName = "Vehicles (Normal)",
		meta = (ClampMin = "0"))
	int32 CountNormal = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicles", DisplayName = "Vehicles (Hard)",
		meta = (ClampMin = "0"))
	int32 CountHard = 3;

	// Ironman uses this count too.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicles", DisplayName = "Vehicles (Brutal + Ironman)",
		meta = (ClampMin = "0"))
	int32 CountBrutal = 4;

	/**
	 * @brief The air drop's cumulative difficulty lists (base + extras) that give exactly the counts above.
	 * @param OutUnits Overwritten; a count lower than the previous difficulty's adds nothing.
	 */
	void BuildUnits(FEnemyAirDropUnits& OutUnits) const;
};
