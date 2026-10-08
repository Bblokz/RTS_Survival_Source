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

/** Selects the pose family evaluated by the idle branch without changing locomotion state. */
UENUM(BlueprintType)
enum class ESquadIdleAnimationPose : uint8
{
	Regular,
	CrouchCover,
	StandingCoverLeft,
	StandingCoverRight,
	StandingPeekLeft,
	StandingPeekRight,
	// In a trench: crouched below the edge.
	TrenchCover,
	// In a trench: stood up in place to fire.
	TrenchPeek,
};

/**
 * The three branches the AnimGraph's idle section needs for cover, whatever the cover type.
 * Which idle sequence or aim offset a branch plays comes from GetCurrentCoverIdlePose,
 * GetCurrentCoverAimOffset and GetCurrentCoverAimBaseSequence, so new cover types add no graph nodes.
 */
UENUM(BlueprintType)
enum class ESquadCoverGraphPose : uint8
{
	// Regular idle and aim-offset setup.
	NotInCover,
	// Hidden behind cover: play the protected idle sequence, no aim offset.
	CoverIdle,
	// In a pose that can aim: crouch cover while the weapon has a target, standing cover once stepped out.
	CoverAim,
};

/** Tracks which authored cover transition currently owns the unit's full-body animation. */
UENUM(BlueprintType)
enum class ESquadCoverAnimAction : uint8
{
	None,
	Entering,
	Protected,
	Exposing,
	Exposed,
	Returning,
	Exiting,
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
	Misc_Grenade,
};
