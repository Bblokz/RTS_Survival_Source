#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleFireFeedbackComponent.generated.h"

class ACPPTurretsMaster;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UWeaponState;

USTRUCT(BlueprintType)
struct FVehicleFireFeedbackSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_TranslationStiffness = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_TranslationDamping = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_RotationStiffness = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_RotationDamping = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_MaxBackCm = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_MaxPitchDeg = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Hull")
	float M_MaxYawJitterDeg = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|Optimisation")
	int32 M_MaxCalibreForNormalization = 160;

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
	void NotifyWeaponFired(int32 WeaponIndex, int32 WeaponCalibre);

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

	void CacheHullBaseTransform();
	void EvaluateAllCurrentTurretWeapons();
	void TryTrackWeapon(int32 WeaponIndex, UWeaponState* Weapon);
	float GetNormalisedWeaponEnergy01(int32 WeaponCalibre) const;
	void ApplyFeedbackKick(float NormalisedMuzzleEnergy);
	void ApplyCriticallyDampedSpring(
		float DeltaTime,
		float Stiffness,
		float Damping,
		FVector& InOutValue,
		FVector& InOutVelocity) const;
	void ApplyHullFeedbackTransform() const;
	void TryResetRuntimeOffsetsToRest();
};
