// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/Healthbar/GarrisonHealthBar/GarrisonSeatsTextType.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoOwner/CargoOwner.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "Cargo.generated.h"

class UW_GarrisonHealthBar;
class UHealthComponent;
class UPrimitiveComponent;
class UCargo;
class ASquadUnit;
class ASquadController;

/**
 * @brief Tracks how many squads may occupy this cargo and who is currently inside.
 *
 * Maintains a set of inside squads and derives the current count from it to
 * keep state consistent when squads are registered/unregistered.
 */
USTRUCT()
struct FCargoVacancyState
{
	GENERATED_BODY()

	/** Maximum supported squads for this cargo owner. */
	int32 M_MaxSquadsSupported = 0;

	/** Currently docked squads (derived from M_InsideSquads). */
	int32 M_CurrentSquads = 0;

	/** Which squads are inside. */
	UPROPERTY()
	TSet<TObjectPtr<ASquadController>> M_InsideSquads;

	/** @return Whether another squad may enter (strictly less than max). */
	bool HasVacancy() const
	{
		return M_CurrentSquads < M_MaxSquadsSupported;
	}

	/** Register a squad as inside and refresh the derived count. */
	void RegisterSquad(ASquadController* Squad)
	{
		if (not Squad)
		{
			return;
		}
		M_InsideSquads.Add(Squad);
		M_CurrentSquads = FMath::Max(0, M_InsideSquads.Num());
	}

	/** Unregister a squad and refresh the derived count. */
	void UnregisterSquad(ASquadController* Squad)
	{
		if (not Squad)
		{
			return;
		}
		M_InsideSquads.Remove(Squad);
		M_CurrentSquads = FMath::Max(0, M_InsideSquads.Num());
	}
};

/**
 * @brief Tracks socket occupancy for squad units.
 *
 * Exposes helpers to list empty seat sockets and to provide a round-robin
 * reuse order. The actual policy (empty-only vs reuse) is decided by callers.
 */
USTRUCT()
struct FCargoSocketOccupancy
{
	GENERATED_BODY()

	/** All cargo socket names discovered on the mesh. */
	TArray<FName> M_AllCargoSockets;

	/** Map of socket -> unit occupying it. */
	UPROPERTY()
	TMap<FName, TObjectPtr<ASquadUnit>> M_SocketToUnit;

	/** Round-robin cursor for reuse when no empty sockets remain. */
	int32 M_NextRoundRobinIndex = 0;

	/** @brief Gather empty sockets (no valid registered unit). */
	TArray<FName> GetEmptySockets() const;

	/**
	 * @brief Clears any occupancy owned by the provided units.
	 * @param Units Units that should release their seats in the map.
	 */
	void ClearUnits(const TArray<ASquadUnit*>& Units)
	{
		if (Units.Num() == 0)
		{
			return;
		}
		TSet<TObjectPtr<ASquadUnit>> UnitSet;
		for (ASquadUnit* U : Units)
		{
			if (U) { UnitSet.Add(U); }
		}
		for (auto It = M_SocketToUnit.CreateIterator(); It; ++It)
		{
			if (UnitSet.Contains(It->Value))
			{
				It.RemoveCurrent();
			}
		}
	}
};

