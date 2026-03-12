#include "UnitAbilityEntry.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapComp.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowedActor/TowedActor.h"
#include "RTS_Survival/RTSComponents/TowMechanic/VehicleTow/VehicleTow.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"

bool FAbilityHelpers::GetCanSquadRemanAbandonedTeamWeapon(
	ASquadController* SquadController,
	const ATeamWeapon* AbandonedTeamWeapon)
{
	if (not IsValid(SquadController) || not IsValid(AbandonedTeamWeapon))
	{
		return false;
	}

	if (not AbandonedTeamWeapon->GetIsAbandoned())
	{
		return false;
	}

	const int32 RequiredOperators = AbandonedTeamWeapon->GetRequiredOperators();
	if (RequiredOperators <= 0)
	{
		return false;
	}

	if (SquadController->GetSquadAlreadyHasTeamWeapon())
	{
		return false;
	}

	return SquadController->GetSquadUnitAmount() >= RequiredOperators;
}

bool FAbilityHelpers::GetCanTowTeamWeaponWithCurrentCargoCapacity(
	const ATankMaster* TowVehicle,
	const ATeamWeaponController* TeamWeaponController, UTowedActorComponent*& OutCompToTow)
{
	OutCompToTow = nullptr;
	if (not IsValid(TowVehicle))
	{
		return false;
	}

	if (not IsValid(TeamWeaponController))
	{
		return false;
	}

	const UVehicleTowComponent* const VehicleTowComponent = TowVehicle->FindComponentByClass<UVehicleTowComponent>();
	if (not IsValid(VehicleTowComponent) || not VehicleTowComponent->IsTowFree())
	{
		return false;
	}

	const UCargo* const CargoComponent = TowVehicle->FindComponentByClass<UCargo>();
	if (not IsValid(CargoComponent))
	{
		return false;
	}

	if (not TeamWeaponController->GetHasControlledTeamWeapon())
	{
		return false;
	}

	const UTowedActorComponent* TowedActorComponent = TeamWeaponController->GetControlledTeamWeaponTowedActorComponentNoReport();
	if (not IsValid(TowedActorComponent) || not TowedActorComponent->IsTowFree())
	{
		return false;
	}

	OutCompToTow = const_cast<UTowedActorComponent*>(TowedActorComponent);
	return CargoComponent->GetCanFitSquadPure(TeamWeaponController);
}

bool FAbilityHelpers::GetCanExecuteDetachTow(const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return false;
	}

	const UTowedActorComponent* TowedActorComponent = Actor->FindComponentByClass<UTowedActorComponent>();
	if (IsValid(TowedActorComponent) && not TowedActorComponent->IsTowFree())
	{
		return true;
	}

	const UVehicleTowComponent* VehicleTowComponent = Actor->FindComponentByClass<UVehicleTowComponent>();
	if (IsValid(VehicleTowComponent) && not VehicleTowComponent->IsTowFree())
	{
		return true;
	}

	return false;
}


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
