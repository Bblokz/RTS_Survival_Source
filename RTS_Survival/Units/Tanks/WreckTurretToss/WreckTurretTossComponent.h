#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckTurretTossSettings.h"

#include "WreckTurretTossComponent.generated.h"

class UStaticMeshComponent;
class URTSFireSubsystem;

/**
 * @brief Added at runtime to a freshly spawned tank wreck when the tank died during an ammo cook-off.
 * Blows the wreck's turret mesh off the hull with physics, keeps an ammo fire burning in the open turret ring,
 * and turns physics off again once the turret lies still so the landed turret costs no simulation.
 */
UCLASS(ClassGroup=(RTS))
class RTS_SURVIVAL_API UWreckTurretTossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWreckTurretTossComponent();

	/**
	 * @brief Throws the wreck turret; call once right after the wreck spawned (its construction script placed the turret).
	 * @param Settings Launch, fire and landing tuning from the tank that died.
	 * @param LiveTurretYawOffset Yaw of the live turret relative to the tank hull at death, so the wreck turret
	 * leaves the ring facing where the gun pointed.
	 * @return False when the wreck has no usable hull or turret mesh; nothing is thrown then.
	 */
	bool StartToss(const FWreckTurretTossSettings& Settings, float LiveTurretYawOffset);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryFindWreckMeshes();
	static bool GetCanSimulateTurret(UStaticMeshComponent& TurretMesh);
	void PlaceTurretOnRing(float LiveTurretYawOffset) const;
	FVector GetTurretRingLocation() const;
	void StoreTurretCollision();
	void PrepareTurretForFlight();
	void LaunchTurret() const;
	void SpawnExplosion(ERTS_ExplosionType ExplosionType, const FVector& Location) const;
	void StartHullAmmoFire();
	void StartTurretFire();
	void StartLandingChecks();
	void CheckIfTurretLanded();
	void SettleTurret();
	void StopFire(int32& InOutFireHandle) const;

	UFUNCTION()
	void OnTurretHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                 FVector NormalImpulse, const FHitResult& Hit);

	bool GetIsValidTurretMesh() const;
	bool GetIsValidHullMesh() const;
	bool GetIsValidFireSubsystem() const;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_TurretMesh;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_HullMesh;

	UPROPERTY()
	TWeakObjectPtr<URTSFireSubsystem> M_FireSubsystem;

	FWreckTurretTossSettings M_Settings;

	// Turret collision and mobility before the toss; restored when it lands so it behaves like the normal wreck turret.
	FCollisionResponseContainer M_TurretCollisionResponses;
	ECollisionChannel M_TurretObjectType = ECC_Destructible;
	ECollisionEnabled::Type M_TurretCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	EComponentMobility::Type M_TurretMobility = EComponentMobility::Movable;

	// World location of the open turret ring at launch; the hull fire burns here and fall-through checks compare to it.
	FVector M_TurretRingLocation = FVector::ZeroVector;

	FTimerHandle M_LandingCheckTimerHandle;
	float M_LaunchWorldTime = 0.f;
	// Accumulates time spent below the settle speeds; reset whenever the turret moves again.
	float M_TimeBelowSettleSpeed = 0.f;

	int32 M_HullFireHandle = INDEX_NONE;
	int32 M_TurretFireHandle = INDEX_NONE;

	bool bM_IsFlying = false;
	bool bM_HasPlayedLandingImpact = false;
};
