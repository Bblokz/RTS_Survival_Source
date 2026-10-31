// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GA_StripFlight.generated.h"

class AAircraftMaster;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FStripFlightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AAircraftMaster> AircraftClass;

	// Longest flight to the start of the strip; shorter when the map edge is closer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxApproachDistance = 25000.0f;

	// Flight distance after the strip before the aircraft disappears.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ExitDistance = 15000.0f;
};

/** @brief One aircraft flying one strip; several can overlap when missions execute the ability repeatedly. */
struct FStripFlightRun
{
	int32 RunId = INDEX_NONE;
	FVector LineStart = FVector::ZeroVector;
	FVector LineEnd = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	float StripLength = 0.0f;
	// Below zero until the aircraft reaches the start of the strip.
	float HandledDistance = -1.0f;
	TWeakObjectPtr<AAircraftMaster> Aircraft;
	// False while the aircraft class loads; afterwards a missing aircraft means the flight is over.
	bool bM_HasSpawnedAircraft = false;
};

/**
 * @brief Base of the abilities where an aircraft flies a strip the player drew and does something along it. The
 * aircraft is a real target: when it is shot down, what it had not reached yet does not happen.
 * Derived classes implement OnStripFlightProgress. Enemy AI and missions only give a point: the strip is then centred
 * on it, at the longest length the aim settings allow.
 */
UCLASS(Abstract)
class RTS_SURVIVAL_API UGA_StripFlight : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd) override;
	virtual void BeginDestroy() override;

protected:
	/**
	 * @brief Called while the aircraft flies along the strip.
	 * @param Run The flight.
	 * @param PreviousDistance Distance along the strip already handled; below zero on the first call.
	 * @param NewDistance Distance along the strip the aircraft has reached; never above the strip length.
	 */
	virtual void OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
	                                   const float NewDistance)
	{
	}

	virtual void OnStripFlightStarted(const FStripFlightRun& Run)
	{
	}

	// Also called for a flight that was shot down before it finished the strip.
	virtual void OnStripFlightEnded(const FStripFlightRun& Run)
	{
	}

	// Assets that have to be in memory before the flight starts, besides the aircraft.
	virtual void GetAssetsToLoadBeforeFlight(TArray<FSoftObjectPath>& OutAssets) const
	{
	}

	/**
	 * @param TargetLocation Point given by the enemy AI or a mission.
	 * @return Direction of the strip centred on the point; by default the direction the aircraft comes from.
	 */
	virtual FVector GetDirectionForPointExecution(const FVector& TargetLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FStripFlightSettings M_StripFlightSettings;

private:
	TArray<FStripFlightRun> M_Runs;

	UPROPERTY(Transient)
	int32 M_NextRunId = 0;

	// Keeps async loads alive until their callbacks can spawn the aircraft.
	TArray<TSharedPtr<FStreamableHandle>> M_LoadHandles;

	FTimerHandle M_ProgressTimerHandle;

	FStripFlightRun* FindRun(const int32 RunId);
	void OnAssetsLoaded(const int32 RunId);
	void SpawnAircraftForRun(const int32 RunId);
	void QueueFlightOrderForNextFrame(AAircraftMaster* Aircraft, const int32 RunId);
	void StartProgressTimer();
	void OnProgressTimer();
	void UpdateRunProgress(const int32 RunId);
	void EndRun(const int32 RunId);
	void ClearProgressTimer();
};
