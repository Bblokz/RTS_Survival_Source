// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameUnitAddRemoveDelegates/FGameUnitAddRemoveDelegates.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "UObject/Object.h"
#include "GameUnitManager.generated.h"

class AAircraftMaster;
enum class ESquadSubtype : uint8;
enum class ENomadicSubtype : uint8;
enum class ETankSubtype : uint8;
struct FTrainingOption;
class AHpPawnMaster;
enum class ETargetPreference : uint8;
class FGetAsyncTarget;
class ASquadUnit;
class ATankMaster;
struct FStrategicAIRequestBatch;
struct FStrategicAIResultBatch;
/**
 * Contains all the units for all the players in the game.
 */
UCLASS()
class RTS_SURVIVAL_API UGameUnitManager : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Constructor */
	UGameUnitManager();

	// Allows for disabling the optimization for all units for cinematics.
	void SetAllUnitOptimizationEnabled(const bool bEnable);

	/** @brief External entry point to subscribe to unit add/remove events per player and unit type. */
	UPROPERTY()
	FGameUnitAddRemoveDelegates GameUnitDelegates;


	/**
	 * @brief Adds a target request.
	 * @param SearchLocation The location from which to search.
	 * @param SearchRadius The radius within which to search.
	 * @param NumTargets The number of closest targets to find.
	 * @param OwningPlayer The player owning this request.
	 * @param PrefferedTarget The preferred target type.
	 * @param Callback The callback function to invoke with the result.
	 */
	void RequestClosestTargets(
		const FVector& SearchLocation,
		float SearchRadius,
		int32 NumTargets,
		int32 OwningPlayer, ETargetPreference PrefferedTarget,
		TFunction<void(const TArray<AActor*>&)> Callback);

	/**
	 * @brief Requests strategic AI results based on detailed unit state cached on the async thread.
	 * @param RequestBatch Aggregated strategy requests to evaluate asynchronously.
	 * @param Callback Callback invoked on the game thread with the computed results.
	 */
	void RequestStrategicAIRequests(
		const FStrategicAIRequestBatch& RequestBatch,
		TFunction<void(const FStrategicAIResultBatch&)> Callback);

	/**
 * @brief Returns all currently alive UnitMasters of the provided player.
 * @param Player The player of which we want the UnitMasters.
 */
	TArray<ASquadUnit*> GetSquadUnitsOfPlayer(const uint8 Player) const;

	/**
	 * @brief adds the given UM to the array for the corresponding player.
	 * @param SquadUnit: the UnitMaster that will be added.
	 * @param Player The Player to which the unit belongs.
	 */
	void AddSquadUnitForPlayer(
		ASquadUnit* SquadUnit,
		const uint8 Player);

	/**
	 * @brief Removes the given unit from the array for the specified player
	 * @param SquadUnit: The UnitMaster to remove.
	 * @param Player The player to which the unit belongs.
	 */
	void RemoveSquadUnitForPlayer(
		ASquadUnit* SquadUnit,
		const uint8 Player);

	/**
	 * @brief adds the given Tank to the array for the corresponding player.
	 * @param Tank: the Tank that will be added.
	 * @param Player The Player to which the unit belongs.
	 */
	void AddTankForPlayer(
		ATankMaster* Tank,
		const uint8 Player);

	bool AddAircraftForPlayer(
		AAircraftMaster* Aircraft,
		const uint8 Player);

	bool RemoveAircraftForPlayer(
		AAircraftMaster* Aircraft,
		const uint8 Player);

	bool AddBxpForPlayer(
		ABuildingExpansion* Bxp,
		const uint8 Player);

	bool RemoveBxpForPlayer(
		ABuildingExpansion* Bxp,
		const uint8 Player);
	void AddActorForPlayer(
		AActor* Actor,
		const uint8 Player);

	void RemoveActorForPlayer(
		AActor* Actor,
		const uint8 Player);


	/**
	 * @brief Removes the given unit from the array for the specified player
	 * @param Tank: The Tank to remove.
	 * @param Player The player to which the unit belongs.
	 */
	void RemoveTankForPlayer(
		ATankMaster* Tank,
		const uint8 Player);

	/**
	 * @brief: Returns all alive tanks of the provided player
	 * @param Player The player that owns the tanks
	 */
	TArray<ATankMaster*> GetPlayerTanks(const uint8 Player) const;

	/**
	 * @brief Returns a target in provided range, stops searching if a UnitMaster Target was found.
	 * @param PlayerSearch The team of the unit that is executing the search.
	 * @param SearchRadius The range radius in which the target is searched.
	 * @param SearchLocation The location from which the search is preformed.
	 */
	AActor* GetClosestTargetPreferSquadUnit(
		const uint8 PlayerSearch,
		const float SearchRadius,
		const FVector SearchLocation
	) const;

	/**
	 * @brief Returns a target in provided range, stops searching if a Tank Target was found.
	 * @param PlayerSearch The team of the unit that is executing the search.
	 * @param SearchRadius The range radius in which the target is searched.
	 * @param SearchLocation The location from which the search is preformed.
	 */
	AActor* GetClosestTargetPreferTank(
		const uint8 PlayerSearch,
		const float SearchRadius,
		const FVector& SearchLocation
	) const;

	/**
	 * @brief Returns the closest target in provided range.
	 * @param PlayerSearch The team of the unit that is executing the search.
	 * @param SearchRadius The range radius in which the target is searched.
	 * @param SearchLocation The location from which the search is preformed.
	 */
	AActor* GetClosestTarget(
		const uint8 PlayerSearch,
		const float SearchRadius,
		const FVector& SearchLocation
	) const;

	AActor* FindUnitForPlayer(
		const FTrainingOption& TrainingOption,
		const int32 OwningPlayer) const;

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

