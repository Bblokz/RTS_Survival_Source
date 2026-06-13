#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "RTS_Survival/Units/Aircraft/VTOLDropship/VTOLDropshipAircraft.h"
#include "VTOLDropshipDeliveryComponent.generated.h"

class UMeshComponent;

USTRUCT()
struct FVTOLDropshipDeliveryConfig
{
	GENERATED_BODY()

	float M_StartHeightOffset = 0.f;
	FName M_LandingSocketName = NAME_None;

	UPROPERTY()
	FUnitCost M_DeliveryResources;

	UPROPERTY()
	TArray<TSoftClassPtr<AActor>> M_ActorsToSpawnWhenLanded;

	float M_LandedDuration = 0.f;
	float M_TimeBetweenRuns = 0.f;
	float M_ActorSpawnOffsetRadius = 0.f;

	UPROPERTY()
	TSoftClassPtr<AVTOLDropshipAircraft> M_AircraftClass;

	UPROPERTY()
	TSubclassOf<AVTOLDropshipAircraft> M_LoadedAircraftClass;
};

USTRUCT()
struct FVTOLDropshipDeliveryRuntimeState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_LandingSocketMesh;

	UPROPERTY()
	TWeakObjectPtr<AVTOLDropshipAircraft> M_CachedAircraft;

	UPROPERTY()
	FTimerHandle M_NextRunTimerHandle;

	bool bM_IsEnabled = false;
	bool bM_IsDeliveryInProgress = false;
	bool bM_IsCleaningUp = false;
};

/**
 * @brief Add to a building to run recurring cached VTOL dropship deliveries from a mesh socket.
 * Blueprint initializes the socket, payload, timing, aircraft class, and whether the loop starts enabled.
 * @note InitVTOLDropshipDeliveryComponent: call in Blueprint before enabling deliveries.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UVTOLDropshipDeliveryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTOLDropshipDeliveryComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Sets all delivery data at once so the component can safely cache/reuse one aircraft.
	 * @param StartHeightOffset How far above the landing socket the cached aircraft waits between runs.
	 * @param LandingSocketMesh Mesh that owns the landing socket.
	 * @param LandingSocketName Socket used as the exact landing transform.
	 * @param DeliveryResources Resources delivered when the aircraft lands.
	 * @param ActorsToSpawnWhenLanded Actor classes spawned around the landing socket after touchdown.
	 * @param LandedDuration Seconds the aircraft stays landed before taking off again.
	 * @param TimeBetweenRuns Delay after a completed run before the next descent starts.
	 * @param AircraftClass Dropship class to cache and reuse.
	 * @param ActorSpawnOffsetRadius Spacing between payload actors around the landing point.
	 * @param bStartsEnabled Whether the first wait timer starts immediately after init.
	 */
	UFUNCTION(BlueprintCallable, Category="VTOL Dropship")
	void InitVTOLDropshipDeliveryComponent(
		float StartHeightOffset,
		UMeshComponent* LandingSocketMesh,
		FName LandingSocketName,
		const FUnitCost& DeliveryResources,
		const TArray<TSoftClassPtr<AActor>>& ActorsToSpawnWhenLanded,
		float LandedDuration,
		float TimeBetweenRuns,
		TSoftClassPtr<AVTOLDropshipAircraft> AircraftClass,
		float ActorSpawnOffsetRadius,
		bool bStartsEnabled);

	UFUNCTION(BlueprintCallable, Category="VTOL Dropship")
	void SetDropshipDeliveryEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintCallable, Category="VTOL Dropship")
	bool GetIsDropshipDeliveryEnabled() const;

	void OnDropshipAircraftRunCompleted(AVTOLDropshipAircraft* CompletedAircraft);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	FVTOLDropshipDeliveryConfig M_DeliveryConfig;

	UPROPERTY()
	FVTOLDropshipDeliveryRuntimeState M_RuntimeState;

	bool bM_HasBeenInitialised = false;

	void StartNextRunTimer();
	void ClearNextRunTimer();
	void StartDeliveryRun();
	void EnsureCachedAircraftAndStartRun(const FTransform& StartTransform, const FTransform& LandingTransform);
	AVTOLDropshipAircraft* SpawnCachedAircraft(const FTransform& StartTransform, const FTransform& LandingTransform);
	bool GetShouldReplaceCachedAircraft(const AVTOLDropshipAircraft* CachedAircraft) const;
	void DestroyCachedAircraftForReplacement(AVTOLDropshipAircraft* CachedAircraft);
	void ConfigureCachedAircraftForRun(AVTOLDropshipAircraft* CachedAircraft);
	void BindCachedAircraftDestroyedCallback(AVTOLDropshipAircraft* CachedAircraft);
	void ClearCachedAircraftReference();
	UFUNCTION()
	void OnCachedAircraftDestroyed(AActor* DestroyedActor);
	FTransform GetLandingSocketTransform() const;
	FTransform GetStartTransformFromLandingTransform(const FTransform& LandingTransform) const;
	void HandleOwnerEndPlayCleanup();
	bool GetIsValidLandingSocketMesh() const;
	bool GetIsValidCachedAircraft() const;
	bool CacheAircraftClass();
	bool GetIsValidAircraftClass() const;
	bool GetIsValidLoadedAircraftClass() const;
	bool GetIsValidConfiguration() const;
};
