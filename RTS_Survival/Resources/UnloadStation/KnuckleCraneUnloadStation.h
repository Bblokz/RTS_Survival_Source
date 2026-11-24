// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ResourceUnloadStation.h"

#include "KnuckleCraneUnloadStation.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** The moving parts of one knuckle boom; all hang from the turret, which slews on the portal deck. */
struct FKnuckleCraneArm
{
	TWeakObjectPtr<UStaticMeshComponent> Turret;
	TWeakObjectPtr<UStaticMeshComponent> Boom1;
	TWeakObjectPtr<UStaticMeshComponent> Boom2;
	TWeakObjectPtr<UStaticMeshComponent> Ram1Cylinder;
	TWeakObjectPtr<UStaticMeshComponent> Ram1Rod;
	TWeakObjectPtr<UStaticMeshComponent> Ram2Cylinder;
	TWeakObjectPtr<UStaticMeshComponent> Ram2Rod;
};

/**
 * @brief Knuckle-boom loader on a portal over the truck nose (the Strike Division refinery's unload crane).
 * The boom tip plays the part of the HQ crane's trolley: it follows the station's open "rail" (the tip path from the
 * hopper round the open side to the bed) and the rope, grab and payload hang from it as on the HQ. Each tick the
 * turret slews towards the tip and a two-link solve (knuckle up) poses both booms and their rams.
 * @note Defaults are the refinery's (single bay at the old drop-off point, overflow point beside it). The rig numbers
 * mirror _AssetSource/RefineryUnloadCrane/build_knuckle_crane.py; see Docs/Units/RefineryUnloadCrane.md.
 */
UCLASS()
class RTS_SURVIVAL_API AKnuckleCraneUnloadStation : public AResourceUnloadStation
{
	GENERATED_BODY()

public:
	AKnuckleCraneUnloadStation();

protected:
	virtual void OnTrolleyCreated(FUnloadTrolley& Trolley) override;
	virtual void UpdateTrolleyVisuals(const FUnloadTrolley& Trolley) const override;

	// Slew axis on the portal deck (station frame, cm); Z is the deck top the turret stands on.
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig")
	FVector SlewPivot = FVector(220.f, 13.f, 310.f);

	// Shoulder pin height above the turret origin (cm).
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig")
	float ShoulderHeight = 120.f;

	// Shoulder pin to knuckle pin (cm).
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig", meta=(ClampMin="10"))
	float Boom1Length = 220.f;

	// Knuckle pin to the tip sheave (cm); the tip is where the rail spline puts the "trolley".
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig", meta=(ClampMin="10"))
	float Boom2Length = 300.f;

	// Ram 1 (lift): cylinder eye on the turret (X along the boom, Y up) and rod eye on boom 1 (X along, Y normal).
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig")
	FVector2D Ram1TurretPoint = FVector2D(46.f, 42.f);

	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig")
	FVector2D Ram1Boom1Point = FVector2D(99.f, -18.f);

	// Ram 2 (knuckle): cylinder eye on top of boom 1, rod eye on the lever behind the knuckle (boom-2 frame).
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig")
	FVector2D Ram2Boom1Point = FVector2D(70.f, 20.f);

	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Rig")
	FVector2D Ram2Boom2Point = FVector2D(-40.f, 22.f);

	// Origin on the slew axis at deck top, +X = boom direction.
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> TurretMesh;

	// Origin on the shoulder pin, boom along +X.
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> Boom1Mesh;

	// Origin on the knuckle pin, boom along +X, tip sheave at Boom2Length.
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> Boom2Mesh;

	// Ram parts: origin on the eye pin, along +X. The cylinder points from its eye to the rod's eye and back.
	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> Ram1CylinderMesh;

	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> Ram1RodMesh;

	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> Ram2CylinderMesh;

	UPROPERTY(EditDefaultsOnly, Category="Knuckle Crane|Meshes")
	TObjectPtr<UStaticMesh> Ram2RodMesh;

private:
	TArray<FKnuckleCraneArm> M_Arms;

	void BuildRefineryDefaults();
	void PoseArm(const FKnuckleCraneArm& Arm, const FVector& TipWorld) const;
};
