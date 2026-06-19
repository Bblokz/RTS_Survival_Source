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

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FAircraftStrafingGlobalAbilitySettings M_AircraftStrafingSettings;

	// Keeps async aircraft class load requests alive until their callbacks can safely spawn the aircraft.
	TArray<TSharedPtr<FStreamableHandle>> M_AircraftClassLoadHandles;

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
};
