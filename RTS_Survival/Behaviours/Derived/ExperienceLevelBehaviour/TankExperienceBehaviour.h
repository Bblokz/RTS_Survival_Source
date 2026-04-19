// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExperienceLevelBehaviour.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

#include "TankExperienceBehaviour.generated.h"

class ATankMaster;
class ACPPTurretsMaster;
class UTrackPathFollowingComponent;

USTRUCT()
struct FTankExperienceTurretState
{
	GENERATED_BODY()

	UPROPERTY()
	float M_BaseRotationSpeed = 0.f;

	UPROPERTY()
	float M_AppliedMultiplier = 1.f;
};

USTRUCT()
struct FTankExperienceSpeedState
{
	GENERATED_BODY()

	UPROPERTY()
	float M_BaseDesiredSpeed = 0.f;

	UPROPERTY()
	float M_AppliedMultiplier = 1.f;

	bool bM_HasCachedBaseDesiredSpeed = false;
};

/**
 * @brief Experience behaviour for tanks that adds turret-rotation and movement-speed multipliers.
 */
UCLASS()
class RTS_SURVIVAL_API UTankExperienceBehaviour : public UExperienceLevelBehaviour
{
	GENERATED_BODY()

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnStack(UBehaviour* StackedBehaviour) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Experience Multipliers")
	float TurretRotationSpeedMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Experience Multipliers")
	float TankSpeedMultiplier = 1.0f;

private:
	void CacheTankDependencies();
	void ApplyTankSpecificMultipliers();
	void ApplyTurretRotationSpeedMultiplier();
	void ApplyTankSpeedMultiplier();
	void RestoreTankSpecificMultipliers();
	void RestoreTurretRotationSpeedMultiplier();
	void RestoreTankSpeedMultiplier();
	void ClearCachedTankDependencies();

	bool TryGetTankMaster(ATankMaster*& OutTankMaster);
	bool TryGetTrackPathFollowingComponent(UTrackPathFollowingComponent*& OutTrackPathFollowingComponent);
	bool GetIsValidTankMaster() const;
	bool GetIsValidTrackPathFollowingComponent() const;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TWeakObjectPtr<UTrackPathFollowingComponent> M_TrackPathFollowingComponent;

	UPROPERTY()
	TMap<TWeakObjectPtr<ACPPTurretsMaster>, FTankExperienceTurretState> M_TurretStateByTurret;

	UPROPERTY()
	FTankExperienceSpeedState M_SpeedState;
};
