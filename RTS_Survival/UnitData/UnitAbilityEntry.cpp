#include "UnitAbilityEntry.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapComp.h"

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

UAttachedWeaponAbilityComponent* FAbilityHelpers::GetAttachedWeaponAbilityComponent(
	const EAttachWeaponAbilitySubType Type, const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return nullptr;
	}

	TArray<UAttachedWeaponAbilityComponent*> AbilityComponents;
	Actor->GetComponents<UAttachedWeaponAbilityComponent>(AbilityComponents);
	for (UAttachedWeaponAbilityComponent* AbilityComponent : AbilityComponents)
	{
		if (not IsValid(AbilityComponent))
		{
			continue;
		}

		if (AbilityComponent->GetAttachedWeaponAbilityType() == Type)
		{
			return AbilityComponent;
		}
	}

	return nullptr;
}

UTurretSwapComp* FAbilityHelpers::GetTurretSwapAbilityComponent(const ETurretSwapAbility Type, const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return nullptr;
	}

	TArray<UTurretSwapComp*> TurretSwapComponents;
	Actor->GetComponents<UTurretSwapComp>(TurretSwapComponents);
	for (UTurretSwapComp* TurretSwapComponent : TurretSwapComponents)
	{
		if (not IsValid(TurretSwapComponent))
		{
			continue;
		}

		if (TurretSwapComponent->GetCurrentActiveSwapAbilityType() == Type)
		{
			return TurretSwapComponent;
		}
	}

	return nullptr;
}
