#include "UnitAbilityEntry.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"

UGrenadeComponent* FAbilityHelpers::GetGrenadeAbilityCompOfType(const EGrenadeAbilityType Type,
	const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return nullptr;
	}

	TArray<UGrenadeComponent*> GrenadeComponents;
	Actor->GetComponents<UGrenadeComponent>(GrenadeComponents);
	for (UGrenadeComponent* GrenadeComponent : GrenadeComponents)
	{
		if (not IsValid(GrenadeComponent))
		{
			continue;
		}

		if (GrenadeComponent->GetGrenadeAbilityType() == Type)
		{
			return GrenadeComponent;
		}
	}

	return nullptr;
}

UAimAbilityComponent* FAbilityHelpers::GetHasAimAbilityComponent(const EAimAbilityType Type, const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return nullptr;
	}

	TArray<UAimAbilityComponent*> AimComponents;
	Actor->GetComponents<UAimAbilityComponent>(AimComponents);
	for (UAimAbilityComponent* AimComponent : AimComponents)
	{
		if (not IsValid(AimComponent))
		{
			continue;
		}

		if (AimComponent->GetAimAbilityType() == Type)
		{
			return AimComponent;
		}
	}

	return nullptr;
}
