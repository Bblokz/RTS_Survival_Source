// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"

#include "ConvoyDefinition.generated.h"

/**
 * @brief Everything one kind of convoy is made of: trucks, cargo, guard tiers, spacing, pace and reactions.
 * Hand it with a route to AConvoy::StartConvoy; game modes pick the guard tier and a cargo multiplier.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UConvoyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UConvoyDefinition();

	// Shown in announcements ("Fuel convoy").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FText DisplayName;

	// Truck column front to back; ExtraTrucks of a tier repeat the last entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TArray<FConvoyTruckSpec> Trucks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TArray<FConvoyGuardTier> GuardTiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FConvoyCargoSettings Cargo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FConvoySpacingSettings Spacing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FConvoySpeedSettings Speed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FConvoyCombatSettings Combat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FConvoyRobustnessSettings Robustness;

	// Minimap mark that follows the lead truck while the convoy drives.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	EMinimapIconType MinimapIcon = EMinimapIconType::Warning;

	// Minimap mark of a cargo cache.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	EMinimapIconType CacheMinimapIcon = EMinimapIconType::Scavenge;

	/** @return The tier at Index, clamped to the authored tiers; nullptr when there are none. */
	const FConvoyGuardTier* GetGuardTier(int32 Index) const;

	/** @return Trucks of the column for a tier, front to back, one entry per truck. */
	TArray<FConvoyTruckSpec> GetTruckColumn(int32 TierIndex) const;

	/** @return Enemy training points of the tier's escort (the guard cost the cargo scales with). */
	int32 GetGuardTrainingPoints(int32 TierIndex) const;

	/**
	 * @brief The cargo of the whole convoy for a tier: guard cost × ResourcesPerGuardPoint × multiplier.
	 * @param TierIndex Guard tier.
	 * @param CargoMultiplier Caller scaling (difficulty, mission).
	 * @return Summed cargo over all trucks.
	 */
	FConvoyCargoValue GetTotalCargo(int32 TierIndex, float CargoMultiplier) const;

	/** @return One truck's share of the total cargo (rounded up so no truck is empty). */
	FConvoyCargoValue GetCargoPerTruck(int32 TierIndex, float CargoMultiplier) const;

	/** @return Problems that would stop a convoy from starting; empty when the definition is usable. */
	TArray<FString> Validate() const;
};
