// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "AirBase_SocketRecordAndBayData/SocketRecord.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AirbaseAnimInstance/UAirBaseAnimInstance.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftSocketState/AircraftSocketState.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AirFieldRepairManager/AirFieldRepairManager.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "AircraftOwnerComp.generated.h"

class UMeshComponent;
class USkeletalMeshComponent;
class AAircraftMaster;

/**
 * @brief Per-component tuning for bay doors and roof animation.
 *
 * @note WHY: These values live on the component instance so designers can tune different
 *            buildings without code changes (door timings, ABP class, mesh offset).
 */
USTRUCT(BlueprintType)
struct FAircraftOwnerSettings
{
	GENERATED_BODY()

	/** How long it takes the bay door to animate from closed to open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Airbase")
	float OpenAircraftSocketAnimTime = 1.25f;

	/** How long it takes the bay door to animate from open to closed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Airbase")
	float CloseAircraftSocketAnimTime = 1.1f;

	/** Skeletal mesh that renders/animates the roof doors over the bays. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Airbase")
	USkeletalMesh* AircraftRoofMesh = nullptr;

	/** Animation blueprint class, MUST derive from UAirBaseAnimInstance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Airbase")
	TSubclassOf<UAirBaseAnimInstance> AircraftRoofAnimBPClass;

	/** Relative offset for the roof mesh when attached to the building mesh passed into InitAirbase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Airbase")
	FVector AircraftRoofMeshRelativeLocation = FVector::ZeroVector;
};


/**
 * @brief Owns and arbitrates aircraft bay sockets for a nomadic building (expandable airbase).
 *
 * @note ******************************
 * @note WHY this component exists:
 * @note - Centralizes socket discovery/assignment and door orchestration to avoid race conditions
 * @note   between multiple aircraft (VTO vs. landing).
 * @note - Keeps the roof animation authoritative so aircraft focus on flight logic only.
 * @note - Owns repair ticking for landed aircraft to decouple healing from the unit itself.
 * @note ******************************
 * @note Lifecycle:
 * @note - UnpackAirbase() is called when the nomadic vehicle converts to a building; we discover sockets
 * @note   and spawn the roof mesh/ABP.
 * @note - Pack-up flow evicts aircraft safely (VTO), snapshots state, and can be cancelled/restored cleanly.
 * @note ******************************
 * @note Door policy (WHY):
 * @note - Open/close requests are idempotent and timeboxed; when open completes we grant the pending action
 * @note   (VTO or Landing) to guarantee the bay is physically clear/safe at the exact moment of transition.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAircraftOwnerComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UAircraftOwnerComp();

	// Allow the manager to access M_Sockets (so it can repair/inspect landed aircraft).
	friend struct FAirfieldRepairManager;

	// ---------------- Public API ----------------

	/**
	 * @brief Terminal event for this airbase; notifies all assigned aircraft and clears internal state.
	 *
	 * @post All aircraft drop ownership (aircraft recover to safe state), timers cleared, maps emptied.
	 */
	void OnAirbaseDies();

	/**
	 * @brief Expand the nomadic vehicle into a building and initialize bays/roof.
	 *
	 * @param BuilidngMesh The mesh we attach the roof to and query sockets from.
	 * @param RepairPowerMlt Multiplier for repair strength while landed.
	 * @post Sockets discovered and initialized; roof skeletal mesh + ABP created and defaulted to Closed.
	 */
	void UnpackAirbase(UMeshComponent* BuilidngMesh, const float RepairPowerMlt);

	/** @return Whether packing up is allowed (no aircraft physically inside). */
	bool GetCanPackUp() const;

	/**
	 * @brief Begin pack-up: evict aircraft, snapshot assignments, and switch roof to construction visuals.
	 *
	 * @param NomadicConstructionMaterial Material applied during pack animation.
	 * @post All assigned aircraft VTO (door arbitration handled); assignments are cleared but recorded so cancel can restore.
	 */
	void OnPackUpAirbaseStart(UMaterialInstance* NomadicConstructionMaterial);

	/**
	 * @brief Cancel pack-up and restore the previous bay/assignment state instantly (no anim waits).
	 *
	 * @post Doors resolve to logical end states (Closed or Open) without montages; aircraft re-assigned; materials restored.
	 */
	void OnPackUpAirbaseCancelled();

