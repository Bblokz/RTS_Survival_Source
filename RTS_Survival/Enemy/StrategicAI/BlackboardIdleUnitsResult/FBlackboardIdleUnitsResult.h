#pragma once

#include "CoreMinimal.h"

#include "FBlackboardIdleUnitsResult.generated.h"

struct FBlackboardIdleUnitEntry;
class ASquadController;
class ATankMaster;

USTRUCT()
struct FBlackboardIdleUnitsResult
{
	GENERATED_BODY()

	// Note: assumes that the entries contain valid pointers.
	void SetupResultForPickedEntries(const TArray<FBlackboardIdleUnitEntry>& Entries);
	
	UPROPERTY()
	TArray<TObjectPtr<ATankMaster>> TankMasters;
	
	UPROPERTY()
	TArray<TObjectPtr<ASquadController>> SquadControllers;
	
};
