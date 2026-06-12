// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TechBehaviourAbility.h"

#include "RTS_Survival/RTSComponents/AbilityComponents/TechBehaviourAbilityComponent/TechBehaviourAbilityComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UTechBehaviourAbility::UTechBehaviourAbility()
{
	BehaviourAbilityComponentClass = UTechBehaviourAbilityComponent::StaticClass();
}

void UTechBehaviourAbility::OnTechAppliedToActor(AActor* Actor)
{
	AddBehaviourAbilityComponentToActor(Actor);
	Super::OnTechAppliedToActor(Actor);
}

bool UTechBehaviourAbility::GetIsValidBehaviourAbilityComponentClass() const
{
	if (IsValid(BehaviourAbilityComponentClass.Get()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"BehaviourAbilityComponentClass",
		"GetIsValidBehaviourAbilityComponentClass",
		this);
	return false;
}

UTechBehaviourAbilityComponent* UTechBehaviourAbility::CreateBehaviourAbilityComponent(AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return nullptr;
	}

	if (not GetIsValidBehaviourAbilityComponentClass())
	{
		return nullptr;
	}

	UTechBehaviourAbilityComponent* BehaviourAbilityComponent = NewObject<UTechBehaviourAbilityComponent>(
		Actor,
		BehaviourAbilityComponentClass.Get());
	if (IsValid(BehaviourAbilityComponent))
	{
		return BehaviourAbilityComponent;
	}

	RTSFunctionLibrary::ReportError("Failed to create TechBehaviourAbilityComponent for " + Actor->GetName());
	return nullptr;
}

void UTechBehaviourAbility::AddBehaviourAbilityComponentToActor(AActor* Actor) const
{
	UTechBehaviourAbilityComponent* BehaviourAbilityComponent = CreateBehaviourAbilityComponent(Actor);
	if (not IsValid(BehaviourAbilityComponent))
	{
		return;
	}

	Actor->AddInstanceComponent(BehaviourAbilityComponent);
	BehaviourAbilityComponent->RegisterComponent();
	BehaviourAbilityComponent->SetupBehAbilityFromSettings(BehaviourAbilitySettings);
}
