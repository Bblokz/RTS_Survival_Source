// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "FieldWorkshopRearmAura.generated.h"

/**
 * @brief The field workshop's rearm radius: allied units inside it reload faster. It reuses the ammo resupply aura of
 * the Sd.Kfz. 250/251 ammo carriers (its reload behaviour, rising icon and ammo-belt ground shader are set in the
 * workshop Blueprint) and reaches every kind of allied unit: vehicles directly, infantry and team weapons through
 * their squad controller (bAffectSquadControllers). There is no magazine top-up (user decision 2026-11-05).
 * @note Disabled until UFieldWorkshopComponent enables it after the workshop landed.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UFieldWorkshopRearmAura : public URadiusAOEBehaviourComponent
{
	GENERATED_BODY()

public:
	UFieldWorkshopRearmAura();

	float GetRearmRadius() const { return GetAOERadius(); }

protected:
	virtual bool IsValidTarget(AActor* ValidActor) const override;
	// The ammo carrier's host behaviour text names its own 15 m radius and infantry; this one states the workshop's.
	virtual void SetHostBehaviourUIData(UBehaviour& Behaviour) const override;
};
