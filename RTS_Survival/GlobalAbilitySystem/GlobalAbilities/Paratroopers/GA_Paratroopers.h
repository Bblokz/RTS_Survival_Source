// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "GA_Paratroopers.generated.h"

class AAircraftMaster;
class ASquadController;
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

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FParatroopersGlobalAbilitySettings M_ParatroopersSettings;

	struct FParatrooperWaitingToJump
	{
		TWeakObjectPtr<ASquadUnit> SquadUnit;
		TWeakObjectPtr<ASquadController> Squad;
	};

	struct FParatrooperSquadInDrop
	{
		TWeakObjectPtr<ASquadController> Squad;
		// Soldiers queued or still descending; the squad is released when this reaches zero.
		int32 UnitsNotLanded = 0;
		bool bM_HasQueuedUnits = false;
	};

	// One aircraft pass; several can overlap when missions execute the ability repeatedly.
	struct FParatrooperDropRun
	{
		int32 RunId = INDEX_NONE;
		FVector TargetLocation = FVector::ZeroVector;
		FVector PassOverLocation = FVector::ZeroVector;
		FVector RunDirection = FVector::ForwardVector;
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		bool bM_HasReachedTarget = false;
		int32 PendingSquadSpawns = 0;
		int32 JumpedUnitCount = 0;
		TArray<FParatrooperWaitingToJump> UnitsWaitingToJump;
		TArray<FParatrooperSquadInDrop> SquadsInDrop;
		FTimerHandle JumpTimerHandle;
	};

	TArray<FParatrooperDropRun> M_DropRuns;

	UPROPERTY(Transient)
	int32 M_NextRunId = 0;

	// Keeps async aircraft class loads alive until their callbacks can spawn the aircraft.
	TArray<TSharedPtr<FStreamableHandle>> M_AircraftClassLoadHandles;

	FParatrooperDropRun* FindRun(int32 RunId);

	void OnAircraftClassLoaded(int32 RunId);
	void QueueStraightRunForNextFrame(AAircraftMaster* SpawnedAircraft, int32 RunId, const FVector& EndLocation);
	FVector BuildRunEndLocation(const FVector& StartLocation, const FVector& PassOverLocation) const;

	void OnAircraftOverTarget(int32 RunId);
	void SpawnSquadForRun(int32 RunId, ESquadSubtype SquadSubtype, int32 SquadIndex);
	FVector GetProjectedSquadSpawnLocation(const FVector& TargetLocation, int32 SquadIndex, int32 SquadCount) const;
	void OnSquadSpawned(int32 RunId, AActor* SpawnedActor);
	void OnSquadReadyToJump(int32 RunId, ASquadController* Squad);

	void OnJumpTimer(int32 RunId);
	void JumpNextUnit(FParatrooperDropRun& Run);
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
