#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Interfaces/Commands.h"

#include "FormationData.generated.h"

class UEnemyFormationController;

USTRUCT()
struct FFormationUnitData
{
	GENERATED_BODY()


	bool IsValidFormationUnit() const;
	TWeakInterfacePtr<ICommands> Unit = nullptr;
	FVector Offset = FVector::ZeroVector;
	bool bHasReachedNextDestination = false;
	FDelegateHandle MovementCompleteHandle;
	// Projected spot the unit was last sent to on its waypoint; the unit idling far from it was not a real arrival.
	FVector M_WaypointDestination = FVector::ZeroVector;
	// Tracks the last known location used for stuck detection while moving to the current waypoint.
	FVector M_LastKnownLocation = FVector::ZeroVector;
	bool bM_HasLastKnownLocation = false;
	// Counts consecutive checks without enough progress towards the current waypoint.
	int32 StuckCounts = 0;

	bool bPreviousStuckTeleportsFailed = false;
	// Time when this unit first entered combat after reaching the current waypoint.
	float M_CombatStartTimeSeconds = -1.f;
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
	FVector AverageSpawnLocation = FVector::ZeroVector;

	// Settings used when this formation is running an attack move wave.
	FAttackMoveWaveSettings AttackMoveSettings = {};

	// Whether this formation uses attack move wave logic.
	bool bIsAttackMoveFormation = false;

	// Whether this formation uses random patrol with attack move support logic.
	bool bIsRandomPatrolWithAttackMoveFormation = false;

	// Runtime state for random patrol with attack move logic.
	struct FRandomPatrolWithAttackMoveState
	{
		// All patrol points this formation can choose from.
		TArray<FVector> M_PatrolPoints = {};

		// Config values for patrol and guarding behavior.
		FRandomPatrolWithAttackMoveSettings M_Settings = {};

		// Current patrol point index inside M_PatrolPoints.
		int32 M_CurrentPatrolPointIndex = INDEX_NONE;

		// Current guard iteration count at the active patrol point.
		int32 M_CurrentGuardIteration = 0;

		// Time in seconds when the next guard iteration is allowed.
		float M_NextGuardIterationTimeSeconds = 0.f;

		// True while guard iterations are in progress for the current patrol point.
		bool bM_IsGuardingCurrentPatrolPoint = false;
	} M_RandomPatrolWithAttackMoveState;

	// Runtime state while a random patrol formation answers an alarm
	// (UEnemyFormationController::ConvergePatrolFormationsOnLocation); the patrol state above is kept for the return.
	struct FAlarmConvergeState
	{
		// Where the formation attack-moves to.
		FVector M_ConvergeLocation = FVector::ZeroVector;

		// Seconds after the first combat before the formation breaks off and returns to its patrol loop.
		float M_CombatSecondsBeforeReturn = 0.f;

		// Seconds the formation may take to arrive while it meets no enemy; scaled by its distance at the start.
		float M_MaxSecondsWithoutCombat = 0.f;

		float M_StartTimeSeconds = -1.f;

		// Latched once: the first formation check in which any unit was in combat; negative until then.
		float M_FirstCombatTimeSeconds = -1.f;

		// The first formation check in which every unit had arrived; negative until then.
		float M_AllArrivedTimeSeconds = -1.f;

		// While true the formation check runs the convergence instead of the patrol, with unit attack-move orders.
		bool bM_IsConverging = false;
	} M_AlarmConvergeState;

	FVector GetFormationUnitLocation();

private:
	void EnsureAllFormationUnitsAreValid(UEnemyFormationController* EnemyFormationController);
	void OnReachedUnitNotInFormation(TWeakInterfacePtr<ICommands> UnitThatReached) const;
};
