// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ApartmentFootprint.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Environment/DestructableEnvActor/CrushDestructionTypes/ECrushDestructionType.h"

#include "ApartmentDestructibleActor.generated.h"

class UApartmentFootprintNavModifier;
class UBoxComponent;
class UGeometryCollectionComponent;
class UNavArea;

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

/** @brief Tuning of how vehicles break an apartment by driving into it. */
USTRUCT(BlueprintType)
struct FApartmentCrushSettings
{
	GENERATED_BODY()

	// Which vehicles break panels by driving into the building; the outer ring area decides who can get there at all.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush")
	ECrushDestructionType CrushDestructionType = ECrushDestructionType::HeavyTank;

	// Damage dealt to the panels around a vehicle every crush update; at or above the panel health they give way at once.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush", meta = (ClampMin = "0.0"))
	float DamagePerUpdate = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush", meta = (ClampMin = "0.02"))
	float UpdateInterval = 0.1f;

	// Speed given to the pieces a vehicle pushes over, along its direction of travel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush", meta = (ClampMin = "0.0"))
	float PushSpeed = 350.f;
};

/**
 * @brief Place in a level and assign an apartment geometry collection (built with RTS.Apartment.BuildGeometryCollection)
 * as rest collection on the geometry component; weapon hits then break panels out where they land.
 * Vehicles that drive into it crush the panels around them, and its footprint marks the navigation mesh by itself:
 * an outer ring only heavy tanks may enter and an inner core nobody may enter.
 * The collection's pivot must be on the ground inside the footprint.
 */
UCLASS()
class RTS_SURVIVAL_API AApartmentDestructibleActor : public AActor
{
	GENERATED_BODY()

public:
	AApartmentDestructibleActor();

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	/**
	 * @brief Breaks whatever part of the building is inside the box, as a vehicle of that size driving through would.
	 * @param WorldBox The volume the vehicle occupies.
	 * @param PushDirection Direction the vehicle travels in; broken pieces are pushed along it.
	 * @return The number of building pieces found inside the box.
	 */
	int32 CrushInsideBox(const FBox& WorldBox, const FVector& PushDirection);

	/** @brief Recomputes the footprint from the rest collection; call after assigning another collection from script. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Navigation")
	void RebuildFootprint();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FApartmentDestructionSettings DestructionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FApartmentCrushSettings CrushSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FApartmentFootprintSettings FootprintSettings;

	// Area of the walls and the offset around them; the default lets only heavy tanks in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSoftClassPtr<UNavArea> OuterRingNavArea;

	// Area of everything deeper inside; the default blocks every unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> InnerCoreNavArea;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection;

	bool GetIsValidGeometryCollection() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UApartmentFootprintNavModifier> M_OuterRingNav;
	bool GetIsValidOuterRingNav() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UApartmentFootprintNavModifier> M_InnerCoreNav;
	bool GetIsValidInnerCoreNav() const;

	// Covers the footprint and its offset; only tells which vehicles are close enough to crush anything.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> M_CrushSensor;
	bool GetIsValidCrushSensor() const;

	// Vehicles inside the sensor; they are polled because a geometry collection reports no overlaps of its own.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_CrushingVehicles;

	FTimerHandle M_CrushTimerHandle;

	UFUNCTION()
	void OnCrushSensorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                               const FHitResult& SweepResult);

	void CrushUpdate();
	void CrushUnderVehicle(const AActor& Vehicle);
	bool GetPassesCrushGate(const AActor* OtherActor) const;
	void SetupCrushSensorCollision() const;

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
