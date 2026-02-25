#pragma once

#include "CoreMinimal.h"

#include "TeamWeaponState.generated.h"

UENUM(BlueprintType)
enum class ETeamWeaponState : uint8
{
	Uninitialized,
	Spawning,
	Ready_Packed,
	Packing,
	Moving,
	Deploying,
	Ready_Deployed,
	Abandoned,
};


UENUM(BlueprintType)
enum class ETeamWeaponMontage : uint8
{
	NoMontage,
	DeployMontage,
	PackMontage,
	MoveForwardMontage,
	MoveBackwardMontage,
	FireMontage
};
