#pragma once

#include "CoreMinimal.h"


#include "FormationData.generated.h"

class UEnemyFormationController;
class ICommands;

USTRUCT()
struct FFormationUnitData
{
	GENERATED_BODY()


	bool IsValidFormationUnit() const;
	TWeakInterfacePtr<ICommands> Unit = nullptr;
	FVector Offset = FVector::ZeroVector;
	bool bHasReachedNextDestination = false;
	FDelegateHandle MovementCompleteHandle;
};

USTRUCT()
struct FFormationData
{
	GENERATED_BODY()

	bool CheckIfFormationReachedCurrentWayPoint(TWeakInterfacePtr<ICommands> UnitThatReached, UEnemyFormationController* EnemyFormationController);
	TArray<FFormationUnitData> FormationUnits = {};
	TArray<FVector> FormationWaypoints = {};
	// Points from each Waypoint to the next Waypoint. For the last waypoint this is a specially provided direction.
	TArray<FRotator> FormationWaypointDirections = {};
	int32 CurrentWaypointIndex = 0;
	int32 FormationID = -1;
	FVector GetFormationUnitLocation();

private:
	void EnsureAllFormationUnitsAreValid(UEnemyFormationController* EnemyFormationController);
	void OnReachedUnitNotInFormation(TWeakInterfacePtr<ICommands> UnitThatReached) const;
};
