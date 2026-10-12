// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ApartmentDestructibleActor.generated.h"

class UGeometryCollectionComponent;

/** @brief Tuning of how weapon hits break panels out of an apartment geometry collection. */
USTRUCT(BlueprintType)
struct FApartmentDestructionSettings
{
	GENERATED_BODY()

	// Accumulated weapon damage a wall or floor panel takes before it breaks loose.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "1.0"))
	float PanelHealth = 250.f;

	// Chaos samples strain at piece centres and a panel is 300 cm wide, so this has to reach the nearest panel centre.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "1.0"))
	float StrainRadius = 230.f;

	// How many rings of connected neighbours receive a share of the damage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0"))
	int32 StrainPropagationDepth = 1;

	// Share of the damage passed on to each next ring of neighbours.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrainPropagationFactor = 0.35f;

	// Speed given to the panel that was hit when it breaks loose, along the shot direction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float BreakingPieceSpeed = 450.f;

	// Everything below this height above the actor's origin is pinned, so the building cannot topple as a whole.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float AnchoredGroundStoreyHeight = 310.f;
};

/**
 * @brief Place in a level and assign an apartment geometry collection (built with RTS.Apartment.BuildGeometryCollection)
 * as rest collection on the geometry component; weapon hits then break panels out where they land.
 * The collection's pivot must be at the footprint centre on the ground.
 */
UCLASS()
class RTS_SURVIVAL_API AApartmentDestructibleActor : public AActor
{
	GENERATED_BODY()

public:
	AApartmentDestructibleActor();

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FApartmentDestructionSettings DestructionSettings;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection;

	bool GetIsValidGeometryCollection() const;

	// Makes weapons the only thing that can break panels; falling debris would otherwise chain-break the building.
	void BeginPlay_SetupWeaponOnlyDamage();
	void BeginPlay_AnchorGroundStorey();

	/**
	 * @brief Weapons only report where they hit, while Chaos strain needs the piece that was hit.
	 * @param ImpactLocation Where the weapon reported its hit.
	 * @param ShotDirection Direction the shot travelled in.
	 * @param OutHit The hit on the geometry collection, with the piece in Item.
	 * @return Whether a piece of the building was found along the shot.
	 */
	bool TryFindHitPiece(const FVector& ImpactLocation, const FVector& ShotDirection, FHitResult& OutHit) const;

	FVector GetImpactLocation(const FDamageEvent& DamageEvent, const AActor* DamageCauser) const;
	FVector GetShotDirection(const FDamageEvent& DamageEvent, const AActor* DamageCauser,
	                         const FVector& ImpactLocation) const;
};
