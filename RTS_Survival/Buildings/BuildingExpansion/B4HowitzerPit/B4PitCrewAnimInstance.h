// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/GunPitCrewAnimInstance.h"
#include "B4PitCrewAnimInstance.generated.h"

/**
 * @brief Anim instance of one B-4 gun pit crew member (AnimBp_B4Pit_Crew); the generic gun pit crew instance kept
 * under its own name for the existing blueprint. Any infantry-skeleton pit crew can use AnimBp_B4Pit_Crew.
 */
UCLASS()
class RTS_SURVIVAL_API UB4PitCrewAnimInstance : public UGunPitCrewAnimInstance
{
	GENERATED_BODY()
};
