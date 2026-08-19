// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttackWave.h"
#include "Components/ActorComponent.h"
#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#endif
#include "EnemyWaveController.generated.h"


class ASquadController;
class ATankMaster;
class AEnemyController;
class ARTSAsyncSpawner;
class UNavigationSystemV1;
class URTSGameInstance;

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
/**
 * @brief Value-only record correlating one completed wave iteration with its synchronously created formation.
 */
struct FEnemyAIShippingWaveFormationHandoff
{
	int32 WaveID = INDEX_NONE;
	int32 WaveIteration = 0;
	int32 FormationID = INDEX_NONE;
	int32 CreatedFormationCount = 0;
	int32 ResolvedSpawnActorCount = 0;
	int32 AcceptedSquadCount = 0;
	int32 AcceptedTankCount = 0;
	int32 AcceptedUnitCount = 0;
	int32 FormationUnitCount = 0;
	int32 RequestedWaypointCount = 0;
	int32 FormationWaypointCount = 0;
	int32 FormationPatrolPointCount = 0;
	EEnemyAIShippingFormationKind ExpectedFormationKind = EEnemyAIShippingFormationKind::Unknown;
	EEnemyAIShippingFormationKind ActualFormationKind = EEnemyAIShippingFormationKind::Unknown;
	FVector RequestedFirstDestination = FVector::ZeroVector;
	FVector RequestedFinalDestination = FVector::ZeroVector;
	bool bFormationCreated = false;
	bool bFormationKindMatches = false;
	bool bFormationUnitCountMatches = false;
};
#endif

