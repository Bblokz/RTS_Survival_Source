#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/VehicleFireFeedbackComponent/VehicleFireFeedbackComponent.h"
#include "AnimMeshFeedbackComponent.generated.h"

class ACPPTurretsMaster;
class USkeletalMeshComponent;

/**
 * @brief Allows turrets to drive recoil feedback on a single skeletal mesh using the same weapon-tracking flow.
 * Use this when no separate hull static mesh exists, while keeping turret setup identical to vehicle feedback.
 * @note InitializeAnimMeshFeedbackComponent: call in blueprint with animated skeletal mesh + turret master.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAnimMeshFeedbackComponent : public UVehicleFireFeedbackComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="VehicleFireFeedback")
	void InitializeAnimMeshFeedbackComponent(
		USkeletalMeshComponent* InAnimatedMesh,
		ACPPTurretsMaster* InTurretMaster);

protected:
	virtual bool GetIsValidHullMesh() const override;
	virtual void CacheHullBaseTransform() override;
	virtual void ApplyFeedbackKick(float NormalisedMuzzleEnergy, float TurretWorldYawDegrees) override;
	virtual void ApplyHullFeedbackTransform() const override;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_AnimatedMesh = nullptr;

	bool GetIsValidAnimatedMesh() const;
};
