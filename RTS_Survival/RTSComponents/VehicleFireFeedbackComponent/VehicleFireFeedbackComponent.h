#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleFireFeedbackComponent.generated.h"

class ACPPTurretsMaster;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UWeaponState;
class URTSOptimizer;

USTRUCT(BlueprintType)
struct FVehicleFireFeedbackSettings
{
	GENERATED_BODY()

	/**
	 * @brief Controls how aggressively the hull translation recoil returns to rest.
	 *
	 * Higher values pull the hull back to its base relative location faster, producing a
	 * snappier "thump" after the kick. Too high can look jittery at low frame rates.
	 *
	 * Used by ApplyCriticallyDampedSpring() for M_RecoilOffsetCm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_TranslationStiffness = 120.0f;

	/**
	 * @brief Controls how much translation recoil oscillation is suppressed.
	 *
	 * Higher damping reduces bounce and overshoot, making the recoil settle more quickly.
	 * Too high can feel "dead" (no punch). Too low can visibly wobble after firing.
	 *
	 * Used by ApplyCriticallyDampedSpring() for M_RecoilOffsetCm as the velocity term.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_TranslationDamping = 18.0f;

	/**
	 * @brief Controls how aggressively the hull rotation recoil returns to rest.
	 *
	 * Higher values pull the hull back to its base relative rotation faster, producing
	 * a sharp rotational kick and quick recovery. Too high can look twitchy.
	 *
	 * Used by ApplyCriticallyDampedSpring() for M_RecoilRotDeg.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_RotationStiffness = 140.0f;

	/**
	 * @brief Controls how much rotational recoil oscillation is suppressed.
	 *
	 * Higher damping reduces rotational ringing and overshoot, making the hull stop rocking
	 * sooner. Too low can produce visible post-shot rocking; too high can remove weight.
	 *
	 * Used by ApplyCriticallyDampedSpring() for M_RecoilRotDeg as the velocity term.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_RotationDamping = 20.0f;

	/**
	 * @brief Maximum backwards kick applied to the hull per shot, in centimeters.
	 *
	 * The kick is applied along the hull's local +X axis (with a negative sign in code),
	 * scaled by the normalised muzzle energy:
	 *   M_RecoilOffsetCm.X += -M_MaxBackCm * Energy01
	 *
	 * Increase to make large-calibre guns feel heavier. Keep small to avoid clipping
	 * through attached parts or looking like the tank slides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_MaxBackCm = 8.0f;

	/**
	 * @brief Maximum pitch-up kick applied to the hull per shot, in degrees.
	 *
	 * Applied to the hull rotation spring as:
	 *   M_RecoilRotDeg.X += M_MaxPitchDeg * Energy01
	 *
	 * Sign assumes your hull +X is forward and positive pitch corresponds to a nose-up
	 * visual in your setup. If your mesh orientation differs, adjust the axis/sign in
	 * ApplyFeedbackKick() rather than forcing bad values here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_MaxPitchDeg = 1.2f;

	/**
	 * @brief Maximum random yaw jitter applied to the hull per shot, in degrees.
	 *
	 * Adds a subtle, non-deterministic "punch" so repeated shots do not look identical:
	 *   YawJitter = RandRange(-M_MaxYawJitterDeg, +M_MaxYawJitterDeg) * Energy01
	 *   M_RecoilRotDeg.Y += YawJitter
	 *
	 * Keep very small. This is a readability/feel enhancement, not a gameplay rotation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_MaxYawJitterDeg = 0.25f;

	/**
	 * @brief Upper bound calibre used to normalise weapon energy into [0..1].
	 *
	 * Normalisation currently uses:
	 *   Energy01 = Clamp(Calibre / MaxCalibreForNormalization, 0..1)
	 *
	 * This value is an optimisation / design convenience: it prevents per-weapon tuning
	 * by mapping typical calibres into a predictable range. Raising it reduces recoil
	 * for all weapons (since Energy01 becomes smaller); lowering it makes more weapons
	 * hit the 1.0 clamp (stronger recoil).
	 *
	 * Must be > 0; code clamps it to at least 1 to avoid divide-by-zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Optimisation")
	int32 M_MaxCalibreForNormalization = 160;

	/**
	 * @brief Threshold used to snap recoil state back to exact rest (zero) when almost settled.
	 *
	 * When all spring values and velocities have SizeSquared() <= this epsilon, the component
	 * hard-resets offsets and velocities to zero to avoid tiny perpetual drift and to reduce
	 * unnecessary updates.
	 *
	 * NOTE: This is compared to SizeSquared(), so it is effectively a "squared magnitude"
	 * threshold. Keep it small (e.g., 0.0001 - 0.01) depending on how quickly you want
	 * micro-motions to stop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Optimisation")
	float M_SetToRestEpsilon = 0.01f;
};

/**
 * @brief Handles tank hull fire feedback and only reacts to the selected largest-calibre turret weapon.
 * Setup from blueprint once with meshes + turret, then the turret notifies this component when weapons are added/fired.
 * @note InitializeFeedbackComponent: call in blueprint with track mesh, hull mesh and turret after components are ready.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UVehicleFireFeedbackComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleFireFeedbackComponent();

	UFUNCTION(BlueprintCallable, Category="VehicleFireFeedback")
	void InitializeFeedbackComponent(
		USkeletalMeshComponent* InTrackRootMesh,
		UStaticMeshComponent* InHullMesh,
		ACPPTurretsMaster* InTurretMaster);

	void OnTurretWeaponAdded(int32 WeaponIndex, UWeaponState* Weapon);
	void NotifyWeaponFired(int32 WeaponIndex, int32 WeaponCalibre, float TurretWorldYawDegrees);
	void SetOptimizationComponent(URTSOptimizer* InOptimizer);
	void ForceResetRecoilAndSleep();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback")
	FVehicleFireFeedbackSettings M_FeedbackSettings;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_TrackRootMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_HullMesh = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> M_TurretMaster;

	UPROPERTY()
	TWeakObjectPtr<UWeaponState> M_TrackedWeapon;

	UPROPERTY()
	TWeakObjectPtr<URTSOptimizer> M_OptimizationComponent;

	FVector M_BaseHullRelativeLocation = FVector::ZeroVector;
	FRotator M_BaseHullRelativeRotation = FRotator::ZeroRotator;

	FVector M_RecoilOffsetCm = FVector::ZeroVector;
	FVector M_RecoilVelocity = FVector::ZeroVector;
	FVector M_RecoilRotDeg = FVector::ZeroVector;
	FVector M_RecoilRotVelocity = FVector::ZeroVector;

	int32 M_TrackedWeaponIndex = INDEX_NONE;
	int32 M_TrackedWeaponCalibre = 0;
	float M_CachedTrackedEnergy01 = 0.0f;

	bool GetIsValidHullMesh() const;
	bool GetIsValidTrackRootMesh() const;
	bool GetIsValidTurretMaster() const;
	bool GetHasValidTrackedWeapon() const;
	bool GetIsValidOptimizationComponent() const;

	void CacheHullBaseTransform();
	void EvaluateAllCurrentTurretWeapons();
	void TryTrackWeapon(int32 WeaponIndex, UWeaponState* Weapon);
	float GetNormalisedWeaponEnergy01(int32 WeaponCalibre) const;
	void ApplyFeedbackKick(float NormalisedMuzzleEnergy, float TurretWorldYawDegrees);
	void ApplyCriticallyDampedSpring(
		float DeltaTime,
		float Stiffness,
		float Damping,
		FVector& InOutValue,
		FVector& InOutVelocity) const;
	void ApplyHullFeedbackTransform() const;
	void SetTickEnabledForActiveRecoil();
	void TryResetRuntimeOffsetsToRestAndSleep();
};