/**
 * @brief Coordinates the spawning and iteration of enemy attack waves for the enemy controller.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyWaveController : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyWaveController();

	void StartNewAttackWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		float WaveInterval,
		float IntervalVarianceFraction,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const bool bInstantStart = false,
		AActor* WaveCreator = nullptr,
		const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings = {}, const float
		PerAffectingBuildingTimerFraction = 0.f, const float FormationOffsetMultiplier = 0
	);

	/**
	 * @brief Use when a repeating wave should pause for allied combat support before advancing.
	 * @param WaveType Determines wave logic and whether the wave requires an owning actor.
	 * @param WaveElements Defines the unit types and spawn points for this wave.
	 * @param WaveInterval Time between wave iterations.
	 * @param IntervalVarianceFraction Variance fraction applied to the wave interval.
	 * @param Waypoints Formation movement path for the spawned units.
	 * @param FinalWaypointDirection The facing direction after reaching the final waypoint.
	 * @param MaxFormationWidth The maximum width of the formation that is spawned in units next to each other.
	 * @param bInstantStart Whether to start immediately or after the first timer.
	 * @param WaveCreator The actor responsible for spawning this wave when required by the wave type.
	 * @param WaveTimerAffectingBuildings Generator buildings checked for validity that influence the wave spawn timing.
	 * @param PerAffectingBuildingTimerFraction Fraction of the wave interval added per destroyed generator building.
	 * @param FormationOffsetMultiplier Multiplier for the formation offset when spawning units in formation.
	 * @param HelpOffsetRadiusMltMax Multiplier for max help distance around allied combat units.
	 * @param HelpOffsetRadiusMltMin Multiplier for min help distance around allied combat units.
	 * @param MaxAttackTimeBeforeAdvancingToNextWayPoint Max time to wait at a waypoint before advancing while in combat.
	 * @param MaxTriesFindNavPointForHelpOffset Attempts per tick to find a valid help location.
	 * @param ProjectionScale Scale for RTSToNavProjectionExtent used in help projections.
	 */
	void StartNewAttackMoveWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		float WaveInterval,
		float IntervalVarianceFraction,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const bool bInstantStart,
		AActor* WaveCreator,
		const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings,
		const float PerAffectingBuildingTimerFraction,
		const float FormationOffsetMultiplier,
		const float HelpOffsetRadiusMltMax,
		const float HelpOffsetRadiusMltMin,
		const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
		const int32 MaxTriesFindNavPointForHelpOffset,
		const float ProjectionScale);

	/**
	 * @brief Starts a one-off enemy attack wave with an optional delay.
	 * @param WaveType Determines wave logic and whether the wave requires an owning actor.
	 * @param WaveElements Defines the unit types and spawn points for this one-off wave.
	 * @param Waypoints Formation movement path for the spawned units.
	 * @param FinalWaypointDirection The facing direction after reaching the final waypoint.
	 * @param MaxFormationWidth The maximum width of the formation that is spawned in units next to each other.
	 * @param TimeTillWave Delay before starting the wave; zero or less starts immediately.
	 * @param WaveCreator The actor responsible for spawning this wave when required by the wave type.
	 * @param FormationOffsetMultiplier
	 */
	void StartSingleAttackWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const float TimeTillWave,
		AActor* WaveCreator = nullptr, const float FormationOffsetMultiplier = 0
	);

	/**
	 * @brief Use when a one-off wave should pause for allied combat support before advancing.
	 * @param WaveType Determines wave logic and whether the wave requires an owning actor.
	 * @param WaveElements Defines the unit types and spawn points for this one-off wave.
	 * @param Waypoints Formation movement path for the spawned units.
	 * @param FinalWaypointDirection The facing direction after reaching the final waypoint.
	 * @param MaxFormationWidth The maximum width of the formation that is spawned in units next to each other.
	 * @param TimeTillWave Delay before starting the wave; zero or less starts immediately.
	 * @param WaveCreator The actor responsible for spawning this wave when required by the wave type.
	 * @param FormationOffsetMultiplier Multiplier for the formation offset when spawning units in formation.
	 * @param HelpOffsetRadiusMltMax Multiplier for max help distance around allied combat units.
	 * @param HelpOffsetRadiusMltMin Multiplier for min help distance around allied combat units.
	 * @param MaxAttackTimeBeforeAdvancingToNextWayPoint Max time to wait at a waypoint before advancing while in combat.
	 * @param MaxTriesFindNavPointForHelpOffset Attempts per tick to find a valid help location.
	 * @param ProjectionScale Scale for RTSToNavProjectionExtent used in help projections.
	 */
	void StartSingleAttackMoveWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const float TimeTillWave,
		AActor* WaveCreator,
		const float FormationOffsetMultiplier,
		const float HelpOffsetRadiusMltMax,
		const float HelpOffsetRadiusMltMin,
		const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
		const int32 MaxTriesFindNavPointForHelpOffset,
		const float ProjectionScale);

	/**
	 * @brief Starts a one-off spawn wave that transitions into infinite random patrol with attack-move support.
	 * @param WaveType Determines wave logic and whether an owning actor is required.
	 * @param WaveElements Unit options and spawn points for this patrol wave.
	 * @param PatrolPoints Patrol points selected at random after each guard cycle.
	 * @param OverrideFirstPatrolPointIndex Optional first patrol point index override.
	 * @param AmountIterationsAtPatrolPoint Number of guard iterations to execute at each patrol point.
	 * @param GuardTimePerPatrolPointIteration Delay between guard iterations.
	 * @param GuardSphereRadius Radius used to sample guard offsets around the active patrol point.
	 * @param MaxFormationWidth Formation width for spawned units.
	 * @param TimeTillPatrol Delay before first spawn execution.
	 * @param WaveCreator Optional owner actor required by specific wave types.
	 * @param FormationOffsetMultiplier Formation offset multiplier for movement.
	 * @param HelpOffsetRadiusMltMax Max support offset multiplier used by attack move assist logic.
	 * @param HelpOffsetRadiusMltMin Min support offset multiplier used by attack move assist logic.
	 * @param MaxAttackTimeBeforeAdvancingToNextWayPoint Max combat linger time before advancing.
	 * @param MaxTriesFindNavPointForHelpOffset Max attempts to project support positions.
	 * @param ProjectionScale Nav projection scale for support positions.
	 */
	void StartSingleRandomPatrolWithAttackMoveWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& PatrolPoints,
		const int32 OverrideFirstPatrolPointIndex,
		const int32 AmountIterationsAtPatrolPoint,
		const float GuardTimePerPatrolPointIteration,
		const float GuardSphereRadius,
		const int32 MaxFormationWidth,
		const float TimeTillPatrol,
		AActor* WaveCreator,
		const float FormationOffsetMultiplier,
		const float HelpOffsetRadiusMltMax,
		const float HelpOffsetRadiusMltMin,
		const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
		const int32 MaxTriesFindNavPointForHelpOffset,
		const float ProjectionScale);

	void InitWaveController(
		AEnemyController* EnemyController);

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	int32 ShippingTest_GetCompletedAttackMoveWaveCount() const
	{
		return M_ShippingTest_CompletedAttackMoveWaveCount;
	}

	int32 ShippingTest_GetCompletedPatrolWaveCount() const
	{
		return M_ShippingTest_CompletedPatrolWaveCount;
	}

	int32 ShippingTest_GetFailedSpawnCount() const
	{
		return M_ShippingTest_FailedSpawnCount;
	}

	int32 ShippingTest_GetActiveWaveCount() const
	{
		return M_AttackWaves.Num();
	}

	TArray<FEnemyAIShippingWaveFormationHandoff> ShippingTest_GetFormationHandoffHistory() const
	{
		return M_ShippingTest_FormationHandoffHistory;
	}
