// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSPooledRagdoll.generated.h"

class UMaterialInterface;
class UPhysicalMaterial;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * @brief A crew body URTSRagdollSubsystem keeps parked out of sight and wakes when a vehicle dies.
 * Its collision stays on while parked: a mesh without collision has no bodies, and rebuilding them on wake costs
 * about 5 times as much as waking ready ones. The mesh is fixed for life for the same reason.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class RTS_SURVIVAL_API ARTSPooledRagdoll : public AActor
{
	GENERATED_BODY()

public:
	ARTSPooledRagdoll();

	/**
	 * @brief One-time setup right after the pool spawned this actor.
	 * @param CrewMesh The mesh this body keeps for life.
	 * @param CorpseMaterial Physical material that stops the body sliding after it lands.
	 */
	void InitializeForPool(USkeletalMesh* CrewMesh, UPhysicalMaterial* CorpseMaterial);

	/**
	 * @brief Shows the body and starts simulating; launch it a tick later, as a velocity set now is lost.
	 * @param Location Where the body appears.
	 * @param Rotation Its tumble start, so the bodies of one explosion differ.
	 * @param Material The look for this life; nullptr keeps the mesh's own material.
	 */
	void Wake(const FVector& Location, const FRotator& Rotation, UMaterialInterface* Material);

	void Launch(const FVector& Velocity, const FVector& SpinDegrees);

	/** @brief Ends the simulation and keeps the last pose, so the sinking corpse costs nothing. */
	void Freeze();

	void Sink(float DepthCm);

	/** @brief Hides the body far below the map, ready for its next life. */
	void Park();

	bool GetIsAsleep() const;

private:
	bool GetIsValidCrewMesh() const;

	UPROPERTY(VisibleAnywhere, Category="Ragdoll")
	TObjectPtr<USkeletalMeshComponent> M_CrewMesh;
};
