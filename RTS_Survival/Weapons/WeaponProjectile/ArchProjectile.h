// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "ArchProjectile.generated.h"

class UWeaponStateArchProjectile;
class UProjectileMovementComponent;
class UNiagaraSystem;
class USoundCue;

/**
 * @brief An arching projectile that follows a parabolic trajectory with adjustable arch height.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AArchProjectile : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArchProjectile(const FObjectInitializer& ObjectInitializer);

	void CalculateTrajectory();
	/**
	 * @brief Initialize the arching projectile.
	 * @param NewProjectileOwner The weapon state that owns this projectile.
	 * @param Range The maximum range of the projectile.
	 * @param Damage The base damage of the projectile.
	 * @param ArmorPen The initial armor penetration value.
	 * @param ArmorPenMaxRange The armor penetration at max range.
	 * @param ShrapnelParticles Number of shrapnel particles generated upon explosion.
	 * @param ShrapnelRange The effective range of shrapnel.
	 * @param ShrapnelDamage Damage dealt by shrapnel.
	 * @param ShrapnelArmorPen Armor penetration of shrapnel.
	 * @param OwningPlayer The owning player ID.
	 * @param ImpactSurfaceData The explosion effect to play upon impact.
	 * @param BounceEffect The effect to play upon bounce.
	 * @param BounceSound The sound to play upon bounce.
	 * @param ImpactScale The scale of the impact effect.
	 * @param BounceScale The scale of the bounce effect.
	 * @param ProjectileSpeed The speed of the projectile.
	 * @param LaunchRotation The initial rotation of the projectile.
	 * @param TargetLocation The target location the projectile should hit.
	 * @param ArchStretch The factor to adjust the arch height. 1.0 is normal arch height.
	 * @param ImpactConcurrency
	 * @param ImpactAttenuation
	 */
	void InitProjectile(
		UWeaponStateArchProjectile* NewProjectileOwner,
		const float Range,
		const float Damage,
		const float ArmorPen,
		const float ArmorPenMaxRange,
		const uint32 ShrapnelParticles,
		const float ShrapnelRange,
		const float ShrapnelDamage,
		const float ShrapnelArmorPen,
		const int32 OwningPlayer,
		const TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& ImpactSurfaceData,
		UNiagaraSystem* BounceEffect,
		USoundCue* BounceSound,
		const FVector& ImpactScale,
		const FVector& BounceScale,
		const float ProjectileSpeed,
		const FRotator& LaunchRotation,
		const FVector& TargetLocation,
		const float ArchStretch, USoundConcurrency* ImpactConcurrency, USoundAttenuation* ImpactAttenuation);
	void CalculateAscent();

	/** Setup trace channel based on owning player */
	void SetupTraceChannel(int NewOwningPlayer);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	void BeginDescent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;

	/** Handle the timed explosion */
	void HandleTimedExplosion();

	/** When the projectile is close to impact, perform traces */
	void CheckForImpact();

	/** Called when the projectile hits an actor */
	void OnHitActor(AActor* HitActor, const FVector& HitLocation, const ERTSSurfaceType HitSurface);

	/** Damage the actor with shrapnel */
	void DamageActorWithShrapnel(AActor* HitActor);

private:
	// Projectile data
	UPROPERTY()
	UWeaponStateArchProjectile* M_ProjectileOwner;

	UPROPERTY()
	FVector M_ProjectileSpawn;

	UPROPERTY()
	float M_Range;

	UPROPERTY()
	float M_FullDamage;

	UPROPERTY()
	float M_ArmorPen;

	UPROPERTY()
	float M_ArmorPenAtMaxRange;

	UPROPERTY()
	FDamageEvent M_DamageEvent;

	UPROPERTY()
	TSubclassOf<UDamageType> M_DamageType;

	UPROPERTY()
	uint32 M_ShrapnelParticles;

	UPROPERTY()
	float M_ShrapnelRange;

	UPROPERTY()
	float M_ShrapnelDamage;

	UPROPERTY()
	float M_ShrapnelArmorPen;

	UPROPERTY()
	USoundAttenuation* M_ImpactAttenuation = nullptr;

	UPROPERTY()
	USoundConcurrency* M_ImpactConcurrency = nullptr;


	UPROPERTY()
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> M_SurfaceImpactData;

	UPROPERTY()
	UNiagaraSystem* M_BounceVfx;

	UPROPERTY()
	USoundCue* M_BounceSound;

	UPROPERTY()
	FVector M_ImpactScale;

	UPROPERTY()
	FVector M_BounceScale;

	/** The target location the projectile should hit */
	FVector M_TargetLocation;

	/** The stretch factor for the arch. 1.0 is normal arch, higher values make the arch higher */
	float M_ArchStretch;

	/** Total time of flight */
	float M_TotalTime;

	/** Elapsed time since launch */
	float M_ElapsedTime;

	/** Gravity acceleration */
	float M_Gravity;

	/** Projectile speed */
	float M_ProjectileSpeed;

	/** Trace channel for collision detection */
	ECollisionChannel M_TraceChannel;

	/** Projectile Movement Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	UProjectileMovementComponent* M_ProjectileMovement;

	/** Spawn explosion at location */
	void SpawnExplosion(const FVector Location, const ERTSSurfaceType SurfaceHit) const;

	/** Spawn bounce effect */
	void SpawnBounce(const FVector Location, const FRotator BounceDirection) const;

	FVector M_StartLocation;

	bool bIsAscending = false;
	float M_ApexHeight;
	float M_AscentDamping;
};
