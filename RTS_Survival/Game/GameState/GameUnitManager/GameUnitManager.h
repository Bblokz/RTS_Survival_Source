// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameUnitAddRemoveDelegates/FGameUnitAddRemoveDelegates.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Game/UserSettings/GameplaySettings/HealthbarVisibilityStrategy/HealthBarVisibilityStrategy.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "GameUnitManager.generated.h"

#ifndef RTS_WITH_SHIPPING_MAP_TESTS
#define RTS_WITH_SHIPPING_MAP_TESTS 0
#endif

class AAircraftMaster;
class ANomadicVehicle;
class ASquadController;
class UTechnologyEffect;
enum class EAircraftSubtype : uint8;
enum class EBuildingExpansionType : uint8;
enum class ESquadSubtype : uint8;
enum class ENomadicSubtype : uint8;
enum class ETankSubtype : uint8;
struct FTrainingOption;
class AHpPawnMaster;
class UHealthComponent;
enum class ETargetPreference : uint8;
class FGetAsyncTarget;
class ASquadUnit;
class ATankMaster;
struct FStrategicAIRequestBatch;
struct FStrategicAIResultBatch;
struct FAsyncTargetSnapshot;

/**
 * @brief Everything the unit manager keeps for one player: its alive units per kind and how many of each
 * subtype it has. UGameUnitManager holds one per player number.
 */
USTRUCT()
struct FGameUnitsOfPlayer
{
	GENERATED_BODY()

	/** @brief contains all SquadUnits currently alive, for this player. */
	UPROPERTY()
	TArray<ASquadUnit*> SquadUnits;

	/** @brief contains all TankMasters currently alive, for this player; nomadic vehicles are in here too. */
	UPROPERTY()
	TArray<ATankMaster*> TankMasters;

	UPROPERTY()
	TArray<AAircraftMaster*> AircraftMasters;

	UPROPERTY()
	TArray<ABuildingExpansion*> Bxps;

	// Every other registered actor with health (standalone turrets, test turret owners).
	UPROPERTY()
	TArray<AActor*> Actors;

	UPROPERTY()
	TMap<ETankSubtype, int32> TankCountByType;

	UPROPERTY()
	TMap<ESquadSubtype, int32> SquadCountByType;

	UPROPERTY()
	TMap<ENomadicSubtype, int32> NomadicCountByType;

	UPROPERTY()
	TMap<EBuildingExpansionType, int32> BxpCountByType;

