// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "Abilities.generated.h"

/*Enum containing all possible Ability IDs*/
UENUM(BlueprintType)
enum class EAbilityID : uint8
{
	IdNoAbility,
	// This enum is not used in command queues but when squad units are busy moving closer to the specific target.
	// On the squad controller the IdAttack is set as the active ability.
	IdNoAbility_MoveCloserToTarget,
	// Used for squad units to evade allied units, will not trigger logic on the squad controller.
	IdNoAbility_MoveToEvasionLocation,
	// Used when the unit has no commands in the queue.
	IdIdle,
	IdGeneral_Confirm,
	IdAttack,
	IdAttackGround,
	IdMove,
	IdPatrol,
	IdStop,
	IdSwitchWeapon,
	IdSwitchMelee,
	IdProne,
	IdCrouch,
	IdStand,
	IdSwitchSingleBurst,
	IdReverseMove,
	IdRotateTowards,
	IdCreateBuilding,
	IdConvertToVehicle,
	IdHarvestResource,
	IdReturnCargo,
	IdPickupItem,
	IdScavenge,
	IdDigIn,
	IdBreakCover,
	IdFireRockets,
	IdCancelRocketFire,
	IdRocketsReloading,
	IdRepair,
	IdReturnToBase,
	IdAircraftOwnerNotExpanded,
	IdEnterCargo,
	IdExitCargo,
	IdEnableResourceConversion,
	IdDisableResourceConversion,
	IdCapture,
};

inline static FString Global_GetAbilityIDAsString(const EAbilityID Ability)
{
	switch (Ability)
	{
	case EAbilityID::IdNoAbility: return TEXT("No Ability");
	case EAbilityID::IdIdle: return TEXT("Idle");
	case EAbilityID::IdAttack: return TEXT("Attack");
		case EAbilityID::IdGeneral_Confirm: return TEXT("Confirm");
	case EAbilityID::IdAttackGround: return TEXT("AttackGround");
	case EAbilityID::IdMove: return TEXT("Move");
	case EAbilityID::IdPatrol: return TEXT("Patrol");
	case EAbilityID::IdStop: return TEXT("Stop");
	case EAbilityID::IdSwitchWeapon: return TEXT("Switch Weapon");
	case EAbilityID::IdSwitchMelee: return TEXT("Switch to Melee");
	case EAbilityID::IdProne: return TEXT("Go Prone");
	case EAbilityID::IdCrouch: return TEXT("Crouch");
	case EAbilityID::IdStand: return TEXT("Stand");
	case EAbilityID::IdSwitchSingleBurst: return TEXT("Switch Fire Mode");
	case EAbilityID::IdReverseMove: return TEXT("Reverse Move");
	case EAbilityID::IdRotateTowards: return TEXT("Rotate Towards");
	case EAbilityID::IdCreateBuilding: return TEXT("Create Building");
	case EAbilityID::IdConvertToVehicle: return TEXT("Convert to Vehicle");
	case EAbilityID::IdHarvestResource: return TEXT("Harvest Resource");
	case EAbilityID::IdReturnCargo: return TEXT("Return Cargo");
	case EAbilityID::IdPickupItem: return TEXT("Pickup Item");
	case EAbilityID::IdScavenge: return TEXT("Scavenge");
	case EAbilityID::IdDigIn: return TEXT("Dig In");
	case EAbilityID::IdBreakCover: return TEXT("Break Cover");
		case EAbilityID::IdCapture: return TEXT("Capture");
	case EAbilityID::IdFireRockets: return TEXT("Fire Rockets");
	case EAbilityID::IdCancelRocketFire: return TEXT("Cancel Rocket Fire");
	case EAbilityID::IdRocketsReloading: return TEXT("Rockets Reloading");
	case EAbilityID::IdRepair: return TEXT("Repair");
	case EAbilityID::IdReturnToBase: return TEXT("Back To Base");
		case EAbilityID::IdEnterCargo: return TEXT("Enter Cargo");
		case EAbilityID::IdExitCargo: return TEXT("Exit Cargo");
		case EAbilityID::IdAircraftOwnerNotExpanded: return TEXT("Aircraft Owner Not Expanded");
	case EAbilityID::IdNoAbility_MoveCloserToTarget: return TEXT("Move Closer To Target");
	case EAbilityID::IdDisableResourceConversion: return TEXT("Disable Resource Conversion");
	case EAbilityID::IdEnableResourceConversion: return TEXT("Enable Resource Conversion");
	default: return TEXT("Unknown Ability");
	}
}