	/**
	 * @brief Finalize pack-up by destroying the roof and marking as vehicle.
	 *
	 * @pre GetCanPackUp() == true.
	 * @post Roof mesh/anim destroyed; packing records cleared; state set to PackedUp_Vehicle.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Airbase")
	void OnPackUpAirbaseComplete();

	/** @return True if all sockets are reserved or occupied (landed/assigned/training). */
	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="Airbase")
	bool IsAirBaseFull() const;

	/** @return True if the airbase is currently packed up as a vehicle. */
	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="Airbase")
	inline bool IsAirbasePackedUp() const { return M_PackingState == EAirbasePackingState::PackedUp_Vehicle; }

	/** @return True if the airbase is expanded as a building. */
	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="Airbase")
	inline bool IsAirbaseUnPackedAsBuilding() const
	{
		return M_PackingState == EAirbasePackingState::Unpacked_Building;
	}

	/**
	 * @brief Reserve a socket for a soon-to-spawn aircraft of the given type (training queue).
	 *
	 * @param AircraftType Subtype to reserve for.
	 * @return True if reservation succeeded.
	 */
	bool OnAircraftTrainingStart(const EAircraftSubtype AircraftType);

	/**
	 * @brief Release the oldest reservation for this type when training is cancelled.
	 *
	 * @param AircraftType Subtype whose reservation should be freed.
	 */
	void OnAircraftTrainingCancelled(const EAircraftSubtype AircraftType);

	/**
	 * @brief Trainer delivered an aircraft: assign it to a socket, place it, and mark “inside”.
	 *
	 * @param TrainedAircraft The spawned aircraft.
	 * @param AircraftType Subtype used to match a reservation (falls back to first free).
	 * @return true if assignment and placement succeeded.
	 * @post Repairs start ticking (we landed), doors may open proactively for immediate VTO.
	 */
	bool OnAircraftTrained(AAircraftMaster* TrainedAircraft, const EAircraftSubtype AircraftType);

	/**
	 * @brief An orphan or foreign aircraft is switching to this owner; assign it to a free socket.
	 *
	 * @param Aircraft The aircraft requesting ownership change.
	 * @return true on successful assignment.
	 */
	bool OnAircraftChangesToThisOwner(AAircraftMaster* Aircraft);

	/**
	 * @brief Mark an assigned aircraft as inside (landed) to enable repair ticks.
	 *
	 * @param Aircraft The aircraft that just landed in this bay.
	 */
	void OnAircraftLanded(AAircraftMaster* Aircraft);

	/**
	 * @brief Aircraft requests lift-off: open door (if needed) and grant VTO when open completes.
	 *
	 * @param Aircraft Requesting aircraft (must be assigned).
	 * @return true if the request is valid and will be granted (immediately or after opening).
	 */
	bool OnAircraftRequestVTO(AAircraftMaster* Aircraft);

	/**
	 * @brief Cancel a pending landing request and optionally close the door (unless packing up).
	 *
	 * @param Aircraft Target aircraft.
	 * @param bCloseDoor If true, attempt to close if not in PackingUp.
	 * @return true if the pending landing was cleared.
	 */
	bool CancelPendingLandingFor(class AAircraftMaster* Aircraft, bool bCloseDoor);

	/**
	 * @brief Ensure an aircraft may land here (assigns a free socket if unknown and space exists).
	 *
	 * @param Aircraft Aircraft intending to land.
	 * @return true if assigned and permitted to land.
	 */
	bool IsAircraftAllowedToLandHere(AAircraftMaster* Aircraft);

	/**
	 * @brief Request doors to open for landing for the assigned socket; grants landing on open.
	 *
	 * @param Aircraft Aircraft that will land.
	 * @param bOutDoorsAlreadyOpen Output flag if the door was already open.
	 * @return true if the open request was accepted.
	 */
	bool RequestOpenBayDoorsForLanding(AAircraftMaster* Aircraft, bool& bOutDoorsAlreadyOpen);

	/**
	 * @brief Open doors without enqueuing VTO/landing actions (pure visual/state transition).
	 *
	 * @param Aircraft The requesting aircraft (must be assigned).
	 * @param bOutDoorsAlreadyOpen Whether the bay was already open.
	 * @param OpeningAnimTime Returns the time used for the opening animation.
	 * @return True if accepted (aircraft assigned here), false otherwise.
	 */
	bool RequestOpenBayDoorsNoPendingAction(AAircraftMaster* Aircraft, bool& bOutDoorsAlreadyOpen,
	                                        float& OpeningAnimTime);


	// ----------------- Utility for aircraft -----------------

	/** @return Assigned socket index for this aircraft or INDEX_NONE if unassigned. */
	int32 GetAssignedSocketIndex(const AAircraftMaster* Aircraft) const;

	/** @return Socket world location if index valid. */
	bool GetSocketWorldLocation(const int32 SocketIndex, FVector& OutWorldLocation) const;

	/** @return Socket world rotation if index valid. */
	bool GetSocketWorldRotation(const int32 SocketIndex, FRotator& OutWorldRotation) const;

	/**
	 * @brief Remove an aircraft from this owner (clears maps, unbinds, and closes doors).
	 *
	 * @param Aircraft The aircraft to remove.
	 * @return Whether the aircraft was part of this owner and removed successfully.
	 * @post Door is requested to close for that socket.
	 */
	bool RemoveAircraftFromBuilding_CloseBayDoors(AAircraftMaster* Aircraft);

	/** @return Amount of discovered/managed pads. */
	int32 GetAmountAirPads() const { return M_Sockets.Num(); }

