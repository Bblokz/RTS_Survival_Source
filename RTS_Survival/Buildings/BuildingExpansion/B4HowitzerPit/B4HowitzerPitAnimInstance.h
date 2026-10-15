// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/GunPitAnimInstance.h"
#include "B4HowitzerPitAnimInstance.generated.h"

/**
 * @brief Anim instance of the B-4 gun pit turret; the whole gun sits on a turntable, so the turret yaw turns
 * B_Root and the pitch elevates B_Mantlet through two Modify Bone nodes (see UGunPitAnimInstance).
 * The reload montage keys the loading elevation itself and raises B4_LoadingElevation meanwhile.
 */
UCLASS()
class RTS_SURVIVAL_API UB4HowitzerPitAnimInstance : public UGunPitAnimInstance
{
	GENERATED_BODY()

public:
	UB4HowitzerPitAnimInstance();
};
