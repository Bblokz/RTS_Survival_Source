#pragma once

#include "CoreMinimal.h"

#include "DamagedVehicleRetreat.generated.h"

class ATankMaster;
class ASquadController;

UENUM()
enum class EDamagedVehicleRetreatPhase : uint8
{
	// Reverse order issued once; the retreat behaviour shows until the vehicle arrives or stops reversing.
	Reversing,
	// Stopped at (or short of) the retreat location; waits there until repaired.
	AwaitingRepair
};

/**
 * @brief One direct control vehicle that dropped below the retreat health threshold.
 * The vehicle and its repairing hazmat stay out of the idle blackboard until the vehicle is repaired or dies.
 */
USTRUCT()
struct FDamagedVehicleRetreatEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> Vehicle;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> RepairHazmat;

	UPROPERTY()
	FVector RetreatLocation = FVector::ZeroVector;

	UPROPERTY()
	EDamagedVehicleRetreatPhase Phase = EDamagedVehicleRetreatPhase::Reversing;

	// Repair orders given to the current hazmat; a hazmat that keeps dropping the order is swapped out.
	UPROPERTY()
	int32 RepairOrdersToHazmat = 0;

	// Last hazmat that failed to take or keep the repair order; skipped when picking the next one for this vehicle.
	UPROPERTY()
	TWeakObjectPtr<ASquadController> FailedRepairHazmat;
};

/**
 * @brief Direct control's roster and active retreats for the damaged-vehicle repair flow.
 * The roster outlives idle-blackboard membership, so units given strategic orders are still checked for damage.
 */
USTRUCT()
struct FDirectControlDamagedVehicleState
{
	GENERATED_BODY()

	// Every vehicle ever registered with direct control; picked units leave the blackboard but stay here.
	UPROPERTY()
	TArray<TWeakObjectPtr<ATankMaster>> DirectControlVehicles;

	// Every hazmat squad ever registered with direct control, so busy hazmats can still be pulled for repairs.
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> DirectControlHazmats;

	UPROPERTY()
	TArray<FDamagedVehicleRetreatEntry> ActiveRetreats;

	FTimerHandle CheckTimerHandle;
};