/**
 * @brief Component for cargo providers (buildings, halftracks) that discovers seat/entrance sockets and tracks capacity.
 *
 * Scans a supplied mesh for "cargo" sockets (seating/pose) and "entrance" sockets (one or many),
 * caches them, and exposes helpers to allocate seats to a squad according to the current policy
 * (by default: assign only empty sockets; callers can opt into reuse using FCargoSocketOccupancy if desired).
 *
 * @note SetupCargo: call in blueprint to provide the mesh and max supported squads.
 * @note Do not call setup cargo in the blueprint of hte the nomadic vehicle; it will be called automaitcally in  c++
 * once we know what building mesh will be used.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UCargo : public UActorComponent
{
	GENERATED_BODY()

public:
	UCargo();

	
	/**
	 * @brief Handle the case where all units of a squad died while still inside this cargo.
	 *
	 * Clears seat occupancy for that squad, releases its garrison slot (if any),
	 * and unregisters it from vacancy without applying extra seat deltas.
	 */
	void OnSquadCompletelyDiedInside(ASquadController* SquadController);

	/**
	 * @brief Pushes an occupancy update for a garrison slot (enter or leave) and adjusts seats text accordingly.
	 * @param Squad      The squad that is entering or leaving.
	 * @param SlotIndex  Widget slot index [0..2] for this squad.
	 * @param bIsEntering True when occupying (enter), false when vacating (leave).
	 */
	void UpdateGarrisonWidgetForSquad(ASquadController* Squad, const int32 SlotIndex,
	                                  const bool bIsEntering);
	

	void OnOwnerDies(const ERTSDeathType DeathType);
	void AdjustSeatsForSquad(ASquadController* Squad, int32 SeatsDelta);

	void SetIsEnabled(const bool bEnable);

	bool GetIsEnabledAndVacant() const;

	int32 GetMaxSquadsSupported() const { return M_VacancyState.M_MaxSquadsSupported; }
	int32 GetCurrentSquads() const { return M_VacancyState.M_CurrentSquads; }

	FORCEINLINE FVector GetSeatOffset() const { return M_Offset; }

	/**
	 * @brief Access the cached cargo mesh.
	 * @return The mesh component, or nullptr if not configured.
	 */
	UPrimitiveComponent* GetCargoMesh() const;

	/**
	 * @brief Discover cargo & entrance sockets on the provided mesh and configure capacity.
	 * @param MeshComponent Mesh to scan (weakly cached). Must contain sockets named with "cargo"/"entrance".
	 * @param MaxSupportedSquads Maximum squads allowed simultaneously for this cargo owner.
	 * @param CargoPositionsOffset
	 * @param SeatTextType
	 * @param bStartEnabled
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupCargo(UPrimitiveComponent* MeshComponent, int32 MaxSupportedSquads, const FVector CargoPositionsOffset,
	                const
	                EGarrisonSeatsTextType SeatTextType, const bool bStartEnabled = true);

	/**
	 * @brief Select the closest entrance to a world location.
	 * @param FromWorldLocation Query point (usually squad controller location).
	 * @return World location of the nearest "entrance" socket; falls back to owner location if none exist.
	 */
	FVector GetClosestEntranceWorldLocation(const FVector& FromWorldLocation) const;

	/**
	 * @brief Attempt to allocate seat sockets for the given squad.
	 *
	 * Assigns available **empty** seat sockets to squad units and fills @p OutAssignments
	 * with Unit -> SocketName pairs. Occupied sockets are not reused by default.
	 *
	 * @param SquadController The entering squad controller.
	 * @param OutAssignments Output mapping per unit to the assigned seat socket. May be smaller than the squad size.
	 * @return true if the cargo currently has squad vacancy and cargo sockets exist; false if no vacancy or no sockets.
	 *         Note: OutAssignments can be empty when all seats are taken by others even if this returns true.
	 */
	bool RequestCanEnterCargo(ASquadController* SquadController, TMap<ASquadUnit*, FName>& OutAssignments);

	void OnSquadEntered(ASquadController* SquadController);

	/**
	 * @brief Release all seats held by the provided squad.
	 * @param SquadController Squad that exits the cargo.
	 * @param Units Units belonging to the squad that should clear their seat occupancy.
	 */
	void OnSquadExited(ASquadController* SquadController, const TArray<ASquadUnit*>& Units);

	/**
	 * @brief Validate the cached cargo mesh.
	 * @return true if the mesh pointer is valid; logs an error and returns false otherwise.
	 */
	bool GetIsValidCargoMesh() const;

	/**
	 * @brief Get the world-space transform for a socket on the cached mesh.
	 * @param SocketName Name of the socket to query.
	 * @return World transform if available; otherwise the owner's transform or identity if no owner.
	 */
	FTransform GetSocketWorldTransform(const FName& SocketName) const;

	/** @return Owning actor (shortcut). */
	AActor* GetCargoOwnerActor() const { return GetOwner(); }

protected:
	virtual void BeginPlay() override;