#endif

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;
	// Cached navigation system for attack move wave projections.
	UPROPERTY()
	TWeakObjectPtr<UNavigationSystemV1> M_NavigationSystem = nullptr;
	bool EnsureEnemyControllerIsValid();
	void CacheGenerationSeedFromGameInstance();
	int32 GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt = 0) const;
	float GetSeededFloatInRange(const float MinValue, const float MaxValue, const int32 DecisionSalt = 0) const;
	UPROPERTY(Transient)
	TArray<FAttackWave> M_AttackWaves;

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	int32 M_ShippingTest_CompletedAttackMoveWaveCount = 0;
	int32 M_ShippingTest_CompletedPatrolWaveCount = 0;
	int32 M_ShippingTest_FailedSpawnCount = 0;
	TArray<FEnemyAIShippingWaveFormationHandoff> M_ShippingTest_FormationHandoffHistory;

	TArray<int32> ShippingTest_GetFormationHistoryIDs();
	int32 ShippingTest_GetNextWaveIteration(const int32 WaveID) const;

	/**
	 * @brief Makes wave-to-formation creation failures observable without retaining either controller or its actors.
	 * @param CompletedWave Value snapshot of the completed wave iteration.
	 * @param WaveSquads Valid squads accepted for the formation request.
	 * @param WaveTanks Valid tanks accepted for the formation request.
	 * @param FormationHistoryIDsBefore IDs recorded immediately before the synchronous formation request.
	 */
	void ShippingTest_RecordFormationHandoff(
		const FAttackWave& CompletedWave,
		const TArray<ASquadController*>& WaveSquads,
		const TArray<ATankMaster*>& WaveTanks,
		const TArray<int32>& FormationHistoryIDsBefore);

	/**
	 * @brief Resolves the one new formation and copies its value-only creation state into the handoff record.
	 * @param InOutHandoff Record that receives the new formation identity, kind, and accepted unit count.
	 * @param FormationHistoryIDsBefore IDs that existed before the formation request.
	 */
	void ShippingTest_ResolveFormationHandoff(
		FEnemyAIShippingWaveFormationHandoff& InOutHandoff,
		const TArray<int32>& FormationHistoryIDsBefore);
	EEnemyAIShippingFormationKind ShippingTest_GetExpectedFormationKind(const FAttackWave& CompletedWave) const;
