// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CargoParachuteDescentComponent.h"
#include "ParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "GA_Paratroopers.generated.h"

class AAircraftMaster;
class AParachuteJumperProxy;
class ASquadController;
class ATeamWeapon;
class ATeamWeaponController;
class ASquadUnit;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FParatroopersGlobalAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AAircraftMaster> AircraftClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ESquadSubtype> SquadSubtypes;

	// Seconds between two soldiers jumping out of the aircraft.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float DropIntervalPerUnit = 0.45f;

	// Soldiers land on a spiral around the target point, at most this far from it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float LandingSpreadRadius = 750.0f;

	// How far below the aircraft a soldier appears when he jumps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float JumpBelowAircraftOffset = 250.0f;

	// Flight distance after the target point before the aircraft disappears; 0 uses the approach distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ExitDistanceAfterTarget = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParatrooperDescentSettings DescentSettings;

	// Hidden squads wait on a ring of this radius around the target; a team weapon squad's gun lands where its squad
	// waited, so drops of several guns need a wider ring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SquadSpawnRingRadius = 200.0f;

	// Raises the transport to at least this height above the target, so big cargo canopies have room to open and
	// fall; 0 keeps the mission's aircraft height. Cinematic passes keep their own start.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinFlightHeightAboveTarget = 0.0f;

	// Team weapon squads only: the gun comes down on its own cargo chute (jumps before its crew).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCargoParachuteDescentSettings TeamWeaponCargoDescent;
};

