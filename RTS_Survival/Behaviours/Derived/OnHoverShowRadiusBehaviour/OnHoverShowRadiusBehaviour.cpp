// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/Behaviours/Derived/OnHoverShowRadiusBehaviour/OnHoverShowRadiusBehaviour.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UOnHoverShowRadiusBehaviour::OnAdded(AActor* BehaviourOwner)
{
	Super::OnAdded(BehaviourOwner);

	if (not IsValid(BehaviourOwner))
	{
		return;
	}

	M_RadiusAOEBehaviourComponent = BehaviourOwner->FindComponentByClass<URadiusAOEBehaviourComponent>();
	(void)GetIsValidRadiusAOEBehaviourComponent();
}

void UOnHoverShowRadiusBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	Super::OnRemoved(BehaviourOwner);

	M_RadiusAOEBehaviourComponent = nullptr;
}

void UOnHoverShowRadiusBehaviour::OnBehaviorHover(const bool bIsHovering)
{
	if (not GetIsValidRadiusAOEBehaviourComponent())
	{
		return;
	}

	M_RadiusAOEBehaviourComponent->OnHostBehaviourHovered(bIsHovering);
}

bool UOnHoverShowRadiusBehaviour::GetIsValidRadiusAOEBehaviourComponent() const
{
	if (M_RadiusAOEBehaviourComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_RadiusAOEBehaviourComponent",
		"GetIsValidRadiusAOEBehaviourComponent",
		GetOwningActor());
	return false;
}
