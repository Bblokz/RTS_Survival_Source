// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	bool GetIsAlive() const;
	bool GetIsVehicle() const { return Role != EConvoyUnitRole::EscortSquad; }
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

	FOnConvoyEventNative OnConvoyEventNative;

	UPROPERTY(BlueprintAssignable, Category = "Convoy")
	FOnConvoyEventDynamic OnConvoyEvent;

	UPROPERTY(BlueprintAssignable, Category = "Convoy")
	FOnConvoyFinishedDynamic OnConvoyFinished;

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

	// ---------------- Combat (ConvoyCombat.cpp)
	void UpdateCombat(double Now);
	bool FindNearestThreat(FVector& OutThreatLocation) const;
	bool GetIsAnyMemberInCombat() const;
	void EngageEscorts(double Now);
	void UpdateEngagedEscorts(double Now);
	void EndCombat(double Now);
	void UpdateTrucksLost(double Now);
	uint8 GetHostilePlayer() const;

	// ---------------- Members and cargo (ConvoyCargo.cpp)
	void UpdateDeaths();
	void OnMemberDied(int32 MemberId);
	void HandleTruckLost(FConvoyMember& Member);
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
};
