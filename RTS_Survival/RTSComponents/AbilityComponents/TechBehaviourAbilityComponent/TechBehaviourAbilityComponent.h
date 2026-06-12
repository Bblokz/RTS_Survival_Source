// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/ApplyBehaviourAbilityComponent.h"
#include "TechBehaviourAbilityComponent.generated.h"

/**
 * @brief Runtime-only ability component used by technology effects after research has completed.
 *
 * Designers configure the owning technology asset; the tech effect creates this component and supplies the settings.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTechBehaviourAbilityComponent : public UApplyBehaviourAbilityComponent
{
	GENERATED_BODY()

public:
	/**
	 * @brief Applies technology-provided settings after the component is created at runtime.
	 * @param NewBehaviourAbilitySettings Settings copied from the researched technology effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Technology|Behaviour Ability")
	void SetupBehAbilityFromSettings(const FApplyBehaviourAbilitySettings& NewBehaviourAbilitySettings);

protected:
	virtual bool GetShouldSetupAbilityOnBeginPlay() const override;
};
