#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "VTOLDropshipAircraft.generated.h"

class UVTOLDropshipDeliveryComponent;

UENUM()
enum class EVTOLDropshipAircraftState : uint8
{
	Uninitialised,
	Descending,
	Landed,
	Ascending,
	Cached,
};

USTRUCT()
struct FVTOLDropshipAircraftPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FUnitCost M_DeliveryResources;

	UPROPERTY()
	TArray<TSoftClassPtr<AActor>> M_ActorsToSpawnWhenLanded;

	UPROPERTY()
	TArray<TSubclassOf<AActor>> M_LoadedActorsToSpawnWhenLanded;

	float M_ActorSpawnSpacing = 0.f;
};

/**
 * @brief Used by UVTOLDropshipDeliveryComponent as a cached VTOL-only delivery actor.
 * It descends onto the provided socket location, delivers payload, then ascends vertically back into cache.
 * @note InitVTOLDropshipAircraft: call after spawn before starting the first delivery run.
 */
UCLASS()
class RTS_SURVIVAL_API AVTOLDropshipAircraft : public AAircraftMaster
{
	GENERATED_BODY()

public:
	AVTOLDropshipAircraft(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Initializes the cached aircraft once so later runs can reuse it without respawning.
	 * @param StartTransform High-air transform used as the cache/ascent destination.
	 * @param LandingTransform Exact socket transform used to lock X/Y and rotation during VTOL.
	 * @param LandedDuration Seconds to stay on the pad before ascending again.
	 * @param DeliveryResources Resources delivered when the aircraft touches down.
	 * @param ActorsToSpawnWhenLanded Actor classes spawned around the pad after touchdown.
	 * @param ActorSpawnOffsetRadius Radius between spawned payload actors around the landing point.
	 * @param OwningDeliveryComponent Component that receives completion callbacks while it is alive.
	 */
	UFUNCTION(BlueprintCallable, Category="VTOL Dropship")
	void InitVTOLDropshipAircraft(
		const FTransform& StartTransform,
		const FTransform& LandingTransform,
		float LandedDuration,
		const FUnitCost& DeliveryResources,
		const TArray<TSoftClassPtr<AActor>>& ActorsToSpawnWhenLanded,
		float ActorSpawnOffsetRadius,
		UVTOLDropshipDeliveryComponent* OwningDeliveryComponent);

	/**
	 * @brief Starts one delivery from the cached high-air position to a freshly calculated socket transform.
	 * @param StartTransform High-air transform for this run.
	 * @param LandingTransform Exact socket transform for this run.
	 */
	void StartDeliveryRun(const FTransform& StartTransform, const FTransform& LandingTransform);

	/**
	 * @brief Refreshes cached payload data before reuse so Blueprint re-init changes apply to the next run.
	 * @param LandedDuration Seconds to stay on the pad before ascending again.
	 * @param DeliveryResources Resources delivered when the aircraft touches down.
	 * @param ActorsToSpawnWhenLanded Actor classes spawned around the pad after touchdown.
	 * @param ActorSpawnOffsetRadius Spacing between spawned payload actors around the landing point.
	 * @param OwningDeliveryComponent Component that receives completion callbacks while it is alive.
	 */
	void ConfigureDeliveryPayload(
		float LandedDuration,
		const FUnitCost& DeliveryResources,
		const TArray<TSoftClassPtr<AActor>>& ActorsToSpawnWhenLanded,
		float ActorSpawnOffsetRadius,
		UVTOLDropshipDeliveryComponent* OwningDeliveryComponent);

	/** @brief Called when the owning delivery component is dying during an active delivery. */
	void AbortDeliveryAndDestroyAfterAscent();

	/** @brief Destroys a hidden cached aircraft when the owner dies between runs. */
	void DestroyCachedAircraft();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Do not call AAircraftMaster::Tick from this override; that would re-enable regular aircraft pathing.
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitializeComponents() override;
	virtual void OnRTSUnitSpawned(
		bool bSetDisabled,
		float TimeNotSelectable = 0.f,
		FVector MoveTo = FVector::ZeroVector) override;

	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;
	virtual void TerminateMoveCommand() override;
	virtual void ExecuteAttackCommand(AActor* TargetActor) override;
	virtual void TerminateAttackCommand() override;
	virtual void ExecuteReturnToBase() override;
	virtual void TerminateReturnToBase() override;

private:
	EVTOLDropshipAircraftState M_DropshipState = EVTOLDropshipAircraftState::Uninitialised;

	UPROPERTY()
	FVTOLDropshipAircraftPayload M_Payload;

	UPROPERTY()
	TWeakObjectPtr<UVTOLDropshipDeliveryComponent> M_OwningDeliveryComponent;

	UPROPERTY()
	FTimerHandle M_LandedTimerHandle;

	FTransform M_StartTransform = FTransform::Identity;
	FTransform M_LandingTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VTOL Dropship", meta=(AllowPrivateAccess="true"))
	float M_VerticalMoveSpeed = 1000.f;

	float M_LandedDuration = 0.f;
	bool bM_DestroyAfterAscent = false;
	bool bM_DeliveredPayloadThisRun = false;

	void ForceNeverSelectable() const;
	void TickDescending(float DeltaSeconds);
	void TickAscending(float DeltaSeconds);
	void MoveVerticallyTowardTargetZ(float DeltaSeconds, float TargetZ);
	void OnTouchdown();
	void OnLandedDurationFinished();
	void OnAscentFinished();
	void CacheAtStartLocation();
	void DeliverResources() const;
	void CachePayloadActorClasses();
	bool TryLoadPayloadActorClass(const TSoftClassPtr<AActor>& ActorClassSoftPtr, TSubclassOf<AActor>& OutActorClass) const;
	void SpawnPayloadActors() const;
	void SpawnPayloadActor(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform) const;
	bool TryGetPayloadSpawnLocation(int32 ActorIndex, int32 ActorAmount, FVector& OutSpawnLocation) const;
	float GetPayloadRingRadius(int32 ActorAmount) const;
	float GetSafeVerticalMoveSpeed() const;
	bool TryProjectPayloadLocationToNavigation(const FVector& DesiredLocation, FVector& OutProjectedLocation) const;
	bool GetIsValidOwningDeliveryComponent() const;
};
