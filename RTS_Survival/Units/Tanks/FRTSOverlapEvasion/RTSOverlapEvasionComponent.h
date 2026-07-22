// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"

class UTrackPathFollowingComponent;
class URTSComponent;
class ATrackedTankMaster;

#include "RTSOverlapEvasionComponent.generated.h"

/** Small state for debounced removal. */
USTRUCT()
struct FPendingOverlapRemoval
{
	GENERATED_BODY()
	// Number of consecutive samples that were "clear" (no overlap & beyond clearance buffer).
	int32 ClearSamples = 0;
};

/**
 * @brief Coordinates allied vehicle overlap responses and hands persistent driving contacts to path following.
 * @note TrackOverlapMeshOfOwner: call for every vehicle footprint mesh that should participate in evasion.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSOverlapEvasionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSOverlapEvasionComponent();

	UFUNCTION(BlueprintCallable, Category="RTS|OverlapEvasion")
	void TrackOverlapMeshOfOwner(ATrackedTankMaster* InOwner, UPrimitiveComponent* const InOverlapMesh);

	/**
	 * @brief Queries all tracked overlap components for current overlaps and tries evasion
	 *        on allied actors that are standing on (or intersecting) our footprint.
	 *        Intended to be called after movement completes.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|OverlapEvasion")
	void CheckFootprintForOverlaps();

	void SetOverlapEvasionEnabled(const bool bEnabled);

	/**
	 * @brief Moves a still-idle allied vehicle only after path following classifies it as a direct obstruction.
	 * @param OtherActor Idle allied vehicle that blocks the active driving corridor.
	 * @return True only when an evasion movement command was issued successfully.
	 */
	bool TryDisplaceIdleOverlappingVehicle(AActor* OtherActor) const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<ATrackedTankMaster> M_Owner;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	void SetOwnerInterface(ATrackedTankMaster* InOwner);

	UPROPERTY()
	TArray<TWeakObjectPtr<UPrimitiveComponent>> M_TrackedOverlapMeshes;

	UPROPERTY()
	TWeakObjectPtr<UTrackPathFollowingComponent> M_OwnerTrackPathFollowingComponent;

	bool GetIsValidOwnerTrackPathFollowingComponent() const;

	void SetupTrackPathFollowingCompFromOwner();

	UPROPERTY()
	int32 M_OwnerOwningPlayer = 0;

	UFUNCTION()
	void OnChassisOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                      bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * @brief Classifies vehicle contacts while preventing post-movement footprint scans from using stale drive intent.
	 * @param OtherActor Allied actor currently overlapping the footprint.
	 * @param OtherRTS RTS data belonging to the other actor.
	 * @param ContactLocation Best available overlap contact location.
	 * @param bAllowIdleDisplacement Whether an active drive may escalate a direct idle obstruction to move-and-wait.
	 */
	void TryEvasion(
		AActor* OtherActor,
		URTSComponent* OtherRTS,
		const FVector& ContactLocation,
		bool bAllowIdleDisplacement) const;
	void TryEvasionSquadUnit(AActor* OtherActor, const FVector& ContactLocation) const;

	void SetupOwningPlayer(ATrackedTankMaster* Owner);

	bool ComputeEvasionLocation(const FVector& SelfLoc, const FVector& ContactLocation,
	                            float Radius, FVector& OutProjected, AActor* OtherActor) const;

	bool TryProjectDirection(const FVector& Start, const FVector& Dir, float Distance,
	                         FVector& OutProj, AActor* OtherActor) const;

	/** Returns true when both actors have RTSComponents with the same owning player. */
	bool HaveSameOwningPlayer(const AActor* B, URTSComponent*& OutOtherRTSComponent) const;

	/** End-overlap callback to de-register actors from the path-following overlap wait list. */
	UFUNCTION()
	void OnChassisEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Returns true if we're still touching/too close to OtherActor (keeps it registered). */
	bool HasContactOrTooCloseTo(const AActor* OtherActor) const;

	/** Start/continue a small timer to confirm separation before de-registering. */
	void SchedulePendingRemoval(AActor* OtherActor);
	void Timer_RecheckPendingOverlaps();

	// Actors pending confirmation that they are fully clear before removing from the PF list.
	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FPendingOverlapRemoval> M_PendingRemovalActors;

	// Timer driving rechecks (disabled automatically when the map is empty).
	FTimerHandle M_PendingRemovalTimerHandle;
	/**
	 * @brief Process deduplicated overlap results and run evasion on allied, idle actors.
	 * @param ActorsAndContactLocations Unique list of overlapped actors and an estimated contact location.
	 */
	void ProcessFootprintOverlaps(const TArray<TPair<TWeakObjectPtr<AActor>, FVector>>& ActorsAndContactLocations);

	// Helpers for CheckFootprintForOverlaps
	void CheckFootprintForOverlaps_GatherSamples(
		TMap<TWeakObjectPtr<AActor>, FVector>& AccumulatedContact,
		TMap<TWeakObjectPtr<AActor>, int32>& SampleCounts) const;

	void CheckFootprintForOverlaps_GatherFromMesh(
		UPrimitiveComponent* TrackedComp,
		TMap<TWeakObjectPtr<AActor>, FVector>& AccumulatedContact,
		TMap<TWeakObjectPtr<AActor>, int32>& SampleCounts) const;

	bool CheckFootprintForOverlaps_EstimateContact(
		UPrimitiveComponent* TrackedComp,
		UPrimitiveComponent* OtherComp,
		FVector& OutContactLoc) const;

        void CheckFootprintForOverlaps_BuildUniqueList(
                const TMap<TWeakObjectPtr<AActor>, FVector>& AccumulatedContact,
                const TMap<TWeakObjectPtr<AActor>, int32>& SampleCounts,
                TArray<TPair<TWeakObjectPtr<AActor>, FVector>>& OutUniqueOverlaps) const;

	/**
	 * @brief Registers only explicit movement commands so non-movement activity never enters driving avoidance.
	 * @param OtherActor Allied vehicle currently overlapping the owner's footprint.
	 * @param OtherCommands Command interface used once to classify the current command.
	 * @param OtherRTS RTS data used to cache the physical-clearance distance.
	 */
	void TryRegisterMovingOverlappingTank(
		AActor* OtherActor,
		ICommands* OtherCommands,
		const URTSComponent* OtherRTS) const;

	/**
	 * @brief Issues the existing move-away command after the caller has chosen the extreme displacement response.
	 * @param OtherActor Idle allied vehicle to move.
	 * @param OtherCommands Valid command interface belonging to the idle vehicle.
	 * @param OtherRTS RTS data providing its evasion distance.
	 * @param ContactLocation Best available overlap contact location.
	 * @return True when projection and command submission both succeeded.
	 */
	bool TryIssueIdleVehicleEvasionCommand(
		AActor* OtherActor,
		ICommands* OtherCommands,
		const URTSComponent* OtherRTS,
		const FVector& ContactLocation) const;
};
