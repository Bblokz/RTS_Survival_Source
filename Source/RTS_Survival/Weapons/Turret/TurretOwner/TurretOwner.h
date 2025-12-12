#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "TurretOwner.generated.h"

class UHullWeaponComponent;
class RTS_SURVIVAL_API ACPPTurretsMaster;
class RTS_SURVIVAL_API ACPPTurretsMaster;
class RTS_SURVIVAL_API ITurretOwner;


UINTERFACE(MinimalAPI, NotBlueprintable)
class UTurretOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Interface that turrets use to communicate state to their owners.
 */
class RTS_SURVIVAL_API ITurretOwner
{
	GENERATED_BODY()
	// Uses friends to not add all the functions to the implementor's public interface.
	friend class ACPPTurretsMaster;
	friend class UHullWeaponComponent;

protected:
	/**
	 * @return the player of the turret owner.
	 */
	virtual int GetOwningPlayer() = 0;

	/**
	 * @brief Notifies the owner that the current target is out of range of the turret.
	 * @param TargetLocation The location of the target that is out of range.
	 * @param CallingTurret The turret that is out of range.
	 */
	virtual void OnTurretOutOfRange(
		const FVector TargetLocation,
		ACPPTurretsMaster* CallingTurret) = 0;

	/**
	 * @brief Notifies the owner that the current target is in range of the turret.
	 * @param CallingTurret The turret that is in range.
	 */
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) = 0;

	/**
	 * @param CallingTurret The turret that destroyed its target.
	 * @param CallingHullWeapon The Hull Weapon that destroyed the target.
	 * @param DestroyedActor The actor that was destroyed. may be null.
	 * @param bWasDestroyedByOwnWeapons
	 */
	virtual void OnMountedWeaponTargetDestroyed(
		ACPPTurretsMaster* CallingTurret,
		UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor, const bool bWasDestroyedByOwnWeapons) = 0;


	virtual void OnFireWeapon(ACPPTurretsMaster* CallingTurret) = 0;

	virtual void OnProjectileHit(const bool bBounced) = 0;

	/** @return The world rotation of the owner. */
	virtual FRotator GetOwnerRotation() const = 0;

	
};
