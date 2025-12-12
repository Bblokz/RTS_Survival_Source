// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "EmbeddedTurretsMaster.generated.h"

class RTS_SURVIVAL_API IEmbeddedTurretInterface;
/**
 * Child of TurretMaster: used for turrets that are embedded in a owners's mesh.
 * Uses the interface IEmbededTurretInterface to rotate the turret on that mesh.
 *
 * @note ***********************************************************************************************
 * @note Set in associated owner Blueprint (tank/building ...):
 * @note 1) The turret mesh must be set using InitEmbeddedTurret as well as the owner reference.
 * @note 2) in the same Init function you can specify whether the turret is an assault turret and if so
 * you can set the min and max yaw.
 * @note ***********************************************************************************************
 * @note Set in grandchild (derived) Blueprints:
 * @note 1) implement UpdateWeaponVectors (use the mesh of the owner)
 * @note 2) InitChildTurret in referencecasts.
 * TODO all notes.
 */
UCLASS()
class RTS_SURVIVAL_API AEmbeddedTurretsMaster : public ACPPTurretsMaster
{
	GENERATED_BODY()

public:
	AEmbeddedTurretsMaster();

	/**
	 * @brief Inits the turret mesh and the height of the turret relative to the root location of the tank mesh.
	 * @param NewEmbeddedTurretMesh Mesh of the turret.
	 * @param TurretHeight Height of the turret relative to the root location of the tank mesh.
	 * IMPORTANT: this affects the pitch calculation on the turret as it is used to calculate the turret transform.
	 * @param bIsAssaultTurret True if this turret is an assault turret and uses yaw restrictions.
	 * @param MinTurretYaw Minimum yaw of the assault turret.
	 * @param MaxTurretYaw Maximum yaw of the assault turret.
	 * @param NewEmbeddedOwner The owner of the turret.
	 * @param bIsArtillery If this embedded turret is an artillery piece.
	 * @param ArtilleryDistanceUseMaxPitch The distance at which the artillery piece should use the max pitch when aiming,
	 * below this the turret falls back to regular pitch calculations.
	 */
	UFUNCTION(BlueprintCallable, Category = "ReferenceCasts")
	void InitEmbeddedTurret(
		USkeletalMeshComponent* NewEmbeddedTurretMesh,
		const float TurretHeight,
		const bool bIsAssaultTurret,
		const float MinTurretYaw,
		const float MaxTurretYaw,
		TScriptInterface<IEmbeddedTurretInterface> NewEmbeddedOwner,
		const bool bIsArtillery = false, const float
		ArtilleryDistanceUseMaxPitch = 0.0f);

protected:
	virtual void Tick(float DeltaSeconds) override;

	virtual void BeginPlay() override;
	
	/** @brief Uses the EmbeddedTurretMesh to obtain the turret transform with yaw in local space!.
	 * @copydoc ACPPTurretsMaster::GetTurretTransform
	 */
	virtual FTransform GetTurretTransform() const override;

	/**
	 * @brief Calculates the local Rotater needed to face the target position, in a basis with respect to the
	 * skeletal mesh of which the turret part.
	 *
	 * @param TargetLocation The location to face in world space.
	 * @param TurretLocation The location of the turret in world space.
	 */
	virtual FRotator GetTargetRotation(const FVector& TargetLocation, const FVector& TurretLocation) const override;

	/** @brief Rotates the embedded turret by setting the target angle for the animation using the interface.
	 * @copydoc ACPPTurretsMaster::RotateTurret
	 */
	virtual void RotateTurret(const float DeltaTime) override;

	/** @brief Overwritten to use the interface rather than the function on blueprint child instance.
	 * @copydoc ACPPTurretsMaster::UpdateTargetPitchCPP
	 */
	virtual void UpdateTargetPitchCPP(float NewPitch) override final;

	/** @brief Set to access the mesh for updating weapon vectors for the weapon sockets.*/
	UPROPERTY(BlueprintReadOnly, Category = "Reference")
	USkeletalMeshComponent* EmbeddedTurretMesh;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Reference")
	TScriptInterface<IEmbeddedTurretInterface> EmbeddedOwner;

private:
	// The mesh of the owner on which this turret is placed.
	UPROPERTY()
	UMeshComponent* M_OwnerBaseMesh;

	// The height of the turret relative to the root location of the tank mesh.
	float M_TurretHeight;

	// True if this turret is an assault turret and uses yaw restrictions.
	bool bM_IsAssaultTurret;

	// Maximum yaw for the assault turret.
	float M_MaxYaw;
	// Minimum yaw for the assault turret.
	float M_MinYaw;

	bool bM_IsArtillery;
	float M_ArtilleryDistanceUseMaxPitch;

	/**
	 * @brief Calculate the degrees needed to turn the turret to face the target. Uses the interface to turn the turret.
	 * @param CurrentYaw The current yaw of the turret. (In local space)
	 * @param LocalTargetYaw The target yaw of the turret. (In local space)
	 * @param DeltaAngle The angle to turn the turret.
	 * @param DeltaTime The time since the last frame.
	 * @note Embedded Owner is check for validity beforehand.
	 */
	FORCEINLINE void NormalTurretRotation(
		const float& CurrentYaw,
		const float& LocalTargetYaw,
		const float& DeltaAngle,
		const float& DeltaTime);

	/**
	 * @brief Calculates the degrees needed to turn the turret and possibly the hull for the weapons to face the target.
	 * @param CurrentYaw The current yaw of the turret. (In local space)
	 * @param LocalTargetYaw The target yaw of the turret. (In local space)
	 * @param DeltaAngle The angle to turn the turret.
	 * @param DeltaTime The time since the last frame.
	 * @note Embedded Owner is check for validity beforehand.
	 */
	FORCEINLINE void AssaultTurretRotation(
		const float& CurrentYaw,
		const float& LocalTargetYaw,
		const float& DeltaAngle,
		const float& DeltaTime);

	    // Cached once; local "forward" in tank space for Idle_Base
        float M_EmbeddedBaseLocalYaw = 0.f;
    
        // Whether Idle_Base is currently driving a fixed local yaw
        bool bM_UseLocalIdleBaseTarget_Embedded = false;
    
        /** @brief Derive the embedded turret's base yaw (prefer owner angle; fallback to mesh). */
        float ComputeEmbeddedBaseLocalYaw() const;
    
        /** @brief For Idle_Base: set M_TargetRotator.Yaw to the cached local base yaw (set once). */
        void SetEmbeddedIdleBaseTarget();
    
        /** @brief Clamp to yaw limits if those are used (assault turret / constrained designs). */
        float ClampYawToLimits(const float InYaw) const;
    
        // Validators (no spammy logging; just return false if absent)
        bool GetIsValidEmbeddedOwner() const
        {
            return EmbeddedOwner.GetObject() != nullptr;
        }
        bool GetIsValidEmbeddedTurretMesh() const
        {
            return IsValid(EmbeddedTurretMesh);
        }
};