#endif
	void RemoveAttackWaveByIDAndInvalidateTimer(FAttackWave* AttackWave);
	FAttackWave* FindAttackWaveByID(const int32 UniqueID);
	FAttackWave* GetAttackWaveByID(const int32 UniqueID);

	bool CreateNewAttackWaveStruct(
		const int32 UniqueID,
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		float WaveInterval,
		float IntervalVarianceFraction,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		AActor* WaveCreator,
		const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings, const float PerAffectingBuildingTimerFraction,
		const bool bIsSingleWave = false,
		const float FormationOffsetMultiplier = 0,
		const bool bIsAttackMoveWave = false,
		const FAttackMoveWaveSettings& AttackMoveSettings = {},
		const bool bIsRandomPatrolWithAttackMoveWave = false,
		const FRandomPatrolWithAttackMoveSettings& RandomPatrolWithAttackMoveSettings = {});

	bool CreateAttackWaveTimer(FAttackWave* AttackWave, bool bInstantStart);
	// Uses the interval as well as the variance fractions to randomly get the wave iteration time.
	// If the wave depends on generator buildings then the timer factor for those is also going to be included.
	float GetIterationTimeForWave(FAttackWave* AttackWave) const;
	float GetWaveTimeMltDependingOnGenerators(FAttackWave* AttackWave) const;

	// returns true if the creator died of a wave that depended on it.
	bool DidAttackWaveCreatorDie(const FAttackWave* AttackWave) const;
	void OnAttackWaveCreatorDied(FAttackWave* AttackWave);
	
	/**
	 * @brief Reserves supply before requests so inline callbacks cannot overdraw the wave budget.
	 * @return True when at least one request was submitted and its completion is handled by this function.
	 */
	bool SpawnUnitsForAttackWave(FAttackWave* AttackWave);

	/**
	 * @brief Keeps supply reservation separate from request submission so callbacks see the final pending count.
	 * @param Element The configured wave element whose option may consume one supply.
	 * @param OutPickedOption Receives the option when supply was reserved.
	 * @return Whether an option was selected and one unit of wave supply was reserved.
	 */
	bool TryReserveSupplyForWaveElement(
		const FAttackWaveElement& Element,
		FTrainingOption& OutPickedOption);

	/**
	 * @brief Resolves callback and rejected-request paths through one shared completion state.
	 * @param TrainingOption The reserved option submitted to the async spawner.
	 * @param SpawnLocation The configured world location for this request.
	 * @param WaveID Stable identity used because inline callbacks may move or remove array elements.
	 */
	void SubmitSpawnRequestForWave(
		const FTrainingOption& TrainingOption,
		const FVector& SpawnLocation,
		const int32 WaveID);

	bool GetRandomAttackWaveElementOption(const FAttackWaveElement& Element, FTrainingOption& OutPickedOption) const;

	/**
	 * @brief Resolves a reserved request without retaining a pointer into the mutable wave array.
	 * @param TrainingOption The option whose supply was reserved for this callback.
	 * @param SpawnedActor The spawned unit, or null when loading or spawning failed.
	 * @param ID Stable wave identity forwarded by the async spawner.
	 */
	void OnUnitSpawnedForWave(
		const FTrainingOption& TrainingOption,
		AActor* SpawnedActor,
		const int32 ID);

	/**
	 * @brief Refunds and resolves a failed request exactly once through the caller's resolution guard.
	 * @param TrainingOption The option whose reserved supply must be refunded.
	 * @param WaveID Stable wave identity used to update the pending count when the wave still exists.
	 */
	void OnWaveUnitFailedToSpawn(
		const FTrainingOption& TrainingOption,
		const int32 WaveID);
	void CompletePendingSpawnForWave(const int32 WaveID);
	void FinalizeWaveIterationWithoutSpawnedUnits(const int32 WaveID);
	void RefundSupplyForFailedWaveSpawn();

	/**
	 * @brief Re-fetches the record after formation setup may have mutated the wave array.
	 * @param WaveID Stable identity of the completed wave iteration.
	 * @param bIsSingleWave Whether the record must be removed instead of scheduling another iteration.
	 */
	void FinishWaveAfterFormationStarted(const int32 WaveID, const bool bIsSingleWave);

	void OnWaveCompletedSpawn(FAttackWave* Wave);

	/**
	 * @brief Uses a value snapshot because formation setup can re-enter systems that mutate the wave array.
	 * @param CompletedWave Stable settings and waypoints for the completed spawn iteration.
	 * @param WaveSquads Valid squads extracted from the spawned actor list.
	 * @param WaveTanks Valid tanks extracted from the spawned actor list.
	 * @param AverageSpawnLocation Formation origin calculated before movement starts.
	 */
	void StartMovementForCompletedWave(
		const FAttackWave& CompletedWave,
		const TArray<ASquadController*>& WaveSquads,
		const TArray<ATankMaster*>& WaveTanks,
		const FVector& AverageSpawnLocation) const;

	FVector GetAverageSpawnLocation(
		const TArray<ASquadController*>& SquadControllers,
		const TArray<ATankMaster*>& TankMasters) const;

	/**
	 * @brief Filters the completed spawn set before supply ownership transfers to a formation.
	 * @param OutTankMasters Receives valid spawned tanks.
	 * @param OutSquadControllers Receives valid spawned squad controllers.
	 * @param InWaveActors Actors resolved by this wave iteration.
	 * @return Number of invalid actors whose reserved supply was refunded.
	 */
	int32 ExtractTanksAndSquadsFromWaveActors(
		TArray<ATankMaster*>& OutTankMasters,
		TArray<ASquadController*>& OutSquadControllers,
		const TArray<TObjectPtr<AActor>>& InWaveActors) const;

	UPROPERTY()
	TWeakObjectPtr<ARTSAsyncSpawner> M_AsyncSpawner;
	bool GetIsValidAsyncSpawner();

	


	// Used to identify to which wave was provided to the async spawner.
	int32 CurrentUniqueWaveID = -1;
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;

	int32 GetUniqueWaveID()
	{
		return ++CurrentUniqueWaveID;
	}


	// ------------------------------------------------------------------
	// --------- Check wave validity ---------------------------------
	// ------------------------------------------------------------------
	bool GetIsValidWave(const EEnemyWaveType WaveType,
	                    const TArray<FAttackWaveElement>& WaveElements,
	                    const float WaveInterval,
	                    const float IntervalVarianceFactor,
	                    const TArray<FVector>& Waypoints,
	                    AActor* WaveCreator = nullptr,
	                    const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings = {}, const float PerAffectingBuildingTimerFraction =
		                    0
	) const;
	bool GetIsValidSingleAttackWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& Waypoints,
		AActor* WaveCreator) const;
	bool GetIsValidAttackMoveWaveSettings(
		const float HelpOffsetRadiusMltMax,
		const float HelpOffsetRadiusMltMin,
		const int32 MaxTriesFindNavPointForHelpOffset,
		const float ProjectionScale) const;
	bool GetAreWaveElementsValid(TArray<FAttackWaveElement> WaveElements) const;
	bool GetAreWavePointsValid(const TArray<FVector>& Waypoints) const;
	bool GetIsValidTrainingOption(const FTrainingOption& TrainingOption) const;
	bool GetIsValidWaveType(const EEnemyWaveType WaveType,
	                        AActor* WaveCreator = nullptr,
	                        const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings = {}, const float PerAffectingBuildingTimerFraction =
		                        0) const;
	// ------------------------------------------------------------------
	// --------- END Check wave validity ---------------------------------
	// ------------------------------------------------------------------


	void Debug(const FString& Message, const FColor& Color) const;
	bool EnsureIDIsUnique(const int32 ID);
};
