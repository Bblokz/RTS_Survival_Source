// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyTypes.h"

#include "SurvivalDeploymentComponent.generated.h"

class AAircraftMaster;
class ANomadicVehicle;
class ASurvivalModeDirector;
class ATankMaster;
class UGA_Paratroopers;
struct FStreamableHandle;
struct FSurvivalDeploymentSettings;

/**
 * @brief Flies the confirmed army to the chosen start location: every vehicle hangs under its own drop aircraft,
 * infantry jump with parachutes, team weapons and walkers walk out of a landed aircraft. The HQ goes first.
 */
UCLASS(ClassGroup=(Survival))
class RTS_SURVIVAL_API USurvivalDeploymentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivalDeploymentComponent();

	/**
	 * @param Director The survival director.
	 * @param bIsReinforcement Reward drops between waves: no HQ, no director callbacks, the component removes
	 * itself when the last aircraft is gone.
	 */
	void InitDeployment(ASurvivalModeDirector* Director, bool bIsReinforcement = false);

	/**
	 * @brief Starts the drop; the director is told when the HQ landed and when every aircraft is done.
	 * @param Army The confirmed picks, HQ included.
	 * @param DeployCenter Ground location where the HQ lands.
	 * @param ApproachDirection Horizontal direction the aircraft come from (pointing from the centre outwards).
	 */
	void StartDeployment(const TArray<FSurvivalArmyPick>& Army, const FVector& DeployCenter,
	                     const FVector& ApproachDirection);

	bool GetIsDeploying() const { return bM_IsDeploying; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FVehicleDrop
	{
		FTrainingOption Unit;
		FVector LandingLocation = FVector::ZeroVector;
		bool bIsHQ = false;
	};

	struct FLandingSquadDrop
	{
		TArray<ESquadSubtype> Squads;
		FVector LandingLocation = FVector::ZeroVector;
	};

	struct FParatrooperDrop
	{
		TArray<ESquadSubtype> Squads;
		FVector LandingLocation = FVector::ZeroVector;
	};

	// A vehicle hanging under an aircraft; landed once it is detached.
	struct FCarriedVehicle
	{
		TWeakObjectPtr<ATankMaster> Vehicle;
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		bool bIsHQ = false;
		bool bHasLanded = false;
	};

	TWeakObjectPtr<ASurvivalModeDirector> M_Director;
	bool bM_IsDeploying = false;
	bool bM_IsReinforcement = false;
	bool bM_HasReportedHQLanded = false;

	FVector M_DeployCenter = FVector::ZeroVector;
	FVector M_ApproachDirection = FVector::ForwardVector;

	// Launch queue, HQ first; one entry is launched every SecondsBetweenAircraft.
	TArray<FVehicleDrop> M_VehicleDrops;
	TArray<FLandingSquadDrop> M_LandingSquadDrops;
	TArray<FParatrooperDrop> M_ParatrooperDrops;
	int32 M_NextLaunchIndex = 0;
	int32 M_LaunchedAircraftCount = 0;

	TArray<FCarriedVehicle> M_CarriedVehicles;
	TArray<TWeakObjectPtr<AAircraftMaster>> M_ActiveAircraft;

	UPROPERTY()
	TArray<TObjectPtr<UGA_Paratroopers>> M_ParatrooperAbilities;

	UPROPERTY()
	TObjectPtr<UClass> M_VehicleAircraftClass;

	TSharedPtr<FStreamableHandle> M_AircraftClassHandle;
	FTimerHandle M_LaunchTimerHandle;
	FTimerHandle M_MonitorTimerHandle;
	// Vehicles still waiting for the async spawner.
	int32 M_PendingVehicleSpawns = 0;

	void BuildDropPlan(const TArray<FSurvivalArmyPick>& Army);
	TArray<FVector> BuildVehicleLandingLocations(int32 Amount, float& OutOuterRadius) const;
	FVector ProjectToGround(const FVector& Location) const;

	void OnVehicleAircraftClassLoaded();
	void LaunchNext();
	void LaunchVehicleDrop(const FVehicleDrop& Drop);
	void LaunchLandingSquadDrop(const FLandingSquadDrop& Drop);
	void LaunchParatrooperDrop(const FParatrooperDrop& Drop);
	void OnVehicleSpawned(AActor* SpawnedActor, AAircraftMaster* Aircraft, const FVehicleDrop& Drop,
	                      const FVector& AircraftSpawnLocation);
	void PlaceVehicleWithoutAircraft(AActor* SpawnedActor, const FVehicleDrop& Drop);

	/** @return Start of the flight for the n-th aircraft: out along the approach direction, spread sideways. */
	FVector GetAircraftSpawnLocation(int32 AircraftIndex) const;
	FVector GetAircraftRetreatLocation(int32 AircraftIndex) const;
	int32 GetTotalLaunchCount() const;

	void MonitorDeployment();
	void ReportHQLanded(ATankMaster* HQ);
	bool GetHasEverythingFinished() const;
	void FinishDeployment();

	bool GetIsValidDirector() const;
	const FSurvivalDeploymentSettings* GetDeploymentSettings() const;
};
