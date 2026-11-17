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

	// For a gunner whose post blocks part of the circle (the sniper loft's net poles): he only picks targets, and idles,
	// inside [MinTraverseYaw, MaxTraverseYaw] around the forward (+X) of the component he is mounted on.
	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret|Traverse Arc")
	bool bLimitTraverseArc = false;

	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret|Traverse Arc",
		meta = (EditCondition = "bLimitTraverseArc", ClampMin = "-180", ClampMax = "180"))
	float MinTraverseYaw = -90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Infantry Turret|Traverse Arc",
		meta = (EditCondition = "bLimitTraverseArc", ClampMin = "-180", ClampMax = "180"))
	float MaxTraverseYaw = 90.f;
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

	/** @return False for targets outside the traverse arc, when InfantryTurretSettings limits it. */
	virtual bool GetCanTraverseToLocation(const FVector& TargetLocation) const override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** @brief Aims from the weapon in the soldier's hands, not from his feet below the hatch. */
	virtual FTransform GetTurretTransform() const override;

	virtual void UpdateTargetPitchCPP(float NewPitch) override;

	virtual bool GetHasFixedTraverseArc() const override;

	/** @brief Idles inside the traverse arc when it is limited, so the gunner never looks into his post's blind side. */
	virtual float GetIdleAnimationYawTarget() const override;

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

	/**
	 * @brief World yaw the traverse arc is measured from: the forward of the component the turret is mounted on.
	 * @param OutMountWorldYaw The mount's world yaw in degrees.
	 * @return False when the turret is not attached to anything (no arc can be measured).
	 */
	bool GetMountWorldYaw(float& OutMountWorldYaw) const;

	bool GetIsValidSoldierMesh() const;
	bool GetIsValidInfantryAnimInstance() const;

	// Owned by SoldierMesh, which recreates it when the anim class changes.
	UPROPERTY()
	TWeakObjectPtr<UEmbeddedInfantryTurretAnimInstance> M_InfantryAnimInstance;
};
