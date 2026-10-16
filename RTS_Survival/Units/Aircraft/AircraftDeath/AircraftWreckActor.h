// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AircraftWreckActor.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class UNiagaraSystem;

/**
 * @brief The pieces a shot-down aircraft breaks into where it hits the ground. Spawned by UAircraftDeathComponent,
 * which calls BreakUp right after; the pieces carry on with the speed of the impact, then clear themselves away.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AAircraftWreckActor : public AActor
{
	GENERATED_BODY()

public:
	AAircraftWreckActor();

	/**
	 * @brief Turns the collection into loose pieces that keep part of the aircraft's speed and are blown apart.
	 * @param CrashCollection Pieces of this aircraft, built by RTS.Aircraft.BuildCrashCollections.
	 * @param ImpactVelocity Velocity of the aircraft when it hit.
	 * @param VelocityRetained Share of that velocity the pieces keep.
	 * @param ScatterSpeed Outward speed the explosion gives the pieces.
	 * @param LifeSeconds How long the pieces stay.
	 * @return Number of pieces, or 0 when the collection could not be used.
	 */
	int32 BreakUp(UGeometryCollection* CrashCollection, const FVector& ImpactVelocity, float VelocityRetained,
	              float ScatterSpeed, float LifeSeconds);

	/** @return Pieces the wreck broke into; 0 before BreakUp or without a collection. */
	int32 GetPieceCount() const { return M_PieceCount; }

	UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return M_GeometryCollection; }

private:
	UPROPERTY()
	TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection;

	int32 M_PieceCount = 0;

	bool GetIsValidGeometryCollection() const;
	void SetupPieceCollision() const;
};
