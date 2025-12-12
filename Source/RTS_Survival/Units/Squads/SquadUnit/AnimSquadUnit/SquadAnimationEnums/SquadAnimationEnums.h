#pragma once


#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ESquadWeaponAimOffset : uint8
{
	Rifle,
	Pistol,
	Hip,
};

UENUM(blueprintType)
enum class ESquadMovementAnimState : uint8
{
	Idle,
	Walking,
	Running,
};

UENUM(BlueprintType)
enum class ESquadAimPosition : uint8
{
	Standing,
	Crouch,
	Prone,
};

UENUM(BlueprintType)
enum class ESquadWeaponMontage : uint8
{
	NoActiveWeaponMontage,
	ReloadRifle,
	ReloadHip,
	ReloadPistol,
	ReloadRifleProne,
	ReloadPistolProne,
	ReloadRifleCrouch,
	ReloadPistolCrouch,
	FireRifleSingle,
	FireRifleBurst,
	FireHipSingle,
	FireHipBurst,
	FirePistolSingle,
	SwitchToRifle,
	SwitchToPistol,
};

UENUM(BlueprintType)
enum class ESquadAimPositionMontage : uint8
{
	NoActiveAimPositionMontage,
	StandingToCrouch,
	HipToCrouch,
	CrouchToHip,
	CrouchToStanding,
	CrouchToPistol,
	PistolToCrouch,
	StandingToProne,
	ProneToStanding,
	HipToProne,
	ProneToHip,
	Misc_Welding,
};
