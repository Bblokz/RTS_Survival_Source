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

	UPROPERTY(Transient)
	TWeakObjectPtr<AAircraftMaster> M_SpawnedAircraft;

	UPROPERTY(Transient)
	FTimerHandle M_ForcedRetreatTimerHandle;

	// Keeps the async aircraft class load request alive until the callback can safely spawn the aircraft.
	TSharedPtr<FStreamableHandle> M_AircraftClassLoadHandle;

	void RequestSpawnAircraftAtStartLocationAsync(
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& RetreatLocation);
	void OnAircraftClassLoaded(
		const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& RetreatLocation);
	AAircraftMaster* SpawnAircraftAtStartLocation(UClass* AircraftClass, const FVector& StartLocation) const;
	FVector BuildCarpetEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const;
	void StartForcedRetreatTimer();
	void OnForcedRetreatTimerFinished();
	bool GetIsValidSpawnedAircraftForForcedRetreat() const;
	void ClearForcedRetreatTimer();
};