private:
	/** Initializes the GetTargetUnit M_Thread async. */
	void Initialize();

	/** Cleans up the GetTarget Unit M_Thread's resources */
	void ShutdownAsyncTargetThread();

	/** Periodically updates the actor data on the async thread
	 * @note We only add data of those actors that are visible in the fow, this means if the player
	 * sees no enemy units the async target thread has zero targets.
	 */
	void UpdateActorData();

	void UpdateDetailedActorData();

	/** @brief contains all SquadUnits currently alive, for the player. */
	UPROPERTY()
	TArray<ASquadUnit*> M_SquadUnitAlivePlayer;

	/** @brief contains all SquadUnits currently alive, for the enemy player. */
	UPROPERTY()
	TArray<ASquadUnit*> M_SquadUnitsAliveEnemy;

	/** @brief contains all TankMasters currently alive, for the player. */
	UPROPERTY()
	TArray<ATankMaster*> M_TankMastersAlivePlayer;

	/** @brief contains all TankMasters currently alive, for the player. */
	UPROPERTY()
	TArray<ATankMaster*> M_TankMastersAliveEnemy;

	UPROPERTY()
	TArray<AAircraftMaster*> M_AircraftMastersAlivePlayer;

	UPROPERTY()
	TArray<AAircraftMaster*> M_AircraftMastersAliveEnemy;

	UPROPERTY()
	TArray<ABuildingExpansion*> M_BxpAlivePlayer;
	
	UPROPERTY()
	TArray<ABuildingExpansion*> M_BxpAliveEnemy;

	UPROPERTY()
	TArray<AActor*> M_ActorsAlivePlayer;

	UPROPERTY()
	TArray<AActor*> M_ActorsAliveEnemy;

	// -------------- ASYNC TARGET FINDING ---------------

	/** Pointer to the FRunnable of the async GetTargetUnit thread type */
	FGetAsyncTarget* M_AsyncTargetProcessor;

	/** Mapping of Actor IDs enemy units */
	UPROPERTY()
	TMap<uint32, AActor*> M_EnemyActorIDToActorMap;
	// Mapping of actor IDs to player units.
	UPROPERTY()
	TMap<uint32, AActor*> M_PlayerActorIDToActorMap;

	/** Timer handle for periodic actor data updates */
	FTimerHandle M_ActorDataUpdateTimerHandle;

	/** Timer handle for periodic detailed actor data updates */
	FTimerHandle M_DetailedActorDataUpdateTimerHandle;

	// Timer handle for cleaning up invalid actor references.
	FTimerHandle M_CleanUpInvalidRefsTimerHandle;

	void CleanupInvalidActorReferences();

	template <typename T>
	void CleanupActorArray(TArray<T*>& ActorArray);

	

	/** Interval for actor data updates */
	float M_ActorDataUpdateInterval;

	/** Interval for detailed actor data updates */
	float M_DetailedActorDataUpdateInterval;

	/**
	 * @brief Called when target IDs are received from the async thread.
	 * @param TargetIDs The array of target Actor IDs.
	 * @param Callback The original callback function provided by the requester.
	 * @param OwningPlayer
	 */
	void OnTargetIDsReceived(
		const TArray<uint32>& TargetIDs,
		TFunction<void(const TArray<AActor*>&)> Callback, int32 OwningPlayer);

	TPair<ETargetPreference, FVector> CreatePair(const FVector& ActorLocation, const ETargetPreference& Preference);

	/**
	 * @brief Helper to get one array with valid target data computed over all different unit arrays.
	 * @param bGetPlayerUnits Whether to look through the player units or the enemy units. 
	 * @param OutActorData The cumulated data over all different unit type arrays, those units are valid targets
	 *  and are visible.
	 */
	void GetAsyncActorData(const bool bGetPlayerUnits,
	                       TMap<uint32, TPair<ETargetPreference, FVector>>& OutActorData);

	AActor* FindTankUnitOfPlayer(const ETankSubtype TankSubtype, const int32 OwningPlayer) const;
	AActor* FindAircraftUnitOfPlayer(const EAircraftSubtype AircraftSubtype, const int32 OwningPlayer) const;
	AActor* FindSquadUnitOfPlayer(const ESquadSubtype SquadSubtype, const int32 OwningPlayer) const;
	AActor* FindNomadicUnitOfPlayer(const ENomadicSubtype NomadicSubtype, const int32 OwningPlayer) const;
	AActor* FindBxpUnitOfPlayer(const EBuildingExpansionType BxpSubtype, const int32 OwningPlayer) const;

	/**
	 * @brief Adds the actor data of the given actor to the provided map.
	 * @post the ID mapping now contains the actor ID and the actor.
	 * @post the OutActorData now contains the actor data.
	 * @pre The actor is visible.
	 */
	void AddActorData(const int32 EnemyOfActor,
	                  AActor* Actor,
	                  TMap<uint32, AActor*>* CurrentActorIDMapping,
	                  TMap<uint32, TPair<ETargetPreference, FVector>>& OutActorData);
};

template <typename T>
FORCEINLINE void UGameUnitManager::CleanupActorArray(TArray<T*>& ActorArray)
{
	TArray<T*> ValidActors;
	ValidActors.Reserve(ActorArray.Num());

	for (T* EachActor : ActorArray)
	{
		if (IsValid(EachActor)) // use UE's IsValid to avoid adding extra includes in the header
		{
			ValidActors.Add(EachActor);
		}
	}

	ActorArray = MoveTemp(ValidActors);
}
