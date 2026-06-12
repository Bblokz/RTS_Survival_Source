// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/ApplyBehaviourAbilityComponent.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TechBehaviourAbility.generated.h"

class UTechBehaviourAbilityComponent;

/**
 * @brief Base technology asset for adding a behaviour ability to every targeted unit at runtime.
 *
 * Create Blueprint children to choose affected unit subtypes and configure the command-card behaviour ability.
 */
UCLASS(BlueprintType, Blueprintable)
class RTS_SURVIVAL_API UTechBehaviourAbility : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTechBehaviourAbility();

protected:
	virtual void OnTechAppliedToActor(AActor* Actor) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Behaviour Ability")
	FApplyBehaviourAbilitySettings BehaviourAbilitySettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Behaviour Ability")
	TSubclassOf<UTechBehaviourAbilityComponent> BehaviourAbilityComponentClass;

private:
	bool GetIsValidBehaviourAbilityComponentClass() const;
	UTechBehaviourAbilityComponent* CreateBehaviourAbilityComponent(AActor* Actor) const;
	void AddBehaviourAbilityComponentToActor(AActor* Actor) const;
};
