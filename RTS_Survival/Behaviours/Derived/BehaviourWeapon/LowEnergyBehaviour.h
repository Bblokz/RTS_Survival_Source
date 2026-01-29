// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "LowEnergyBehaviour.generated.h"

class ACPPTurretsMaster;
class ABuildingExpansion;
class ATankMaster;

/**
 * @brief Behaviour applied during low energy that slows turret rotation while still modifying weapons.
 */
UCLASS()
class RTS_SURVIVAL_API ULowEnergyBehaviour : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	ULowEnergyBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|LowEnergy")
	float TurretLowEnergyRotationSpeedMlt = 1.0f;

private:
	void ApplyLowEnergyTurretRotation();
	void RestoreTurretRotation();
	bool TryGetOwnerTurrets(TArray<ACPPTurretsMaster*>& OutTurrets) const;

	UPROPERTY()
	TMap<TWeakObjectPtr<ACPPTurretsMaster>, float> M_CachedTurretRotationSpeeds;
};
