#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "UWeaponStateMultiHitLaser.generated.h"

/**
 * @brief Used by weapon owners to fire a laser that can damage multiple targets per pulse.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateMultiHitLaser : public UWeaponStateLaser
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the multi-hit laser so it can delay firing until the startup sound completes.
	 * @param NewOwningPlayer Player index owning the weapon.
	 * @param NewWeaponIndex Weapon array index on the owner.
	 * @param NewWeaponName Identifier for this weapon.
	 * @param NewWeaponOwner Interface to the weapon owner.
	 * @param NewMeshComponent Mesh used to anchor the fire socket.
	 * @param NewFireSocketName Socket the laser fires from.
	 * @param NewWorld World context for timers and traces.
	 * @param MultiHitLaserWeaponSettings Settings that configure the laser, including startup audio and max hits.
	 */
	void InitMultiHitLaserWeapon(
		int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		const FMultiHitLaserWeaponSettings& MultiHitLaserWeaponSettings);

protected:
	virtual void FireWeaponSystem() override;
	virtual void CreateLaunchVfx(const FVector& LaunchLocation, const FVector& ForwardVector,
	                             const bool bCreateShellCase) override;
	virtual void BeginDestroy() override;
	virtual void OnStopFire() override;
	virtual void FireTraceIteration(const int32 PulseSerial) override;

private:
	FMultiHitLaserWeaponSettings M_MultiHitLaserWeaponSettings;

	FTimerHandle M_LaserStartUpTimerHandle;
	bool bM_IsStartupSoundActive = false;

	void StartLaserAfterStartupSound();
	void ScheduleLaserAfterStartupSound();
	void PlayStartupSound(const FVector& LaunchLocation) const;
	float GetStartUpSoundDuration() const;
	bool GetIsValidLaserStartUpSound() const;
	bool GetIsValidMeshComponent() const;

	void OnMultiHitLaserAsyncTraceComplete(
		FTraceDatum& TraceDatum,
		const float ProjectileLaunchTime,
		const FVector& LaunchLocation,
		const FVector& TraceEnd,
		const int32 PulseSerial);
};
