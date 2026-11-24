// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyBehaviourTypes.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"

#include "Convoy.generated.h"

class AConvoy;
class AScavengeableObject;
class ASquadController;
class ATankMaster;
class ICommands;
class UConvoyDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConvoyEventNative, const FConvoyEvent& /*Event*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConvoyEventDynamic, const FConvoyEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConvoyFinishedDynamic, AConvoy*, Convoy, EConvoyOutcome, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConvoyTruckCapturedDynamic, AConvoy*, Convoy, AActor*, Truck);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnConvoyBehaviourEventNative, const FConvoyBehaviourEvent& /*Event*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConvoyBehaviourEventDynamic, const FConvoyBehaviourEvent&, Event);

/** @brief Runtime state of one unit in a convoy. */
USTRUCT()
struct FConvoyMember
{
	GENERATED_BODY()

	int32 MemberId = INDEX_NONE;
	EConvoyUnitRole Role = EConvoyUnitRole::Truck;
	EConvoyStation Station = EConvoyStation::Lead;
	EConvoyMemberState State = EConvoyMemberState::Spawning;
	FString UnitName;

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> Tank;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> Squad;

	// Slot relative to the column reference: along the route (negative = behind) and sideways (positive = right).
	float SlotAlongOffset = 0.f;
	float SlotLateralOffset = 0.f;
	FString SlotName;

	// Where the unit is along the route; RouteDistance doubles as the projection hint.
	float RouteDistance = 0.f;
	float LateralOffset = 0.f;
	FVector Location = FVector::ZeroVector;

	// Vehicles: unrestricted top speed (cm/s) from the unit data. Squads: their own march speed.
	float NominalSpeed = 0.f;
	// Trucks: measured free cruise speed the pace fraction applies to (falls back to NominalSpeed).
	float CalibratedSpeed = 0.f;
	// Learned ratio between the restriction asked for and the ground speed it gives (closed loop).
	float SpeedTrim = 1.f;
	float WantedSpeed = 0.f;
	float AppliedSpeedFraction = 1.f;
	float AppliedMarchSpeed = 0.f;
	float MeasuredSpeed = 0.f;
	FVector PreviousLocation = FVector::ZeroVector;
	double PreviousSampleTime = -1.0;

	bool bHasOrder = false;
	FVector LastOrderTarget = FVector::ZeroVector;
	double LastOrderTime = -1.0;

	// Stuck detection window.
	FVector StuckWindowLocation = FVector::ZeroVector;
	double StuckWindowStart = -1.0;
	int32 StuckAttempts = 0;
	bool bIsStuck = false;
	double StuckSince = -1.0;
	bool bHasPartialPath = false;

	// When an engaging escort last got its attack order.
	double LastEngageOrderTime = -1.0;
	double SpawnRequestTime = -1.0;
	FDelegateHandle DeathHandle;

	// Trucks: the definition's cargo or the truck spec's override; decides whether the truck takes the branch.
	EConvoyCargoType CargoType = EConvoyCargoType::Fuel;
	// Trucks: what became of it (counted for FConvoyCargoOutcome).
	EConvoyTruckFate Fate = EConvoyTruckFate::None;
	FConvoyMemberBranchState Branch;

	bool GetIsAlive() const;
	bool GetIsVehicle() const { return Role != EConvoyUnitRole::EscortSquad; }
	// A truck that turned off at the fork; it no longer keeps a slot in the column.
	bool GetIsOnBranch() const { return State == EConvoyMemberState::Branching; }
	// A living truck that still drives in the column (not on the branch).
	bool GetIsColumnTruck() const { return Role == EConvoyUnitRole::Truck and not GetIsOnBranch() and GetIsAlive(); }
	ICommands* GetCommands() const;
};

/** @brief A cargo cache a destroyed truck left behind. */
USTRUCT()
struct FConvoyCargoCache
{
	GENERATED_BODY()

	int32 CacheId = INDEX_NONE;
	int32 TruckMemberId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AScavengeableObject> Cache;

	FConvoyCargoValue Value;
	FVector Location = FVector::ZeroVector;
	double SpawnTime = -1.0;
	FName IconId = NAME_None;
	FTimerHandle ExpiryTimer;
	// Resolved: scavenged by a player unit or destroyed; bScavengeCompleted tells which.
	bool bScavenged = false;
	bool bScavengeCompleted = false;
	bool bExpired = false;
	bool bSpawnFailed = false;
};

