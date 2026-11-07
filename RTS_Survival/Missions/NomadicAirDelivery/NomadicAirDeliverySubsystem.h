// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NomadicAirDeliveryTypes.h"
#include "NomadicAirDeliverySubsystem.generated.h"

class AAircraftMaster;
class AGliderTankDelivery;
class ANomadicVehicle;
class ATankMaster;
class UGlobalAbilitiesManager;
class UNiagaraComponent;
struct FStreamableHandle;

/** @brief Run state of one delivery between its start and the moment the truck is on the ground. */
USTRUCT()
struct FNomadicAirDeliveryRun
{
	GENERATED_BODY()

	int32 DeliveryId = INDEX_NONE;

	FNomadicAirDeliveryOrder Order;

	// The ground below Order.BuildLocation.
	FVector GroundBuildLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<UNiagaraComponent> Beacon;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> CargoPlane;

	UPROPERTY()
	TWeakObjectPtr<AGliderTankDelivery> Glider;

	// The slung truck; watched so a truck lost in flight ends the run (the plane then never releases it).
	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> SlungNomadic;

	// Keeps the class and beacon loads alive until their callback started the run.
	TSharedPtr<FStreamableHandle> LoadHandle;

	FOnNomadicAirDeliveryLanded OnLanded;
};

/**
 * @brief Brings a player's nomadic truck in by air and orders it to build on the spot, ignoring the HQ build
 * radius: a cargo plane with the truck on ropes, or the Mammut glider for the HQ. A blue smoke beacon marks the
 * spot until the truck is down. Missions call it through UMissionBase::DeliverPlayerNomadicByCargoPlane and
 * UMissionBase::DeliverPlayerHQByMammutGlider; the -NomadicAirDeliveryValidate test drives it directly.
 */
UCLASS()
class RTS_SURVIVAL_API UNomadicAirDeliverySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/**
	 * @brief Starts one delivery; the aircraft and beacon load asynchronously first.
	 * @param Order What to bring, where to build it and how.
	 * @param OnLanded Called once: with the truck right before it is ordered to build, or with null on failure.
	 * @return The delivery id, or INDEX_NONE when the order is invalid (OnLanded is then not called).
	 */
	int32 StartDelivery(const FNomadicAirDeliveryOrder& Order, FOnNomadicAirDeliveryLanded OnLanded);

	int32 GetActiveDeliveryCount() const { return M_Runs.Num(); }

	// Every finished delivery, also those started by missions: the truck (null on failure) and its order.
	FOnAnyNomadicAirDeliveryFinished OnAnyDeliveryFinished;

	/**
	 * @brief Where a glider delivery to this build spot touches down and stops; the truck drives out from the stop.
	 * @param Order A glider order; its GliderTouchdownLocation is used when set.
	 * @param TankExitDistance How far the glider lets the truck drive out on its own.
	 * @param OutTouchdown Ground point of the touchdown.
	 * @param OutStop Ground point where the skid ends.
	 * @return False when the touchdown lies past the build spot, so there is no landing run.
	 */
	bool GetGliderLandingRun(const FNomadicAirDeliveryOrder& Order, float TankExitDistance,
	                         FVector& OutTouchdown, FVector& OutStop) const;

private:
	UPROPERTY()
	TArray<FNomadicAirDeliveryRun> M_Runs;

	int32 M_NextDeliveryId = 1;

	bool GetIsValidOrder(const FNomadicAirDeliveryOrder& Order) const;
	int32 FindRunIndex(int32 DeliveryId) const;
	int32 FindRunIndexByCargoPlane(const AAircraftMaster* CargoPlane) const;
	int32 FindRunIndexByGlider(const AGliderTankDelivery* Glider) const;
	UGlobalAbilitiesManager* GetPlayerGlobalAbilitiesManager() const;
	FVector GetGroundLocation(const FVector& Location) const;

	void OnAssetsLoaded(int32 DeliveryId);
	void SpawnBeacon(FNomadicAirDeliveryRun& Run) const;

	void StartCargoPlaneRun(int32 DeliveryId);
	void OnCargoNomadicSpawned(int32 DeliveryId, AActor* SpawnedActor);
	void OnCargoPayloadReleased(AAircraftMaster* CargoPlane, ATankMaster* ReleasedTank);

	UFUNCTION()
	void OnCargoPlaneDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnSlungNomadicDestroyed(AActor* DestroyedActor);

	void StartGliderRun(int32 DeliveryId);
	void OnGliderTankDelivered(AGliderTankDelivery* Glider, ATankMaster* Tank);

	/**
	 * @brief Ends a run: removes the beacon, tells the requester and orders the truck to build on the spot.
	 * @param RunIndex Index of the run in M_Runs; invalid after the call.
	 * @param Nomadic The delivered truck, or null when the delivery failed.
	 */
	void FinishRun(int32 RunIndex, ANomadicVehicle* Nomadic);
	void OrderBuildOnSpot(ANomadicVehicle& Nomadic, const FVector& GroundBuildLocation,
	                      const FRotator& BuildRotation) const;
};
