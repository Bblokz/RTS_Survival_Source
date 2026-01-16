// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RepairAura/RadiusRepairAura.h"
#include "RadiusHealAura.generated.h"


// Pretty much the same as the repair aura, but uses healing in the description (relies on the same behaviours) and checks if the target is a squad
// which will then heal the squad's health component which delegates the healing to its members.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URadiusHealAura : public URadiusRepairAura
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URadiusHealAura();
	bool IsValidTarget(AActor* ValidActor) const override;

protected:
	virtual void SetDescriptionWithRepairAmount(FBehaviourUIData& OutUIData, const float RepairPerSecond) const override;

};
