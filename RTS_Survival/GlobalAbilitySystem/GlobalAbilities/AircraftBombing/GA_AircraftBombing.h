// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GA_AircraftBombing.generated.h"

class AAircraftMaster;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FAircraftBombingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AAircraftMaster> AircraftClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeUntilForcedRetreat = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CarpetBombingLength = 3000.0f;
};

/**
 * @brief Template ability used by designers to spawn an aircraft and command a timed carpet-bombing run.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_AircraftBombing : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void BeginDestroy() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FAircraftBombingSettings M_AircraftBombingSettings;

	// The latest bomber; M_Runs holds every bomber of the running executions.
	UPROPERTY(Transient)
	TWeakObjectPtr<AAircraftMaster> M_SpawnedAircraft;

	// Unused since bombers keep their own timer in M_Runs; kept so the reflected layout stays the same.
	UPROPERTY(Transient)
	FTimerHandle M_ForcedRetreatTimerHandle;

	struct FBombingRun
	{
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		FTimerHandle ForcedRetreatTimerHandle;
	};

	// Executions can overlap (enemy AI, missions, short cooldowns): one entry per bomber in the air.
	TArray<FBombingRun> M_Runs;

	// Keep the async aircraft class load requests alive until their callbacks can safely spawn the aircraft.
	TArray<TSharedPtr<FStreamableHandle>> M_AircraftClassLoadHandles;
	int32 M_PendingAircraftClassLoads = 0;

	void RequestSpawnAircraftAtStartLocationAsync(
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& RetreatLocation);
	void OnAircraftClassLoaded(
		const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& RetreatLocation);
	FVector BuildCarpetEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const;
	void QueueCarpetBombingOrderForNextFrame(
		AAircraftMaster* SpawnedAircraft,
		const FVector& StartCarpetLocation,
		const FVector& CarpetEndLocation,
		const FVector& RetreatLocation);
	void IssueCarpetBombingOrder(
		AAircraftMaster* SpawnedAircraft,
		const FVector& StartCarpetLocation,
		const FVector& CarpetEndLocation,
		const FVector& RetreatLocation);
	void StartForcedRetreatTimer(AAircraftMaster* SpawnedAircraft);
	void OnForcedRetreatTimerFinished(AAircraftMaster* SpawnedAircraft);

	UFUNCTION()
	void OnSpawnedAircraftDestroyed(AActor* DestroyedActor);
	// Ends the ability once no aircraft class is loading and no bomber of any execution is left.
	void BroadcastBombingFinishedIfNeeded();
	void ClearForcedRetreatTimer();
};
