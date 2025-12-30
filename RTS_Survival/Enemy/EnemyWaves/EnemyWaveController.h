// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttackWave.h"
#include "Components/ActorComponent.h"
#include "EnemyWaveController.generated.h"


class ASquadController;
class ATankMaster;
class AEnemyController;
class ARTSAsyncSpawner;

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
		PerAffectingBuildingTimerFraction = 0.f
	);

	/**
	 * @brief Starts a one-off enemy attack wave with an optional delay.
	 * @param WaveType Determines wave logic and whether the wave requires an owning actor.
	 * @param WaveElements Defines the unit types and spawn points for this one-off wave.
	 * @param Waypoints Formation movement path for the spawned units.
	 * @param FinalWaypointDirection The facing direction after reaching the final waypoint.
	 * @param MaxFormationWidth The maximum width of the formation that is spawned in units next to each other.
	 * @param TimeTillWave Delay before starting the wave; zero or less starts immediately.
	 * @param WaveCreator The actor responsible for spawning this wave when required by the wave type.
	 */
	void StartSingleAttackWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const float TimeTillWave,
		AActor* WaveCreator = nullptr
	);

	void InitWaveController(
		AEnemyController* EnemyController);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:

	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;
	bool EnsureEnemyControllerIsValid();
	TArray<FAttackWave> M_AttackWaves;
	void RemoveAttackWaveByIDAndInvalidateTimer(FAttackWave* AttackWave);
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
		const bool bIsSingleWave = false);

	bool CreateAttackWaveTimer(FAttackWave* AttackWave, bool bInstantStart);
	// Uses the interval as well as the variance fractions to randomly get the wave iteration time.
	// If the wave depends on generator buildings then the timer factor for those is also going to be included.
	float GetIterationTimeForWave(FAttackWave* AttackWave) const;
	float GetWaveTimeMltDependingOnGenerators(FAttackWave* AttackWave) const;

	// returns true if the creator died of a wave that depended on it.
	bool DidAttackWaveCreatorDie(const FAttackWave* AttackWave) const;
	void OnAttackWaveCreatorDied(FAttackWave* AttackWave);
	
	/**
	 * @briefWill only spawn as many units as allowed by the wave supply
	* Only returns true if at least one spawn request was successful, false otherwise.
	* @post The async spawner is spawning the selected units and will use @See OnUnitSpawnedForWave
	* when a unit was spawned.
	*/ 
	bool SpawnUnitsForAttackWave(FAttackWave* AttackWave);

	bool GetRandomAttackWaveElementOption(const FAttackWaveElement& Element, FTrainingOption& OutPickedOption) const;

	/**
	 * @brief Adds the spawned actor to the wave of the provided ID if the actor is valid; returns the wave supply back
	 * to the enemy controller otherwise.
	 *
	 */
	void OnUnitSpawnedForWave(
		const FTrainingOption& TrainingOption,
		AActor* SpawnedActor,
		const int32 ID);
	void OnWaveUnitFailedToSpawn(
		const FTrainingOption& TrainingOption,
		const int32 WaveID);

	void OnWaveCompletedSpawn(FAttackWave* Wave);

	// Gets the tanks and squads from the wave, will destroy actors that are not of those types.
	// Assumes valid Enemy controller is set.
	void ExtractTanksAndSquadsFromWaveActors(
		TArray<ATankMaster*>& OutTankMasters,
		TArray<ASquadController*>& OutSquadControllers, const TArray<AActor*>& InWaveActors) const;

	TWeakObjectPtr<ARTSAsyncSpawner> M_AsyncSpawner;
	bool GetIsValidAsyncSpawner();

	


	// Used to identify to which wave was provided to the async spawner.
	int32 CurrentUniqueWaveID = -1;

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
