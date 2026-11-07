// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CargoRopeSlingComponent.generated.h"

class ATankMaster;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace CargoRopeSling
{
	// Seconds the ropes take to winch back up to the belly hook after the payload is released.
	constexpr float RetractSeconds = 1.5f;
}

/**
 * @brief Runtime rope sling an aircraft creates when it carries a tank payload on ropes under its belly.
 * The aircraft calls StartSling after attaching the payload, SetSwayEnabled(false) on the final descent,
 * SnapPayloadToRest at touchdown and ReleasePayload before detaching; the ropes then winch up and the component
 * destroys itself.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UCargoRopeSlingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCargoRopeSlingComponent();

	/**
	 * @brief Builds the ropes from one hook under the aircraft's belly through a spreader ring to the four top corners
	 * of the payload's visible mesh, and starts the gentle pendulum sway.
	 * @param AircraftMesh The aircraft's visible body; its lowest point is the belly hook. Null uses a default depth.
	 * @param Payload The tank already attached to the owner at (0, 0, -HangDistance).
	 * @param HangDistance Distance from the owner's origin down to the payload's origin.
	 * @return False when no ropes could be built; the caller should destroy the component.
	 */
	bool StartSling(const UPrimitiveComponent* AircraftMesh, ATankMaster* Payload, const float HangDistance);

	/** @brief Fades the sway in or out; disabled on the final descent so the payload hangs straight. */
	void SetSwayEnabled(const bool bEnabled);

	/** @brief Removes all sway at once so the payload sits exactly under the owner where the drop ordered it. */
	void SnapPayloadToRest();

	/** @brief Lets go of the payload; the ropes winch up to the hook and the component destroys itself. */
	void ReleasePayload();

	/** @brief Removes the ropes and this component at once (owner shot down or drop aborted). */
	void DestroySlingImmediately();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	// Stretched along its Z axis between the rope ends; expected 100 cm tall with a 50 cm radius (engine cylinder).
	UPROPERTY(EditDefaultsOnly, Category="Rope Sling")
	TObjectPtr<UStaticMesh> RopeMesh;

	UPROPERTY(EditDefaultsOnly, Category="Rope Sling")
	TObjectPtr<UMaterialInterface> RopeMaterial;

	// Set on the material's "Color" vector parameter when it has one.
	UPROPERTY(EditDefaultsOnly, Category="Rope Sling")
	FLinearColor RopeColor = FLinearColor(0.22f, 0.2f, 0.17f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Rope Sling", meta=(ClampMin="0.1"))
	float MainLineRadius = 12.f;

	UPROPERTY(EditDefaultsOnly, Category="Rope Sling", meta=(ClampMin="0.1"))
	float LegRadius = 8.f;

	// Length of the four legs from the spreader ring down to the payload's roof corners.
	UPROPERTY(EditDefaultsOnly, Category="Rope Sling", meta=(ClampMin="0.0"))
	float SpreaderLegLength = 450.f;

	// Largest sideways swing of the payload in cm.
	UPROPERTY(EditDefaultsOnly, Category="Rope Sling", meta=(ClampMin="0.0"))
	float SwayAmplitude = 35.f;

	UPROPERTY(EditDefaultsOnly, Category="Rope Sling", meta=(ClampMin="0.1"))
	float SwayFadeSeconds = 2.f;

private:
	struct FCargoRopeSwayState
	{
		float Time = 0.f;
		// 0..1 blend of the sway; fades toward bM_IsEnabled.
		float Weight = 0.f;
		bool bM_IsEnabled = false;
	};

	struct FCargoRopeReleaseState
	{
		bool bM_IsReleased = false;
		float Elapsed = 0.f;
		// Rope ends relative to the hook at the moment of release; they shrink to zero while winching up.
		FVector RingOffsetFromHook = FVector::ZeroVector;
		TArray<FVector> LegEndOffsetsFromHook;
	};

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_Payload;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> M_PayloadMesh;

	// Index 0 is the main line from the hook to the spreader ring, 1-4 are the legs to the payload's corners.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_RopeSegments;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_RopeMaterialInstance;

	// Belly hook in the owner's actor space.
	FVector M_HookLocalOffset = FVector::ZeroVector;

	// Roof corners and roof centre of the payload's visible mesh, in that mesh component's space.
	TArray<FVector> M_PayloadTopCornersLocal;
	FVector M_PayloadTopCentreLocal = FVector::ZeroVector;

	float M_HangDistance = 0.f;

	FCargoRopeSwayState M_Sway;
	FCargoRopeReleaseState M_Release;

	/** @return Whether the payload mesh still exists; not an error, the payload can be destroyed while slung. */
	bool GetHasValidPayloadMesh() const;

	bool StartSling_CachePayloadCorners(const USkeletalMeshComponent* PayloadMesh);
	void StartSling_CacheHookOffset(const UPrimitiveComponent* AircraftMesh);
	bool StartSling_CreateRopeSegments();

	FVector GetHookWorldLocation() const;
	FVector GetSpreaderRingLocation(const FVector& HookLocation, const FVector& PayloadTopCentre) const;

	void TickSway(const float DeltaTime);
	void ApplyPayloadSwayOffset(const FVector& SwayOffset) const;
	void UpdateAttachedRopes(const FVector& HookLocation);
	void TickRetract(const float DeltaTime, const FVector& HookLocation);

	/**
	 * @brief Stretches one rope cylinder between two world points.
	 * @param SegmentIndex Index into M_RopeSegments.
	 * @param Start World start of the rope.
	 * @param End World end of the rope.
	 * @param Radius Rope radius in cm.
	 */
	void PlaceRopeSegment(const int32 SegmentIndex, const FVector& Start, const FVector& End, const float Radius) const;

	void DestroyRopeSegments();
};