protected:
	/** @brief Clear door timers, then destroy component resources safely. */
	virtual void BeginDestroy() override;

	/** @brief Ensure OnAirbaseDies was processed on EndPlay to avoid leaks on map teardown. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief Mirror EndPlay safeguard for component-level destruction. */
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** @brief Exists primarily for debug overlays/inspection. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ---------- Validation helpers (rule 0.5) ----------
	/** @return Building mesh validity; errors once at the source. */
	bool GetIsValidBuildingMesh() const;
	/** @return Roof skeletal mesh validity; errors once at the source. */
	bool GetIsValidRoofSkelMesh() const;
	/** @return Roof anim instance validity (must be UAirBaseAnimInstance). */
	bool GetIsValidRoofAnimInstance() const;

	// ---------- Internal data ----------
	UPROPERTY()
	UMeshComponent* M_BuildingMesh = nullptr;

	UPROPERTY()
	USkeletalMeshComponent* M_RoofMeshComp = nullptr;

	UPROPERTY()
	UAirBaseAnimInstance* M_RoofAnimInst = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Airbase")
	FAircraftOwnerSettings M_Settings;

	/** @brief Owns repair timer/logic; heals any landed aircraft. */
	FAirfieldRepairManager M_AirfieldRepairManager;

	// Set to Unpacked_Building once the roof is created in the init function.
	// Set to PackingUp when converting to vehicle, a cancel sets it to Unpacked_Building again.
	// Set to PackedUp_Vehicle once the roof is destroyed in the packup function.
	UPROPERTY()
	EAirbasePackingState M_PackingState = EAirbasePackingState::Unpacked_Building;

	/** @brief Indexed list of bay records (derived from Landing_* socket names). */
	UPROPERTY()
	TArray<FSocketRecord> M_Sockets;

	/** @brief Fast lookup Aircraft -> SocketIndex for assignment queries. */
	UPROPERTY()
	TMap<TWeakObjectPtr<AAircraftMaster>, int32> M_AircraftToSocketIndex;

	// ---------- Discovery / assignment ----------
	/**
	 * @brief Scan the building mesh for sockets named Landing* and build compact records.
	 *
	 * @note WHY: Keeps bay ordering deterministic across static/skeletal meshes.
	 */
	void DiscoverLandingSockets();

	/** @return true and index if SocketName matches "Landing" + number. */
	static bool ParseLandingIndex(const FName& SocketName, int32& OutIndex);

	/** @return First free (non-assigned, non-reserved) socket index or INDEX_NONE. */
	int32 FindFirstFreeSocketIndex() const;

	/** @return Oldest reserved socket index for type or INDEX_NONE. */
	int32 FindFirstReservedForType(const EAircraftSubtype Type) const;

	/** @return Assigned socket index for aircraft or INDEX_NONE. */
	int32 FindAssignedIndex(const AAircraftMaster* Aircraft) const;

	/** @return Whether Index is valid for M_Sockets; errors with context if not. */
	bool EnsureSocketIndexValid(const int32 Index, const FString& Context) const;

	// ---------- Door control ----------
	/**
	 * @brief Open the door if not already opening/open; when open completes, grant pending actions.
	 *
	 * @param SocketIndex Target socket.
	 */
	void RequestOpenIfNeeded(const int32 SocketIndex);

	/**
	 * @brief Close the door if not already closing/closed (no action grant on close).
	 *
	 * @param SocketIndex Target socket.
	 */
	void RequestCloseIfNeeded(const int32 SocketIndex);

	/**
	 * @brief Called when opening has finished; grants either pending VTO or pending Landing.
	 *
	 * @param SocketIndex Target socket.
	 */
	void OnDoorOpened_GrantPending(const int32 SocketIndex);

	/**
	 * @brief Plays the montage for the target door state and arms a timer to resolve final state.
	 *
	 * @param SocketIndex Which bay to animate.
	 * @param NewState Opening or Closing.
	 * @param PlayTime Duration to play before resolving to Open/Closed.
	 */
	void PlayDoorMontage(const int32 SocketIndex, const EAircraftSocketState NewState, const float PlayTime);

	// ---------- State updates ----------
	/**
	 * @brief Assign an aircraft to a socket and bind its death; owner pointer is set on the aircraft.
	 *
	 * @param Aircraft Aircraft to assign.
	 * @param SocketIndex Valid socket index.
	 * @pre Assumes the aircraft has NO other owner set.
	 */
	void AssignAircraftToSocket(AAircraftMaster* Aircraft, const int32 SocketIndex);

	/** @return True if an existing assignment for Aircraft was cleared; outputs old index. */
	bool ClearAssignmentForAircraft(AAircraftMaster* Aircraft, int32& OutSocketIndex);

	/** @return True if the record at index was cleared (maps, flags, pending). */
	bool ClearSocketRecByIndex(const int32 SocketIndex);

	// ---------- Delegate handling ----------
	/**
	 * @brief Track OnUnitDies on the aircraft so we can free the bay even if the pointer becomes invalid.
	 *
	 * @param Aircraft Aircraft to bind to.
	 * @param SocketIndex Its socket index snapshot.
	 */
	void BindDeath(AAircraftMaster* Aircraft, int32 SocketIndex);

	/** @brief Unbind previously tracked death delegate for this aircraft. */
	void UnbindDeath(AAircraftMaster* Aircraft);

	/** @brief Callback variant when we only have a weak aircraft pointer. */
	void OnBoundAircraftDied(TWeakObjectPtr<AAircraftMaster> WeakAircraft);

	bool bM_DeathProcessed = false; // guard against double calls

	// Track per-aircraft delegate handles so we can unbind cleanly:
	TMap<TWeakObjectPtr<class AAircraftMaster>, FDelegateHandle> M_DeathDelegateHandles;

	// ---------- Errors ----------
	/** @brief Compact error helper for unexpected state mixes. */
	void ReportInvalidOwnerState(const FString& Context) const;

	// ---------- Close helpers ----------
	/** @return True if any assigned aircraft is presently inside (landed). */
	bool HasAnyAircraftInside() const;

	/** @brief Debug text overlays for per-socket state when needed. */
	void DebugTick(const float DeltaTime);

	// Contains the aircraft that were associated with this owner
	TArray<FAircraftRecordPacking> M_PackingRecords;

	// Caches the materials when packing up, in case we need to reapply them after pack up is cancelled.
	UPROPERTY()
	TArray<UMaterialInterface*> M_RoofMaterials;

	/** @brief Swap roof materials to construction style during pack-up. */
	void OnPackUp_SetRoofToConstruction(UMaterialInstance* NomadicConstructionMaterial);

	/** @brief Restore original roof materials when pack-up is cancelled. */
	void OnPackupCancelled_RestoreRoofMaterials();
};
