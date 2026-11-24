// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "GA_AircraftStrafing.generated.h"

class AAircraftMaster;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FAircraftStrafingGlobalAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftClassPtr<AAircraftMaster>> AircraftClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrafeLength = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrafePointTotalLerpTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TotalStrafeTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAircraftAttackMoveSettings AttackMoveSettings;

	// On: the aircraft keep making attack runs over the target for TotalStrafeTime. Off: one run, then they hunt
	// nearby enemies on their own.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRepeatRunsOverTarget = false;

	// On: aircraft that carry bombs drop them at every pull-out (the Ju 87 strafes). Off: a guns-only strafe, whatever
	// the aircraft carries (the enemy's Pe-2 and Il-2 strafes).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDropBombsAtPullOut = true;
};

/**
 * @brief Template ability used by designers to spawn an aircraft and command a strafing run.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_AircraftStrafing : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void BeginDestroy() override;

	/**
	 * @brief Changes a COPY of the ability (mission air support), never the data asset template.
	 * @param AircraftCount Number of aircraft; all use the template's first aircraft class. 0 keeps the template's.
	 * @param TotalStrafeTime Seconds each aircraft keeps attacking. 0 keeps the template's.
	 * @param bRepeatRunsOverTarget True keeps every run over the target instead of free hunting after the first.
	 */
	void SetMissionStrafeOverrides(const int32 AircraftCount, const float TotalStrafeTime,
	                               const bool bRepeatRunsOverTarget);

	int32 GetAircraftCount() const { return M_AircraftStrafingSettings.AircraftClasses.Num(); }
	float GetTotalStrafeTime() const { return M_AircraftStrafingSettings.TotalStrafeTime; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FAircraftStrafingGlobalAbilitySettings M_AircraftStrafingSettings;

	// Keeps async aircraft class load requests alive until their callbacks can safely spawn the aircraft.
	TArray<TSharedPtr<FStreamableHandle>> M_AircraftClassLoadHandles;

	UPROPERTY(Transient)
	int32 M_PendingAircraftLoadCount = 0;

	UPROPERTY(Transient)
	int32 M_ActiveAircraftCount = 0;

	void RequestSpawnAircraftAtStartLocationsAsync(const FVector& TargetLocation);
	void RequestSpawnAircraftAtStartLocationAsync(
		const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& PostStrafeMoveToLocation);
	void OnAircraftClassLoaded(
		const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& PostStrafeMoveToLocation);
	FVector BuildStrafeEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const;
	FVector BuildPostStrafeMoveToLocation(const FVector& TargetLocation) const;
	void QueueStrafeOrderForNextFrame(
		AAircraftMaster* SpawnedAircraft,
		const FVector& StartLocation,
		const FVector& StrafeEndLocation,
		const FVector& PostStrafeMoveToLocation);
	void IssueStrafeOrder(
		AAircraftMaster* SpawnedAircraft,
		const FVector& StartLocation,
		const FVector& StrafeEndLocation,
		const FVector& PostStrafeMoveToLocation) const;

	UFUNCTION()
	void OnSpawnedAircraftDestroyed(AActor* DestroyedActor);
	void BroadcastStrafingFinishedIfNeeded();
};
