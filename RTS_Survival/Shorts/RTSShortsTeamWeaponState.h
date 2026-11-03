// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTSShortsTeamWeaponState.generated.h"

class ATankMaster;
class ATeamWeapon;
class ATeamWeaponController;

/** @brief Distinguishes transport preparation from the crew's native deployment transition. */
UENUM()
enum class ERTSShortsTowPairPhase : uint8
{
	WaitingForCrew,
	HookRequested,
	Towing,
	Deploying,
	Ready
};

/** @brief Keeps each cinematic transport paired with its independently spawned crew and gun. */
USTRUCT()
struct FRTSShortsTowPair
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> Vehicle;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeaponController> Controller;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeapon> Weapon;

	ERTSShortsTowPairPhase Phase = ERTSShortsTowPairPhase::WaitingForCrew;
	FVector VehicleStartLocation = FVector::ZeroVector;
};
