// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSTargetAcquisition.h"
#include "SquadTargetAcquisition.generated.h"

class ASquadController;

/**
 * @brief Used by squad controllers once all squad units are loaded so aggro searches use the final squad loadout.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USquadTargetAcquisition : public URTSTargetAcquisition
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual ETargetPreference GetOwnerTargetPreference() const override;
	virtual bool CanAggroEnemies() const override;
	virtual float GetOwnerRange() const override;

private:
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_OwningSquad;
	[[nodiscard]] bool GetIsValidOwningSquad() const;
	void OnCargo_Debugging() const;
};
