// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SurvivalPOIProp.generated.h"

class UNavModifierComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * @brief A plain static mesh of a point of interest (plane wrecks, debris, thrown turrets) for meshes that have no
 * Blueprint. The survival POI subsystem spawns it deferred and calls InitProp before finishing the spawn.
 * @note Spawned at runtime only.
 */
UCLASS(NotPlaceable)
class RTS_SURVIVAL_API ASurvivalPOIProp : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalPOIProp();

	/**
	 * @brief Sets the mesh and whether it blocks units; call before FinishSpawning.
	 * @param bBlocksNavigation True carves the navmesh under the mesh (the map's navmesh only changes through
	 * nav modifiers) and keeps collision; false makes it scenery without collision.
	 */
	void InitProp(UStaticMesh* Mesh, const bool bBlocksNavigation);

	UStaticMeshComponent* GetMeshComponent() const { return M_Mesh; }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_Mesh = nullptr;

	UPROPERTY()
	TObjectPtr<UNavModifierComponent> M_NavModifier = nullptr;
};