private:
	
	/** Clears seat occupancy for all units that belong to the given squad. */
	void ClearSeatOccupancyForSquad(ASquadController* Squad);
	/** Validated cached mesh. */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> M_CargoMesh;

	/** Names of all discovered "entrance" sockets. */
	UPROPERTY()
	TArray<FName> M_EntranceSockets;

	// Tracks seat occupancy and exposes helpers (empty-socket listing, optional round-robin reuse order).
	UPROPERTY()
	FCargoSocketOccupancy M_SocketOccupancy;

	/** Vacancy state for squads (capacity, inside set, and derived count). */
	UPROPERTY()
	FCargoVacancyState M_VacancyState;

	// Called by the health component via call back once the health component has set up the widget.
	void OnHealthBarWidgetInit(UHealthComponent* InitializedComponent);

	/**
	 * @brief Collect all socket names from the cached mesh.
	 * @param OutAllSockets Output array receiving every socket name on the mesh.
	 */
	void CollectSocketNames(TArray<FName>& OutAllSockets) const;

	/**
	 * @brief Filter socket names by substring (case-insensitive).
	 * @param In Input socket names.
	 * @param Substring Substring to search for (e.g., "cargo", "entrance").
	 * @return All entries of @p In whose name contains @p Substring (case-insensitive).
	 */
	static TArray<FName> FilterSocketsContains(const TArray<FName>& In, const FString& Substring);

	UPROPERTY()
	EGarrisonSeatsTextType M_SeatTextType;
	void RegisterCallBackToInitHealthComponentOnOwner();
	void OnHealthBarWidgetInit_NoValidWidget() const;
	UPROPERTY()
	TWeakObjectPtr<UW_GarrisonHealthBar> M_GarrisonHpBarWidget = nullptr;
	bool EnsureIsValidGarrisonWidget() const;
	void InitGarrisonWidget();


	// Squad -> UI slot (0..MaxSquads-1). Only for widget; seat sockets are tracked elsewhere.
    UPROPERTY()
    TMap<TWeakObjectPtr<ASquadController>, int32> M_SquadToGarrisonSlot;
    
    int32 AssignSlotForSquad(ASquadController* Squad);
    void  ReleaseSlotForSquad(ASquadController* Squad);
    int32 GetSlotIndexForSquad(ASquadController* Squad) const;
	bool GetIsSlotIndexInUse(const int32 SlotIndex) const;
	void SweepInvalidSquadSlots();

	void RegisterSquadAtVacancy(ASquadController* Squad, const bool bRegister);

	void BeginPlay_SetOwnerInterface();

	// Sets up the single per-cargo timer that may reshuffle seat assignments over time.
	void BeginPlay_InitSeatShuffleTimer();

	// Periodic tick, called by the timer, that moves at most one unit to a better free seat.
	void TickSeatReassignment();

	UPROPERTY()
	TScriptInterface<ICargoOwner> M_CargoOwner;
	bool GetIsValidCargoOwner() const;



	// ----------------------------------------
	// --------- Unit dies Helpers ---------
	// ----------------------------------------
	TArray<TObjectPtr<ASquadController>> OnOwnerDies_GetInsideSquadsSnapshot() const;
	void OnOwnerDies_KillAllInsideSquads(const TArray<TObjectPtr<ASquadController>>& InsideSquads);
	void OnOwnerDies_HandleSquad(ASquadController* Squad);
	void OnOwnerDies_KillUnit(ASquadUnit* Unit);
	void OnOwnerDies_ResetState();
	void OnOwnerDies_DetachIfAttachedToOwner(ASquadUnit* Unit);

	// Interval in seconds for automatic seat reshuffle; <= 0 disables reshuffle entirely.
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Optimisation")
	float M_SeatShuffleIntervalSeconds = 0.3f;

	FVector M_Offset = FVector::ZeroVector;

	bool bM_IsEnabled = false;

	// When we disalbe the cargo component move everyone out.
	void MoveOutGarrisonedInfantryOnDisabled();

	
	// Timer handle for dynamic seat reshuffling.
	FTimerHandle M_SeatShuffleTimerHandle;

	// Start / stop seat shuffle timer depending on garrison state.
	void StartSeatShuffleTimer();
	void StopSeatShuffleTimer();
};
