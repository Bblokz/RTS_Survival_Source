// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "TE_LightTanksTracksImprovement.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_LightTanksTracksImprovement : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	virtual void ApplyTechnologyEffect(const UObject* WorldContextObject) override;


protected:
	virtual void OnApplyEffectToActor(AActor* ValidActor) override;
	void UpgradeGameStateOfTank(const ATankMaster* ValidTank) const;

	virtual TSet<TSubclassOf<AActor>> GetTargetActorClasses() const override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Affected Units")
	TArray<TSubclassOf<AActor>> AffectedUnits;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float MaxSpeedMlt = 1.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float TurnRateMlt = 1.25f;
};
