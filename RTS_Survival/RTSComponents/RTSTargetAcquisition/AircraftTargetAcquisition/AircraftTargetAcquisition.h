// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSTargetAcquisition.h"
#include "AircraftTargetAcquisition.generated.h"

class AAircraftMaster;

/**
 * @brief Used by aircraft so aggressive stance only searches while the aircraft can actually engage from the air.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAircraftTargetAcquisition : public URTSTargetAcquisition
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual ETargetPreference GetOwnerTargetPreference() const override;
	virtual bool CanAggroEnemies() const override;
	virtual float GetOwnerRange() const override;

private:
	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_OwningAircraft;
	[[nodiscard]] bool GetIsValidOwningAircraft() const;
	void OnAirborne_Debugging() const;
};
