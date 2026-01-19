// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "SquadAimAbilityComponent.generated.h"

class ASquadController;

/**
 * @brief Aim ability component used by squad controllers to apply effects to infantry weapons.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USquadAimAbilityComponent : public UAimAbilityComponent
{
	GENERATED_BODY()

public:
	USquadAimAbilityComponent();

protected:
	virtual void PostInitProperties() override;

	virtual bool CollectWeaponStates(TArray<UWeaponState*>& OutWeaponStates, float& OutMaxRange) const override;
	virtual void SetWeaponsDisabled() override;
	virtual void SetWeaponsAutoEngage(const bool bUseLastTarget) override;
	virtual void FireWeaponsAtLocation(const FVector& TargetLocation) override;
	virtual void RequestMoveToLocation(const FVector& MoveToLocation) override;
	virtual void StopMovementForAbility() override;

private:
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SquadController;

	bool GetIsValidSquadController() const;
};
