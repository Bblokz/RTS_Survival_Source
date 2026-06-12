// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TechBehaviourAbilityComponent.h"

void UTechBehaviourAbilityComponent::SetupBehAbilityFromSettings(
	const FApplyBehaviourAbilitySettings& NewBehaviourAbilitySettings)
{
	BehaviourAbilitySettings = NewBehaviourAbilitySettings;

	SetupBehaviourAbilityFromCurrentSettings();
}

bool UTechBehaviourAbilityComponent::GetShouldSetupAbilityOnBeginPlay() const
{
	// Technology-created components are configured after RegisterComponent, so BeginPlay must not use default settings.
	return false;
}
