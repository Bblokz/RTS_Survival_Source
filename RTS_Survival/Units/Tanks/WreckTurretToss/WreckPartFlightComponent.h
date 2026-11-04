#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"

#include "WreckPartFlightComponent.generated.h"

class UStaticMeshComponent;
class USoundCue;

/** Landing tuning for one thrown wreck part; filled from the death settings of the tank that died. */
USTRUCT()
struct RTS_SURVIVAL_API FWreckPartFlightSettings
{
	GENERATED_BODY()

	bool bPlayLandingImpact = true;

	ERTS_ExplosionType LandingExplosionType = ERTS_ExplosionType::Small;

	// Plays instead of the landing explosion type's own sound; empty keeps that sound.
	UPROPERTY()
	TObjectPtr<USoundCue> LandingSound = nullptr;

	// Below these speeds (cm/s, deg/s) for SettleDuration seconds the part counts as landed.
	float SettleLinearSpeed = 25.f;
	float SettleAngularSpeed = 20.f;
	float SettleDuration = 0.4f;

	// Physics is always switched off after this many seconds, even when the part still rolls.
	float MaxFlightTime = 9.f;
};

/** Collision and mobility of a wreck part before its flight; restored when it lands. */
USTRUCT()
struct RTS_SURVIVAL_API FWreckPartRestoreState
{
	GENERATED_BODY()

	FCollisionResponseContainer CollisionResponses;
	ECollisionChannel ObjectType = ECC_Destructible;
	ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	EComponentMobility::Type Mobility = EComponentMobility::Movable;
};

/**
 * @brief Added at runtime by a wreck death component to throw one static mesh part of the wreck with physics
 * (the turret on an ammo cook-off, the engine deck on a fuel explosion). Call SetupPart before anything moves the
 * part, then Launch; once the part lies still it turns back into an ordinary static wreck part.
 */
UCLASS(ClassGroup=(RTS))
class RTS_SURVIVAL_API UWreckPartFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWreckPartFlightComponent();

	/**
	 * @brief Adds a flight component to the wreck; one per thrown part.
	 * @param Wreck Spawned wreck that owns the part.
	 * @return The registered component, or null when the wreck is invalid.
	 */
	static UWreckPartFlightComponent* AddToWreck(AActor* Wreck);

	/** Only a mesh with simple collision can simulate; empty or collision-less parts stay where they are. */
	static bool GetCanSimulatePart(UStaticMeshComponent& PartMesh);

	/**
	 * @brief Explosion VFX of the given type with either its own sound or an override.
	 * @param WorldContext Any object in the wreck's world.
	 * @param ExplosionType Explosion pool to pick the VFX (and default sound) from.
	 * @param Location World location of the explosion.
	 * @param OverrideSound Plays instead of the type's sound when set.
	 */
	static void SpawnWreckExplosion(const UObject* WorldContext, ERTS_ExplosionType ExplosionType,
	                                const FVector& Location, USoundCue* OverrideSound);

	/**
	 * @brief Remembers the part and its wreck collision so it can be restored on landing.
	 * Call before the part is moved: placing it may change its mobility.
	 * @param PartMesh Wreck static mesh part to throw.
	 * @param Settings Landing tuning.
	 */
	void SetupPart(UStaticMeshComponent* PartMesh, const FWreckPartFlightSettings& Settings);

	/**
	 * @brief Detaches the part, turns physics on and throws it; the velocities are applied next tick because a
	 * velocity set in the same frame as the simulate switch is lost.
	 * @param LaunchVelocity Linear velocity in cm/s, mass independent.
	 * @param AngularVelocityDegrees Tumble in degrees per second.
	 */
	void Launch(const FVector& LaunchVelocity, const FVector& AngularVelocityDegrees);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StorePartState();
	void PrepareForFlight();
	void ApplyLaunchVelocity(const FVector& LaunchVelocity, const FVector& AngularVelocityDegrees) const;
	void StartLandingChecks();
	void CheckIfLanded();
	void Settle();

	UFUNCTION()
	void OnPartHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	               FVector NormalImpulse, const FHitResult& Hit);

	bool GetIsValidPartMesh() const;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_PartMesh;

	UPROPERTY()
	FWreckPartFlightSettings M_Settings;

	FWreckPartRestoreState M_RestoreState;

	// World location of the part at launch; a part that settles far below it fell through the world.
	FVector M_LaunchLocation = FVector::ZeroVector;

	FTimerHandle M_LandingCheckTimerHandle;
	float M_LaunchWorldTime = 0.f;
	// Accumulates time spent below the settle speeds; reset whenever the part moves again.
	float M_TimeBelowSettleSpeed = 0.f;

	bool bM_IsFlying = false;
	bool bM_HasPlayedLandingImpact = false;
};
