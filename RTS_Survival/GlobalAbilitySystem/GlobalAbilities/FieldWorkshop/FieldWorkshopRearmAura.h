// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "FieldWorkshopRearmAura.generated.h"

class UBehaviourComp;
class ATankMaster;

/**
 * @brief The field workshop's rearm radius: the ammo resupply aura of the Sd.Kfz. 250/251 ammo carriers (its
 * behaviours, rising icon and ammo-belt ground shader are set in the workshop Blueprint), limited to the owner's
 * vehicles, plus a magazine top-up: every TopUpIntervalSeconds each vehicle in range gets every weapon that is
 * reloading or not fully loaded reloaded at once. The game has no ammunition stock, so a full magazine is what "rearmed" means.
 * @note Disabled until UFieldWorkshopComponent enables it after the workshop landed.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UFieldWorkshopRearmAura : public URadiusAOEBehaviourComponent
{
	GENERATED_BODY()

public:
	UFieldWorkshopRearmAura();

	/** @return How many weapons this aura has topped up so far; for tests and logs. */
	int32 GetToppedUpWeaponCount() const { return M_ToppedUpWeaponCount; }

	float GetTopUpIntervalSeconds() const { return TopUpIntervalSeconds; }
	float GetRearmRadius() const { return GetAOERadius(); }

protected:
	virtual bool IsValidTarget(AActor* ValidActor) const override;
	virtual void OnTickComponentsInRange(const TArray<UBehaviourComp*>& BehaviourComponentsInRange) override;
	// The ammo carrier's host behaviour text names its own 15 m radius and infantry; this one states the workshop's.
	virtual void SetHostBehaviourUIData(UBehaviour& Behaviour) const override;

	// Seconds between two magazine top-ups of the vehicles in range; counted in aura intervals.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field Workshop", meta=(ClampMin="0.5"))
	float TopUpIntervalSeconds = 5.f;

private:
	float M_SecondsSinceTopUp = 0.f;
	int32 M_ToppedUpWeaponCount = 0;

	void TopUpVehiclesInRange(const TArray<UBehaviourComp*>& BehaviourComponentsInRange);
	/** @return How many weapons of the vehicle were topped up. */
	int32 TopUpVehicle(const ATankMaster& Vehicle) const;
};
