// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "EmbeddedInfantryTurret.generated.h"

class UEmbeddedInfantryTurretAnimInstance;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// Turret tuning that infantry turrets set in their class defaults instead of a Blueprint InitChildTurret call.
USTRUCT(BlueprintType)
struct FEmbeddedInfantryTurretSettings
{
	GENERATED_BODY()

	// Degrees per second the soldier turns.
	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret", meta = (ClampMin = "1"))
	float RotationSpeed = 120.f;

	// Kept inside the infantry rifle aim offset's pitch range (-90..90).
	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret", meta = (ClampMin = "-90", ClampMax = "90"))
	float MaxPitch = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret", meta = (ClampMin = "-90", ClampMax = "90"))
	float MinPitch = -35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret")
	TEnumAsByte<EIdleRotation> IdleRotationType = EIdleRotation::Idle_Animate;
};

/**
 * @brief A soldier on the infantry master skeleton that a host tank or building expansion carries as a turret,
 * spawned at runtime by UEmbeddedTurretMountComponent. The actor turns the soldier, the rifle aim offset aims the
 * weapon and the infantry fire montages play on each shot. Never collides, affects navigation or receives decals.
 * @note Set SoldierMesh's skeletal mesh, materials and anim class (an UEmbeddedInfantryTurretAnimInstance) and
 * WeaponMesh's static mesh in the Blueprint.
 * @note EventGraph BeginPlay: call the Setup...Weapon matching the infantry weapon with WeaponMesh as mesh component.
 */
UCLASS()
class RTS_SURVIVAL_API AEmbeddedInfantryTurret : public ACPPTurretsMaster
{
	GENERATED_BODY()

public:
	AEmbeddedInfantryTurret();

	// Rides on another mount, so the host registers no vehicle module for it.
	virtual bool GetHasOwnVehicleModuleMount() const override { return false; }

	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode,
	                                 const int32 WeaponCalibre) override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** @brief Aims from the weapon in the soldier's hands, not from his feet below the hatch. */
	virtual FTransform GetTurretTransform() const override;

	virtual void UpdateTargetPitchCPP(float NewPitch) override;

	// A passenger: the host's own weapons decide whether it closes in on an attack target.
	virtual bool GetShouldReportRangeToOwner() const override { return false; }

	// Turned in yaw by the turret master; the soldier and weapon follow.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Infantry Turret")
	TObjectPtr<USceneComponent> TurretRoot;

	// Faces +Y like every infantry master mesh, hence the -90 yaw relative to the turret root.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Infantry Turret")
	TObjectPtr<USkeletalMeshComponent> SoldierMesh;

	// Only the weapon mesh: the fire and shell case sockets the weapon setup uses live on it.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Infantry Turret")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Infantry Turret")
	FEmbeddedInfantryTurretSettings InfantryTurretSettings;

private:
	void BeginPlay_DisableCollisionNavigationAndDecals() const;
	void BeginPlay_CacheInfantryAnimInstance();

	static void DisableCollisionNavigationAndDecals(UPrimitiveComponent* PrimitiveComponent);

	bool GetIsValidSoldierMesh() const;
	bool GetIsValidInfantryAnimInstance() const;

	// Owned by SoldierMesh, which recreates it when the anim class changes.
	UPROPERTY()
	TWeakObjectPtr<UEmbeddedInfantryTurretAnimInstance> M_InfantryAnimInstance;
};
