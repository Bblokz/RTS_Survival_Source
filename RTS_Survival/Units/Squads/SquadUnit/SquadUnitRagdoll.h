// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "SquadUnitRagdoll.generated.h"

class UPhysicalMaterial;
class USkeletalMeshComponent;
class USquadUnitAnimInstance;
enum class ERTSDeathType : uint8;

/** What the killing hit did to the soldier; a zero push direction means the body just drops. */
struct FSquadUnitDeathHit
{
	FVector PushDirection = FVector::ZeroVector;
	// Damage as a fraction of max health, clamped to 0-1: heavy hits throw a body further.
	float DamageFraction = 0.f;
	bool bIsBlast = false;
};

/**
 * @brief Configures and applies ragdoll behaviour for a squad unit when it dies.
 * Kinetic deaths skip the montage and throw the body away from the hit; other deaths ragdoll as the montage ends.
 */
USTRUCT(BlueprintType)
struct FSquadUnitRagdoll
{
	GENERATED_BODY()

	FSquadUnitRagdoll();

	/** @return A physical material that stops a corpse where it lands; default friction lets it glide for metres. */
	static UPhysicalMaterial* CreateCorpseMaterial(UObject* Outer);

	/** @return Whether this mesh can ragdoll; meshes without a physics asset have no bodies to simulate. */
	bool GetCanRagdoll(const USkeletalMeshComponent* MeshComponent) const;

	/** @return Whether this death goes straight to ragdoll instead of playing the death montage. */
	bool GetShouldSkipDeathMontage(ERTSDeathType DeathType, const USkeletalMeshComponent* MeshComponent) const;

	/**
	 * @brief Starts simulating the body; the launch waits a tick as Chaos drops velocities set in the frame
	 * simulation starts.
	 * @param OwningActor Actor owning this ragdoll configuration, used for error reporting and timers.
	 * @param MeshComponent Skeletal mesh that ragdoll physics will be applied to.
	 * @param AnimInstance Current animation instance driving the mesh; may be nullptr.
	 * @param DeathHit The killing hit that decides the launch.
	 * @param InheritedVelocity Velocity of the living soldier, so a running body keeps moving.
	 */
	void StartRagdoll(
		AActor* OwningActor,
		USkeletalMeshComponent* MeshComponent,
		USquadUnitAnimInstance* AnimInstance,
		const FSquadUnitDeathHit& DeathHit,
		const FVector& InheritedVelocity);

	/**
	 * @brief Ends the simulation but keeps the last pose, so the corpse costs nothing while it sinks away.
	 * @param MeshComponent Skeletal mesh that is ragdolling.
	 */
	void FreezeCorpse(USkeletalMeshComponent* MeshComponent) const;

	/** Whether this unit should use ragdoll on death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	bool bEnableRagdoll;

	/** Kinetic deaths skip the death montage and ragdoll the moment the soldier is hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll",
		meta = (EditCondition = "bEnableRagdoll"))
	bool bRagdollAtOnceOnKineticDeath;

	/** Collision profile name to use while ragdolled (defaults to "RagdollCorpse" when empty). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	FName RagdollCollisionProfileName;

	/** Launch speed in cm/s of a hit dealing no real damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Launch")
	float MinHitSpeed;

	/** Launch speed in cm/s of a hit dealing the soldier's full health or more. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Launch")
	float MaxHitSpeed;

	/** Blast launch speed in cm/s at the blast's edge, where it barely damages the soldier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Launch")
	float MinBlastSpeed;

	/** Blast launch speed in cm/s when the blast deals the soldier's full health or more. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Launch")
	float MaxBlastSpeed;

	/** Upward speed of a blast as a fraction of its outward speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Launch")
	float BlastUpwardRatio;

	/** Bone that takes an extra push, so the body folds around it instead of sliding away stiffly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Launch")
	FName ImpulseBoneName;

	/** Time in seconds that ragdoll physics runs before the corpse starts to sink. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	float TimeTillDeath;

	/** Time in seconds the frozen corpse takes to sink out of sight before the unit is destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	float CorpseSinkSeconds;

	/** How far the corpse sinks in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	float CorpseSinkDepthCm;

	/** Linear damping applied to all ragdoll bodies (acts like friction in translation). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	float LinearDamping;

	/** Angular damping applied to all ragdoll bodies (acts like friction in rotation). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	float AngularDamping;

	/** Mass scale applied to all ragdoll bodies (higher = heavier / less movement). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	float MassScale;

	/** True after ragdoll has been activated for this unit. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ragdoll")
	bool bIsRagdollActive;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Effects")
	FCollapseFX DeathEffects;

private:
	void StopAnimations(USquadUnitAnimInstance* AnimInstance, USkeletalMeshComponent* MeshComponent) const;
	void SetupPhysicsAndCollision(USkeletalMeshComponent* MeshComponent) const;
	FVector GetLaunchVelocity(const FSquadUnitDeathHit& DeathHit) const;
	void ScheduleLaunch(AActor* OwningActor, USkeletalMeshComponent* MeshComponent,
	                    const FVector& LaunchVelocity, const FVector& InheritedVelocity) const;
	void StartRagdoll_CreateEffect(AActor* OwningActor);
};
