// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalSovietBaseLantern.generated.h"

class UStaticMeshComponent;

/**
 * @brief A lamp post of a Soviet base, stood beside gates, junctions and storage yard entrances. The base component spawns it
 * and gives it the kit's mesh with InitLantern; it has no collision and nothing to fight.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ASurvivalSovietBaseLantern : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalSovietBaseLantern();

	/** @param Mesh The lamp post; a null mesh leaves the lantern invisible. */
	void InitLantern(UStaticMesh* Mesh);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_Mesh;
};