/**
 * @brief Designer ability: a transport flies straight over the target at full speed; above it the configured squads
 * are spawned hidden and their soldiers jump one by one, each descending on his own parachute.
 * A squad is blocked from commands until all its soldiers landed.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_Paratroopers : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void BeginDestroy() override;

	/**
	 * @brief Gives cinematic callers a repeatable approach without changing the mission's aircraft spawn settings.
	 * @param TargetLocation Ground centre of the drop.
	 * @param AircraftStartLocation Start of the straight flight, including its altitude.
	 */
	void ExecuteCinematicDrop(const FVector& TargetLocation, const FVector& AircraftStartLocation);
	AAircraftMaster* GetActiveDropAircraft() const;
	TArray<TWeakObjectPtr<AAircraftMaster>> GetActiveDropAircrafts() const;
	/** Configure only a cinematic ability copy before starting its passes; zero preserves ordinary mission landings. */
	void SetCinematicLandingTrailLength(float TrailLength);
	/** Positive values override jump spacing on a cinematic copy; zero keeps the template's interval. */
	void SetCinematicJumpInterval(float IntervalSeconds);
	/** Override squads only on a cinematic ability copy before its passes; empty retains the template. */
	void SetCinematicSquadSubtypes(const TArray<ESquadSubtype>& SquadSubtypes);
	TArray<TWeakObjectPtr<ASquadController>> GetActiveDropSquads() const;
	bool GetHasActiveDrop() const;
	void CancelActiveDrops();

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FParatroopersGlobalAbilitySettings M_ParatroopersSettings;

	enum class EParatrooperJumpKind : uint8
	{
		// An infantry soldier: the real unit descends on his parachute.
		Soldier,
		// A team weapon crewman: the real unit waits hidden at his post, a look-alike proxy descends.
		CrewProxy,
		// A team weapon: the real gun descends on its cargo chute to where it was spawned.
		TeamWeaponCargo
	};

	struct FParatrooperWaitingToJump
	{
		TWeakObjectPtr<ASquadUnit> SquadUnit;
		TWeakObjectPtr<ASquadController> Squad;
		EParatrooperJumpKind Kind = EParatrooperJumpKind::Soldier;
		TWeakObjectPtr<ATeamWeapon> TeamWeapon;
		FTransform LandingTransform = FTransform::Identity;
	};

	struct FParatrooperSquadInDrop
	{
		TWeakObjectPtr<ASquadController> Squad;
		// Soldiers (and the gun of a team weapon squad) queued or still descending; the squad is released when this
		// reaches zero.
		int32 UnitsNotLanded = 0;
		bool bM_HasQueuedUnits = false;
		bool bM_IsTeamWeaponSquad = false;
	};

	// One aircraft pass; several can overlap when missions execute the ability repeatedly.
	struct FParatrooperDropRun
	{
		int32 RunId = INDEX_NONE;
		FVector TargetLocation = FVector::ZeroVector;
		FVector PassOverLocation = FVector::ZeroVector;
		FVector RunDirection = FVector::ForwardVector;
		TOptional<FVector> AircraftStartOverride;
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		bool bM_HasReachedTarget = false;
		int32 PendingSquadSpawns = 0;
		int32 JumpedUnitCount = 0;
		int32 QueuedUnitCount = 0;
		TArray<FParatrooperWaitingToJump> UnitsWaitingToJump;
		TArray<FParatrooperSquadInDrop> SquadsInDrop;
		// Crew look-alikes in the air; removed when they land or the drop is cancelled.
		TArray<TWeakObjectPtr<AParachuteJumperProxy>> JumperProxies;
		FTimerHandle JumpTimerHandle;
	};

	TArray<FParatrooperDropRun> M_DropRuns;

	UPROPERTY(Transient)
	int32 M_NextRunId = 0;

	// Cinematic callers can distribute a squad along its pass without changing the shared designer template.
	float M_CinematicLandingTrailLength = 0.f;

	// Keeps async aircraft class loads alive until their callbacks can spawn the aircraft.
	TArray<TSharedPtr<FStreamableHandle>> M_AircraftClassLoadHandles;

	FParatrooperDropRun* FindRun(int32 RunId);
	/**
	 * @brief Keeps ordinary mission spawns and explicitly positioned cinematic passes on the same drop lifecycle.
	 * @param TargetLocation Ground centre shared by the squad landing spirals.
	 * @param AircraftStartOverride Optional cinematic approach; unset uses the owning manager's mission edge.
	 */
	void StartDrop(const FVector& TargetLocation, const TOptional<FVector>& AircraftStartOverride);

	void OnAircraftClassLoaded(int32 RunId);
	void QueueStraightRunForNextFrame(AAircraftMaster* SpawnedAircraft, int32 RunId, const FVector& EndLocation);
	FVector BuildRunEndLocation(const FVector& StartLocation, const FVector& PassOverLocation) const;

	void OnAircraftOverTarget(int32 RunId);
	void SpawnSquadForRun(int32 RunId, ESquadSubtype SquadSubtype, int32 SquadIndex);
	FVector GetProjectedSquadSpawnLocation(const FVector& TargetLocation, int32 SquadIndex, int32 SquadCount) const;
	void OnSquadSpawned(int32 RunId, AActor* SpawnedActor);
	void OnSquadReadyToJump(int32 RunId, ASquadController* Squad);

	/**
	 * @brief Team weapon squads: the controller spawns its gun right after the squad reports fully loaded, so the
	 * crew and gun are queued a tick later. Queues the gun first (cargo chute), then a proxy jump per crewman.
	 */
	void QueueTeamWeaponSquad(int32 RunId, TWeakObjectPtr<ATeamWeaponController> WeakSquad);
	// The gun is spawned on the ground at the hidden squad; it must not be seen there until it lands.
	void OnActorSpawnedDuringDrop(AActor* SpawnedActor);
	void UpdateActorSpawnedHandler();
	bool GetHasTeamWeaponSquadWithoutGun() const;
	static void HideTeamWeaponForDrop(ATeamWeapon* TeamWeapon);
	void JumpSoldier(FParatrooperDropRun& Run, const FParatrooperWaitingToJump& WaitingUnit);
	void JumpTeamWeaponCargo(FParatrooperDropRun& Run, const FParatrooperWaitingToJump& WaitingUnit);
	void JumpCrewProxy(FParatrooperDropRun& Run, const FParatrooperWaitingToJump& WaitingUnit);
	void OnTeamWeaponCargoLanded(int32 RunId, TWeakObjectPtr<ASquadController> Squad, AActor* Load, bool bLanded);
	void OnCrewProxyLanded(int32 RunId, TWeakObjectPtr<ASquadController> Squad, TWeakObjectPtr<ASquadUnit> SquadUnit,
	                       AActor* Proxy, bool bLanded);
	/**
	 * @brief Shows the hidden crewman at his post once his proxy has landed; he was hidden by BeginParachuteDrop.
	 * @param bIsOperator Operators keep their rifle put away (the controller's crew role restriction).
	 */
	static void RevealCrewUnit(ASquadUnit* SquadUnit, bool bIsOperator);
	void DestroyJumperProxies(FParatrooperDropRun& Run) const;

	FDelegateHandle M_ActorSpawnedHandle;

	void OnJumpTimer(int32 RunId);
	void JumpNextUnit(FParatrooperDropRun& Run);
	/**
	 * @brief The aircraft is held at the end of its run until every soldier of the run has jumped from it; a short
	 * approach (cinematics) would otherwise remove it while squads are still loading or queued.
	 */
	void ReleaseAircraftHoldIfAllJumped(const FParatrooperDropRun& Run) const;
	bool GetHasEverySoldierJumped(const FParatrooperDropRun& Run) const;
	/**
	 * @brief The soldier always exits the aircraft (just behind and below it) and then drifts to his landing point.
	 * Only when the aircraft is already gone does he start above the target instead.
	 * @return Actor location for the start of the descent.
	 */
	FVector GetJumpStartLocation(const FParatrooperDropRun& Run) const;
	/**
	 * @brief Spreads the soldiers of a run on a spiral around the target, projected on the navmesh, then traces down
	 * to the landscape so they land on the ground below the canopy.
	 * @return Actor location (capsule centre) on the ground.
	 */
	FVector GetLandingLocation(const FParatrooperDropRun& Run, const ASquadUnit* SquadUnit, int32 JumpIndex) const;
	FVector GetLandingTrailOrigin(const FParatrooperDropRun& Run, int32 JumpIndex) const;
	void OnParatrooperDescentFinished(int32 RunId, TWeakObjectPtr<ASquadController> Squad);
	void MarkUnitDone(FParatrooperDropRun& Run, const TWeakObjectPtr<ASquadController>& Squad);
	void ReleaseFinishedSquads(FParatrooperDropRun& Run) const;
	void ReleaseSquad(ASquadController* Squad) const;
	bool GetIsRunFinished(const FParatrooperDropRun& Run) const;
	void FinishRunIfDone(int32 RunId);

	UFUNCTION()
	void OnSpawnedAircraftDestroyed(AActor* DestroyedActor);
	void BroadcastParatroopersFinishedIfNeeded();
};
