#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckPartFlightComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckTurretTossSettings.h"

#include "WreckTurretTossComponent.generated.h"

class UStaticMeshComponent;
class URTSFireSubsystem;
class USoundCue;

/**
 * @brief Added at runtime to a freshly spawned tank wreck when the tank died during an ammo cook-off.
 * Blows the wreck's turret mesh off the hull (flight and landing by a UWreckPartFlightComponent) and keeps an ammo
 * fire burning in the open turret ring.
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
	void PlaceTurretOnRing(float LiveTurretYawOffset) const;
	FVector GetTurretRingLocation() const;
	void LaunchTurret(UWreckPartFlightComponent& TurretFlight) const;
	FWreckPartFlightSettings GetTurretFlightSettings() const;
	void StartHullAmmoFire();
	void StartDebrisBurst() const;
	void StartTurretFire();
	void StopFire(int32& InOutFireHandle) const;

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

	// World location of the open turret ring at launch; the launch explosion and the hull fire start here.
	FVector M_TurretRingLocation = FVector::ZeroVector;

	int32 M_HullFireHandle = INDEX_NONE;
	int32 M_TurretFireHandle = INDEX_NONE;
};