	UPROPERTY()
	TMap<EAircraftSubtype, int32> AircraftCountByType;
};

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
	 * @brief Applies cached user healthbar visibility settings for the provided component.
	 * @param HealthComponent Component to update based on saved user settings.
	 * @return True if a visibility override was applied, otherwise false.
	 */
	bool TryApplyHealthBarVisibilityFromUserSettings(UHealthComponent* HealthComponent);

	void ApplyPlayerTankHealthBarVisibilityStrategy(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	void ApplyPlayerSquadHealthBarVisibilityStrategy(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	void ApplyPlayerNomadicHealthBarVisibilityStrategy(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	void ApplyPlayerBxpHealthBarVisibilityStrategy(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	void ApplyPlayerAircraftHealthBarVisibilityStrategy(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	void ApplyEnemyTankHealthBarVisibilityStrategy(ERTSEnemyHealthBarVisibilityStrategy Strategy);
	void ApplyEnemySquadHealthBarVisibilityStrategy(ERTSEnemyHealthBarVisibilityStrategy Strategy);
	void ApplyEnemyNomadicHealthBarVisibilityStrategy(ERTSEnemyHealthBarVisibilityStrategy Strategy);
	void ApplyEnemyBxpHealthBarVisibilityStrategy(ERTSEnemyHealthBarVisibilityStrategy Strategy);
	void ApplyEnemyAircraftHealthBarVisibilityStrategy(ERTSEnemyHealthBarVisibilityStrategy Strategy);


	/**
	 * @brief Adds a target request.
	 * @param SearchLocation The location from which to search.
	 * @param SearchRadius The radius within which to search.
	 * @param NumTargets The number of closest targets to find.
	 * @param OwningPlayer The player owning this request; only units of players hostile to it are found.
	 * @param PrefferedTarget The preferred target type.
	 * @param Callback The callback function to invoke with the result. Called exactly once per request, with an
	 * empty array when there is no target; the units in it are alive and hostile to the requester at that moment.
	 * @param MinimumSearchRadius Targets closer than this radius are excluded from the result.
	 */
	void RequestClosestTargets(
		const FVector& SearchLocation,
		float SearchRadius,
		int32 NumTargets,
		int32 OwningPlayer, ETargetPreference PrefferedTarget,
		TFunction<void(const TArray<AActor*>&)> Callback,
		float MinimumSearchRadius = 0.0f);

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
 * @param Player The player of which we want the UnitMasters; exactly this player, not "every enemy".
 */
	TArray<ASquadUnit*> GetSquadUnitsOfPlayer(const uint8 Player) const;

	/**
	 * @brief Returns all alive squad units of a group of players, e.g. FRTSPlayers::GetHostileMask(Player).
	 * @param PlayerMask Bits of the players of which we want the squad units (FRTSPlayers::GetPlayerBit).
	 */
	TArray<ASquadUnit*> GetSquadUnitsOfPlayers(const uint8 PlayerMask) const;

	/**
	 * @brief Returns all alive tanks (nomadic vehicles included) of a group of players.
	 * @param PlayerMask Bits of the players of which we want the tanks (FRTSPlayers::GetPlayerBit).
	 */
	TArray<ATankMaster*> GetTanksOfPlayers(const uint8 PlayerMask) const;

	/**
	 * @brief GetSquadUnitsOfPlayer without the copy, for callers that only walk the units.
	 * @param Player The player that owns the squad units.
	 * @return View into the manager's own array: do not keep it, and do not add or remove units while using it.
	 */
	TConstArrayView<ASquadUnit*> GetSquadUnitsOfPlayerView(const uint8 Player) const;

	/**
	 * @brief GetPlayerTanks without the copy, for callers that only walk the units.
	 * @param Player The player that owns the tanks.
	 * @return View into the manager's own array: do not keep it, and do not add or remove units while using it.
	 */
	TConstArrayView<ATankMaster*> GetTanksOfPlayerView(const uint8 Player) const;

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
	 * @param Player The player that owns the tanks; exactly this player, not "every enemy".
	 */
	TArray<ATankMaster*> GetPlayerTanks(const uint8 Player) const;

#if RTS_WITH_SHIPPING_MAP_TESTS || RTS_WITH_MAP_TESTS
	TArray<AAircraftMaster*> ShippingTest_GetAircraftOfPlayer(uint8 Player) const;
	TArray<ABuildingExpansion*> MapTest_GetBxpsOfPlayer(uint8 Player) const;
#endif

	void ApplyTechToTanksOfPlayer(UTechnologyEffect* TechEffect, const TArray<ETankSubtype>& TankSubtypes, uint8 Player) const;
	void ApplyTechToNomadicsOfPlayer(UTechnologyEffect* TechEffect, const TArray<ENomadicSubtype>& NomadicSubtypes, uint8 Player) const;
	void ApplyTechToSquadsOfPlayer(UTechnologyEffect* TechEffect, const TArray<ESquadSubtype>& SquadSubtypes, uint8 Player) const;
	void ApplyTechToBuildingExpansionsOfPlayer(UTechnologyEffect* TechEffect, const TArray<EBuildingExpansionType>& BxpSubtypes, uint8 Player) const;
	void ApplyTechToAircraftOfPlayer(UTechnologyEffect* TechEffect, const TArray<EAircraftSubtype>& AircraftSubtypes, uint8 Player) const;

	// "Player" is the human player.
	bool GetHasPlayerTankOfType(ETankSubtype TankSubtype) const;
	bool GetHasPlayerSquadOfType(ESquadSubtype SquadSubtype) const;
	bool GetHasPlayerNomadicOfType(ENomadicSubtype NomadicSubtype) const;
	bool GetHasPlayerBxpOfType(EBuildingExpansionType BxpSubtype) const;
	bool GetHasPlayerAircraftOfType(EAircraftSubtype AircraftSubtype) const;
	// "Enemy" is any AI player: true when at least one of them has a unit of the type.
	bool GetHasEnemyTankOfType(ETankSubtype TankSubtype) const;
	bool GetHasEnemySquadOfType(ESquadSubtype SquadSubtype) const;
	bool GetHasEnemyNomadicOfType(ENomadicSubtype NomadicSubtype) const;
	bool GetHasEnemyBxpOfType(EBuildingExpansionType BxpSubtype) const;
	bool GetHasEnemyAircraftOfType(EAircraftSubtype AircraftSubtype) const;
	// Exactly the given player.
	bool GetHasTankOfTypeForPlayer(uint8 Player, ETankSubtype TankSubtype) const;
	bool GetHasSquadOfTypeForPlayer(uint8 Player, ESquadSubtype SquadSubtype) const;
	bool GetHasNomadicOfTypeForPlayer(uint8 Player, ENomadicSubtype NomadicSubtype) const;
	bool GetHasBxpOfTypeForPlayer(uint8 Player, EBuildingExpansionType BxpSubtype) const;
	bool GetHasAircraftOfTypeForPlayer(uint8 Player, EAircraftSubtype AircraftSubtype) const;

	int32 GetPlayerSquadCountOfTypes(const uint8 Player, const TArray<ESquadSubtype>& SquadTypes) const;
	int32 GetPlayerTankCountOfTypes(const uint8 Player, const TArray<ETankSubtype>& TankTypes) const;
	int32 GetPlayerBxpTotalCountOfTypes(const uint8 Player, const TArray<EBuildingExpansionType>& BxpTypes) const;

	/**
	 * @brief Counts only the requested building expansion types in one pass over that player's live BXPs.
	 * Only standing BXPs count (ABuildingExpansion::GetIsStanding), so one still under construction unlocks nothing.
	 * @param Player The player that owns the building expansions.
	 * @param BxpTypes The building expansion types that need explicit count entries.
	 * @return Mapping with every requested type present, including zero-count types.
	 */
	TMap<EBuildingExpansionType, int32> GetPlayerBxpCountsByType(
		const uint8 Player,
		const TArray<EBuildingExpansionType>& BxpTypes) const;
	
	int32 GetPlayerAircraftCountOfTypes(const uint8 Player, const TArray<EAircraftSubtype>& AircraftTypes) const;

	/**
	 * @brief Returns the first alive player tank configured as the command vehicle subtype.
	 * @return First matching command vehicle tank, or nullptr if none are alive.
	 */
	ATankMaster* GetPlayerCommandVehicle() const;

	ANomadicVehicle* GetPlayerHQ()const;

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

public:
	/**
	 * @brief Moves a soldier's count to its current squad subtype. A soldier registers itself before its squad
	 * controller sets its subtype; the controller calls this right after it did.
	 * @param SquadUnit Soldier whose subtype was just set.
	 */
	void RecountSquadUnitSubtype(ASquadUnit* SquadUnit);

	/**
	 * @brief Moves a tank's, nomadic vehicle's, aircraft's or building expansion's count to its current subtype.
	 * Their Blueprints and init functions set the subtype after the unit registered itself;
	 * URTSComponent::SetUnitSubtype calls this once the unit has begun play.
	 * @param Unit Unit whose subtype was just set; other unit kinds are ignored.
	 */
	void RecountUnitSubtype(AActor* Unit);

private:
	/** Initializes the GetTargetUnit M_Thread async. */
	void Initialize();

	/** Cleans up the GetTarget Unit M_Thread's resources */
	void ShutdownAsyncTargetThread();

	/** Periodically updates the actor data on the async thread
	 * @note We only add data of those actors that are visible in the fow to at least one active player that fights
	 * their owner, this means if
	 * the player sees no enemy units and no AI unit sees a player unit, a two-player game has zero targets.
	 */
	void UpdateActorData();

	void UpdateDetailedActorData();

	// Alive units and subtype counters per player, indexed by player number (1..RTSPlayers::MaxPlayers).
	// Slot 0 belongs to the neutral owner and stays empty: neutral things are never registered.
	UPROPERTY()
	FGameUnitsOfPlayer M_UnitsOfPlayer[RTSPlayers::PlayerArraySize];

	/** @return The storage of a player; unset owners (222) share that of the first AI player. */
	FGameUnitsOfPlayer& GetUnitsOfPlayer(uint8 Player);
	const FGameUnitsOfPlayer& GetUnitsOfPlayer(uint8 Player) const;

	// Controllers of the squads that have at least one registered soldier, over all players; the controllers
	// themselves are not registered.
	TSet<ASquadController*> GatherSquadControllersOfAllPlayers() const;

	AActor* FindGenericActorUnitForPlayer(
		const FTrainingOption& TrainingOption,
		int32 OwningPlayer) const;

	void IncrementTankOrNomadicCounter(ATankMaster* Tank, uint8 Player);
	void DecrementTankOrNomadicCounter(ATankMaster* Tank, uint8 Player);
	void IncrementSquadCounter(ASquadUnit* SquadUnit, uint8 Player);
	void DecrementSquadCounter(ASquadUnit* SquadUnit, uint8 Player);

	// Under which squad subtype and player each soldier is counted. A soldier registers before its squad has
	// given it its subtype, so what is taken off the counters when it goes must be what was put on them, not
	// whatever its subtype is by then.
	struct FCountedSquadUnit
	{
		ESquadSubtype Subtype;
		uint8 Player = 0;
	};

	TMap<TObjectKey<ASquadUnit>, FCountedSquadUnit> M_CountedSquadUnits;

	// Under which subtype and player a vehicle, aircraft or building expansion is counted (and, for vehicles,
	// whether on the nomadic counters). Their subtype is set after they registered, so what is taken off the
	// counters when they go must be what was put on them; a unit removed twice or never added has nothing to take off.
	struct FCountedUnitSubtype
	{
		uint8 Subtype = 0;
		uint8 Player = 0;
		bool bIsNomadic = false;
	};

	TMap<TObjectKey<ATankMaster>, FCountedUnitSubtype> M_CountedTanksOrNomadics;
	TMap<TObjectKey<AAircraftMaster>, FCountedUnitSubtype> M_CountedAircraft;
	TMap<TObjectKey<ABuildingExpansion>, FCountedUnitSubtype> M_CountedBxps;
	void RecountTankOrNomadicSubtype(ATankMaster* Tank);
	void RecountAircraftSubtype(AAircraftMaster* Aircraft);
	void RecountBxpSubtype(ABuildingExpansion* Bxp);
	void IncrementBxpCounter(ABuildingExpansion* Bxp, uint8 Player);
	void DecrementBxpCounter(ABuildingExpansion* Bxp, uint8 Player);
	void IncrementAircraftCounter(AAircraftMaster* Aircraft, uint8 Player);
	void DecrementAircraftCounter(AAircraftMaster* Aircraft, uint8 Player);

	// -------------- ASYNC TARGET FINDING ---------------

	/** Pointer to the FRunnable of the async GetTargetUnit thread type */
	FGetAsyncTarget* M_AsyncTargetProcessor;

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
	 * @brief Helper to get one snapshot with valid target data computed over all different unit arrays of all
	 * players, grouped by owning player.
	 * @param OutSnapshot The cumulated data over all different unit type arrays, those units are valid targets
	 *  and are visible to at least one player.
	 */
	void BuildAsyncTargetSnapshot(FAsyncTargetSnapshot& OutSnapshot) const;

	/**
	 * @brief Adds the targetable units of one player to the snapshot, in the order generic actors, tanks,
	 * squad units, aircraft, building expansions.
	 * @param Player Player number whose units are added.
	 * @param OutSnapshot Snapshot to append to.
	 */
	void AppendAsyncTargetUnitsOfPlayer(int32 Player, FAsyncTargetSnapshot& OutSnapshot) const;

	AActor* FindTankUnitOfPlayer(const ETankSubtype TankSubtype, const int32 OwningPlayer) const;
	AActor* FindAircraftUnitOfPlayer(const EAircraftSubtype AircraftSubtype, const int32 OwningPlayer) const;
	AActor* FindSquadUnitOfPlayer(const ESquadSubtype SquadSubtype, const int32 OwningPlayer) const;
	AActor* FindNomadicUnitOfPlayer(const ENomadicSubtype NomadicSubtype, const int32 OwningPlayer) const;
	AActor* FindBxpUnitOfPlayer(const EBuildingExpansionType BxpSubtype, const int32 OwningPlayer) const;

};

template <typename T>
FORCEINLINE void UGameUnitManager::CleanupActorArray(TArray<T*>& ActorArray)
{
	// In place and keeping the order: no new array for every list on every clean-up.
	ActorArray.RemoveAll([](const T* EachActor)
	{
		return not IsValid(EachActor); // use UE's IsValid to avoid adding extra includes in the header
	});
}