/**
 * @brief A column of cargo trucks with an escort that drives a route as one unit: trucks at a fraction of their
 * top speed, escort vehicles limited to the trucks' actual speed, squads marching beside them; fights back, regroups,
 * leaves scavengeable cargo when trucks die and vanishes at the exit.
 * Start one with AConvoy::StartConvoy(Definition, Route, Params); listen to OnConvoyEventNative / OnConvoyEvent.
 * Game-mode free: it knows nothing about survival or missions.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AConvoy : public AActor
{
	GENERATED_BODY()

public:
	AConvoy();

	/**
	 * @brief Spawns a convoy at the start of Route; units are spawned asynchronously, the column starts once
	 * they stand on their slots.
	 * @param WorldContextObject Any object in the world.
	 * @param Definition Trucks, guards, cargo and behaviour.
	 * @param Route Drivable point list, start to exit (UConvoyRouteLibrary::BuildNavigableRoute).
	 * @param Params Owner, guard tier, cargo multiplier.
	 * @return The convoy, or nullptr when the definition or route is unusable (reported).
	 */
	UFUNCTION(BlueprintCallable, Category = "Convoy", meta = (WorldContext = "WorldContextObject"))
	static AConvoy* StartConvoy(const UObject* WorldContextObject, UConvoyDefinition* Definition,
	                            const FConvoyRoute& Route, const FConvoyStartParams& Params);

	/** @brief Ends the convoy now; bDespawnUnits removes the units, otherwise they are released where they stand. */
	UFUNCTION(BlueprintCallable, Category = "Convoy")
	void EndConvoy(bool bDespawnUnits);

	UFUNCTION(BlueprintPure, Category = "Convoy")
	EConvoyState GetConvoyState() const { return M_State; }

	UFUNCTION(BlueprintPure, Category = "Convoy")
	EConvoyOutcome GetOutcome() const { return M_Outcome; }

	UFUNCTION(BlueprintPure, Category = "Convoy")
	int32 GetConvoyId() const { return M_ConvoyId; }

	/** @return True while the column drives or its trucks are being fought over. */
	UFUNCTION(BlueprintPure, Category = "Convoy")
	bool GetIsColumnActive() const { return M_Outcome == EConvoyOutcome::None; }

	UFUNCTION(BlueprintPure, Category = "Convoy")
	int32 GetAliveTruckCount() const;

	UFUNCTION(BlueprintPure, Category = "Convoy")
	FVector GetColumnCentre() const;

	/** @return Caches still waiting to be scavenged. */
	UFUNCTION(BlueprintPure, Category = "Convoy")
	int32 GetOpenCacheCount() const;

	/** @return Total cargo the whole convoy carries (all trucks). */
	UFUNCTION(BlueprintPure, Category = "Convoy")
	FConvoyCargoValue GetTotalCargo() const { return M_TotalCargo; }

	const UConvoyDefinition* GetDefinition() const { return M_Definition.Get(); }
	const FConvoyRoute& GetRoute() const { return M_Route; }
	const FConvoyRoutePolyline& GetRoutePolyline() const { return M_Polyline; }
	const FConvoyStartParams& GetStartParams() const { return M_Params; }
	const TArray<FConvoyCargoCache>& GetCargoCaches() const { return M_Caches; }
	float GetColumnReferenceDistance() const { return M_ColumnReference; }
	float GetPaceSpeed() const { return M_PaceSpeed; }
	float GetMeasuredTruckSpeed() const { return M_MeasuredTruckSpeed; }
	void GetMemberSnapshots(TArray<FConvoyMemberSnapshot>& OutSnapshots) const;

	/** @return The units of the convoy that are still alive (tanks and squad controllers). */
	TArray<AActor*> GetAliveUnits() const;

	/** @return Living escort vehicles and squads (members still spawning do not count). */
	UFUNCTION(BlueprintPure, Category = "Convoy")
	int32 GetAliveEscortCount() const;

	/** @return True when a living escort stands within Radius (2D) of Location. */
	bool GetIsAnyEscortAliveNear(const FVector& Location, float Radius) const;

	/** @return Trucks that surrendered and left the convoy alive; they count neither as destroyed nor as escaped. */
	UFUNCTION(BlueprintPure, Category = "Convoy")
	int32 GetCapturedTruckCount() const { return M_CapturedTruckCount; }

	/** @return One truck's share of the convoy's cargo (what its cache pays). */
	const FConvoyCargoValue& GetCargoPerTruck() const { return M_CargoPerTruck; }

	/** @return True while Unit is a spawned, living member of this convoy that has not been released. */
	bool GetIsActiveMember(const AActor* Unit) const;

	/**
	 * @brief Hands a living truck over to whoever captured it: it leaves the column alive and counts as captured.
	 * The convoy never captures a truck by itself; only trucks that can surrender call this.
	 * @param Truck The truck actor of this convoy.
	 * @return True when Truck was an active truck of this convoy and is now released as captured.
	 */
	bool ReleaseCapturedTruck(AActor* Truck);

	/** @return The running convoy Unit is an active member of, or nullptr. */
	static AConvoy* FindConvoyOfActiveMember(const AActor* Unit);

	// ---------------- Mission behaviours (ConvoyBehaviours.cpp)

	/**
	 * @brief Gives the convoy its optional mission behaviours: a timed stop, a branch for one cargo type, ambush
	 * zones with a radio speed-up and a hunter dismount point. Call right after StartConvoy (AEnemyConvoyRoute does).
	 * @param Plan Resolved route distances and world points; parts that are not enabled do nothing.
	 */
	void SetBehaviourPlan(const FConvoyBehaviourPlan& Plan);

	/** @brief Ends a stop that is still coming or running (reason Cancelled); the column drives on without flooring it. */
	UFUNCTION(BlueprintCallable, Category = "Convoy|Behaviours")
	void CancelStop();

	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	EConvoyStopPhase GetStopPhase() const { return M_BehaviourRuntime.StopPhase; }

	/** @return True while the trucks drive faster after an attack spoiled their stop. */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	bool GetIsFloorItActive() const;

	/** @return Multiplier on the trucks' pace now: flooring it or a radio-boosted ambush zone (the larger one). */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	float GetTruckSpeedMultiplier() const;

	/** @return Trucks that left the map at the route exit. */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	int32 GetEscapedTruckCount() const { return CountTrucksWithFate(EConvoyTruckFate::Escaped); }

	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	int32 GetDestroyedTruckCount() const { return CountTrucksWithFate(EConvoyTruckFate::Destroyed); }

	/** @return Trucks that left at the fork and reached the branch destination. */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	int32 GetBranchDeliveredTruckCount() const { return CountTrucksWithFate(EConvoyTruckFate::BranchDelivered); }

	/** @return Living trucks that drive the branch path now. */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	int32 GetActiveBranchTruckCount() const;

	/** @return Truck fates so far, in total and per cargo type (complete once the convoy finished). */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	FConvoyCargoOutcome GetCargoOutcome() const;

	/**
	 * @brief What one truck of this convoy carries (its spec's override or the definition's cargo).
	 * @param Truck A truck actor of this convoy, alive or not.
	 * @param OutCargoType Receives the cargo type.
	 * @return False when Truck is no truck of this convoy.
	 */
	UFUNCTION(BlueprintPure, Category = "Convoy|Behaviours")
	bool GetTruckCargoType(const AActor* Truck, EConvoyCargoType& OutCargoType) const;

	FOnConvoyEventNative OnConvoyEventNative;

	FOnConvoyBehaviourEventNative OnBehaviourEventNative;

	// Stop, branch, ambush zone and dismount events with their typed data.
	UPROPERTY(BlueprintAssignable, Category = "Convoy|Behaviours")
	FOnConvoyBehaviourEventDynamic OnBehaviourEvent;

	UPROPERTY(BlueprintAssignable, Category = "Convoy")
	FOnConvoyEventDynamic OnConvoyEvent;

	UPROPERTY(BlueprintAssignable, Category = "Convoy")
	FOnConvoyFinishedDynamic OnConvoyFinished;

	// A truck surrendered and left the convoy alive; it now belongs to its captor.
	UPROPERTY(BlueprintAssignable, Category = "Convoy")
	FOnConvoyTruckCapturedDynamic OnTruckCaptured;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UConvoyDefinition> M_Definition;

	FConvoyRoute M_Route;
	FConvoyRoutePolyline M_Polyline;
	FConvoyStartParams M_Params;
	int32 M_ConvoyId = INDEX_NONE;
	EConvoyState M_State = EConvoyState::Spawning;
	EConvoyOutcome M_Outcome = EConvoyOutcome::None;

	UPROPERTY()
	TArray<FConvoyMember> M_Members;

	UPROPERTY()
	TArray<FConvoyCargoCache> M_Caches;

	FConvoyCargoValue M_TotalCargo;
	FConvoyCargoValue M_CargoPerTruck;

	// Column reference: where the first truck's slot is along the route; every slot hangs off it.
	float M_ColumnReference = 0.f;
	// Pace: the slowest truck's TruckSpeedFraction × top speed (cm/s).
	float M_PaceSpeed = 0.f;
	// Smoothed mean ground speed of the trucks that hold the column.
	float M_MeasuredTruckSpeed = 0.f;
	int32 M_PendingSpawns = 0;
	double M_StateStartTime = 0.0;
	double M_LastThreatTime = -1.0;
	FVector M_LastThreatLocation = FVector::ZeroVector;
	FName M_ConvoyIconId = NAME_None;
	TWeakObjectPtr<AActor> M_IconActor;
	FTimerHandle M_TickTimer;
	FTimerHandle M_SelfDestroyTimer;
	int32 M_NextCacheId = 0;
	int32 M_CapturedTruckCount = 0;

	UPROPERTY()
	FConvoyBehaviourPlan M_BehaviourPlan;

	FConvoyBehaviourRuntime M_BehaviourRuntime;

	// Built from M_BehaviourPlan.Branch.BranchPoints; branch trucks are steered and measured along it.
	FConvoyRoutePolyline M_BranchPolyline;

	static int32 M_NextFreeConvoyId;

	bool InitConvoy(UConvoyDefinition* Definition, const FConvoyRoute& Route, const FConvoyStartParams& Params);
	bool GetIsValidDefinition() const;
	void SetState(EConvoyState NewState, const FString& Reason);
	void Broadcast(EConvoyEventType Type, const FConvoyMember* Member, const FString& Detail,
	               const FVector& Location = FVector::ZeroVector);
	double GetNow() const;

	// ---------------- Spawning (ConvoySpawning.cpp)
	void BuildMembersFromDefinition();
	void AddMember(EConvoyUnitRole MemberRole, EConvoyStation Station, const FString& UnitName);
	void AssignSlots();
	void AssignStationSlots(EConvoyStation Station, float RowStart, float RowDirection);
	float GetTruckColumnLength() const;
	void SpawnMembers();
	void SpawnMember(int32 MemberIndex, const FTrainingOption& Option, const TSoftClassPtr<AActor>& ExactClass);
	void OnMemberSpawned(int32 MemberId, AActor* SpawnedActor);
	void SetupSpawnedMember(FConvoyMember& Member, AActor* SpawnedActor);
	float ReadNominalSpeed(const FConvoyMember& Member) const;
	void BindMemberDeath(FConvoyMember& Member);
	void OnSpawningComplete();
	FConvoyMember* FindMember(int32 MemberId);
	const FConvoyMember* FindActiveMemberByActor(const AActor* Unit) const;

	// ---------------- Column (ConvoyColumn.cpp)
	void TickConvoy();
	void UpdateMemberSamples(double Now);
	void UpdateWaitingSquads(double Now);
	void UpdateColumnReference();
	void UpdateTruckSpeeds();
	void UpdateEscortSpeeds();
	float GetEscortWantedSpeed(const FConvoyMember& Member) const;
	void ApplyWantedSpeed(FConvoyMember& Member);
	void UpdateSpeedTrim(FConvoyMember& Member) const;
	void UpdateOrders(double Now);
	void UpdateStationOrder(FConvoyMember& Member, double Now);
	FVector GetSlotLocation(const FConvoyMember& Member) const;
	FVector GetOrderTarget(const FConvoyMember& Member) const;
	void IssueMove(FConvoyMember& Member, const FVector& Target, bool bAttackMove, double Now);
	void IssueSquadMarch(FConvoyMember& Member, const FVector& Target, double Now);
	void UpdateStuck(double Now);
	void HandleStuckMember(FConvoyMember& Member, double Now);
	void UpdatePathState(FConvoyMember& Member) const;
	void UpdateMinimapIcon();
	void CheckForExit();
	bool GetIsTruckHoldingColumn(const FConvoyMember& Member, double Now) const;
	bool GetShouldTrucksHalt(double Now) const;
	/** @return True while a surrendering truck holds itself in place (scuttle countdown, surrendered): no orders, no stuck. */
	bool GetIsMemberHeldInPlace(const FConvoyMember& Member) const;

	// ---------------- Combat (ConvoyCombat.cpp)
	void UpdateCombat(double Now);
	bool FindNearestThreat(FVector& OutThreatLocation) const;
	bool GetIsAnyMemberInCombat() const;
	void EngageEscorts(double Now);
	void UpdateEngagedEscorts(double Now);
	void EndCombat(double Now);
	void UpdateTrucksLost(double Now);
	/** @return Bits of the players the convoy's owner fights; a convoy without a valid owner counts as the first AI player's. */
	uint8 GetHostilePlayersMask() const;

	// ---------------- Members and cargo (ConvoyCargo.cpp)
	void UpdateDeaths();
	void OnMemberDied(int32 MemberId);
	void HandleTruckLost(FConvoyMember& Member);
	// The column has no truck left (destroyed or captured): the escorts stop keeping station.
	void OnLastTruckGone(const FString& Reason);
	/** @return TrucksStopped when any truck was captured, else TrucksDestroyed. */
	EConvoyOutcome GetOutcomeWithoutTrucks() const;
	void RemoveConvoyIconIfAttachedTo(const AActor* Actor);
	void ScheduleCargoCache(const FConvoyMember& Truck);
	void SpawnCargoCache(int32 CacheId);
	void OnCacheScavenged(int32 CacheId);
	void OnCacheScavengedOrDestroyed(int32 CacheId);
	void ResolveCache(int32 CacheId);
	void OnCacheExpired(int32 CacheId);
	FConvoyCargoCache* FindCache(int32 CacheId);
	void ReleaseMember(FConvoyMember& Member, bool bDespawn);
	void FinishConvoy(EConvoyOutcome Outcome, bool bDespawnUnits);
	void TryDestroySelfWhenCachesResolved();
	void RemoveAllMinimapIcons();

	// ---------------- Behaviours (ConvoyBehaviours.cpp)
	// Every state after Spawning: branch trucks drive on while the column fights, regroups or waits at the exit.
	void UpdateBehavioursAnyState(double Now);
	// Cruise, Regroup and UnderAttack only: stop, ambush zones and the hunter dismount.
	void UpdateBehaviours(double Now);
	/** @return True while the convoy is under attack, or a member is in combat (long-range fire). */
	bool GetIsAttackedForBehaviours(double Now) const;
	/** @return Furthest distance along the route any column truck puts the column reference at (the front). */
	float GetFrontColumnReference() const;
	void UpdateStop(double Now);
	void UpdateStopBrakeTimeout(float FrontReference, double Now);
	void StartStop(double Now);
	void EndStop(EConvoyStopEndReason Reason, double Now);
	void StartFloorIt(double Now);
	bool GetIsStopLimitingSpeed() const;
	/** @return WantedSpeed capped so the truck rolls up to its slot behind the stop point and stands there. */
	float GetStopLimitedSpeed(const FConvoyMember& Member, float WantedSpeed) const;
	bool GetIsDismountHaltActive(double Now) const;
	void UpdateAmbushZones();
	bool GetIsInsideBoostedAmbushZone() const;
	bool GetIsRadioPostAlive() const;
	void UpdateHunters(double Now);
	void TriggerHuntersDismount(const FString& Reason, double Now);
	void UpdateBranchTrucks(double Now);
	bool GetCanTruckTakeBranch(const FConvoyMember& Member) const;
	void StartBranch(FConvoyMember& Member, double Now);
	void DriveBranchTruck(FConvoyMember& Member, double Now);
	/** @return True when a nudged re-path order was given this tick. */
	bool UpdateBranchTruckStuck(FConvoyMember& Member, double Now);
	void IssueBranchMove(FConvoyMember& Member, const FVector& Target, double Now);
	void OnBranchTruckArrived(FConvoyMember& Member);
	int32 GetAliveColumnTruckCount() const;
	int32 CountTrucksWithFate(EConvoyTruckFate Fate) const;
	void MarkTruckFate(int32 MemberId, EConvoyTruckFate Fate);
	void MarkAliveTrucksEscaped();
	/** @return Escaped when a truck escaped at the exit or every truck delivered on the branch, else the plain outcome. */
	EConvoyOutcome GetFinalOutcomeWithoutTrucks() const;
	void BroadcastBehaviour(FConvoyBehaviourEvent& Event, EConvoyEventType EventLogType, const FConvoyMember* Member,
	                        const FString& Detail);
};
