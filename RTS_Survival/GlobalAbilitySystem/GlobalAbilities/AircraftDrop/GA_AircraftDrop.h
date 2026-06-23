// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AircraftDropTypes.h"
#include "GA_AircraftDrop.generated.h"

class AAircraftMaster;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FAircraftDropTankWithOffset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TankAttachZOffset = -250.0f;
};

USTRUCT(BlueprintType)
struct FAircraftDropGlobalAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AAircraftMaster> AircraftClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAircraftDropTankWithOffset> TanksWithOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ESquadSubtype> SquadSubtypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 AmountInfantryPerAircraft = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float HowLongStayLanded = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float HowLongStayLandedTankDrop = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float RadiusSpawnSquads = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float BetweenAircraftRectangleOffset = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> DropOffSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundAttenuation> DropOffSoundAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundConcurrency> DropOffSoundConcurrency = nullptr;
};

/**
 * @brief Designer ability for dropping tanks and infantry squads from temporary aircraft.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_AircraftDrop : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void BeginDestroy() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FAircraftDropGlobalAbilitySettings M_AircraftDropSettings;

	TSharedPtr<FStreamableHandle> M_AircraftClassLoadHandle;

	void OnAircraftClassLoaded(TSoftClassPtr<AAircraftMaster> AircraftClassToLoad, FVector TargetLocation);
	void SpawnTankDropAircraft(
		UClass* AircraftClass,
		const FVector& ExecuteLocation,
		const FAircraftDropTankWithOffset& TankWithOffset,
		int32 AircraftIndex) const;
	void SpawnSquadDropAircraft(
		UClass* AircraftClass,
		const FVector& ExecuteLocation,
		const TArray<ESquadSubtype>& Squads,
		int32 AircraftIndex) const;
	FAircraftDropRequest BuildBaseDropRequest(
		const FVector& TargetLocation,
		EAircraftDropPayloadType PayloadType,
		int32 AircraftIndex) const;
	FVector BuildAircraftSpawnLocation(const FVector& TargetLocation, int32 AircraftIndex) const;
	/**
	 * @brief Keeps multi-aircraft drops from stacking while preserving the first requested landing point.
	 * @param TargetLocation Exact designer/player execute point used by the first aircraft.
	 * @param AircraftIndex Spawn order of the aircraft in this drop request.
	 * @param UsedExecuteLocations Locations already reserved by earlier aircraft in this drop.
	 * @return TargetLocation for the first aircraft, otherwise the first projected unique formation location.
	 */
	FVector BuildAircraftExecuteLocation(
		const FVector& TargetLocation,
		int32 AircraftIndex,
		TArray<FVector>& UsedExecuteLocations) const;
	FVector BuildAircraftRectangleFormationLocation(const FVector& TargetLocation, int32 FormationIndex) const;
	bool GetIsExecuteLocationAlreadyUsed(const FVector& ExecuteLocation, const TArray<FVector>& UsedExecuteLocations) const;
};
