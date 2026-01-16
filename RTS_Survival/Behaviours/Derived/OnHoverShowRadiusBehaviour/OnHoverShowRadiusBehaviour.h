// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "OnHoverShowRadiusBehaviour.generated.h"

class URadiusAOEBehaviourComponent;

/**
 * @brief Relays UI hover events to a radius AOE component on the owning actor.
 */
UCLASS()
class RTS_SURVIVAL_API UOnHoverShowRadiusBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnBehaviorHover(const bool bIsHovering) override;

private:
	bool GetIsValidRadiusAOEBehaviourComponent() const;

	UPROPERTY()
	TWeakObjectPtr<URadiusAOEBehaviourComponent> M_RadiusAOEBehaviourComponent;
};
