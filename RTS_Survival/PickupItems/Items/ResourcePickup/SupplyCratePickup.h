// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ResourcePickup.h"
#include "SupplyCratePickup.generated.h"

class UStaticMesh;
class USphereComponent;
class UStaticMeshComponent;
class USoundBase;
class UNiagaraSystem;

/**
 * @brief A resource pickup that needs no Blueprint: code spawns it deferred, calls InitSupplyCrate (crate mesh,
 * contents, feedback) and finishes spawning; any player unit that drives or walks over it collects the contents.
 * Used for air-dropped supplies (EResourceDropPayloadKind::Pickup), e.g. the player resource safety net.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ASupplyCratePickup : public AResourcePickup
{
	GENERATED_BODY()

public:
	ASupplyCratePickup(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Call between SpawnActorDeferred and FinishSpawning, so a unit already standing on the spot collects the
	 * configured contents and not an empty crate.
	 * @param CrateMesh Mesh shown on the ground; may be null (trigger only).
	 * @param CrateMeshScale Uniform scale of the mesh.
	 * @param Resources What collecting the crate gives the player.
	 * @param PickupSound Played on collection; may be null.
	 * @param PickupEffect Spawned on collection; may be null.
	 */
	void InitSupplyCrate(UStaticMesh* CrateMesh, float CrateMeshScale, const TMap<ERTSResourceType, int32>& Resources,
	                     USoundBase* PickupSound, UNiagaraSystem* PickupEffect);

	const TMap<ERTSResourceType, int32>& GetCrateResources() const { return PickupSettings.ResourceGain.ResourceCosts; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_CrateMesh;

	// Named "TriggerCircle": AResourcePickup binds collection to the component whose name contains "Circle".
	UPROPERTY()
	TObjectPtr<USphereComponent> M_CrateTrigger;
};
