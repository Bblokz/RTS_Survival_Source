// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/PlayerUnitsNearTrigger/PlayerUnitsNearMissionTrigger.h"

#include "ForgeStationSightedTrigger.generated.h"

/**
 * @brief Trigger of the Forge station bonus: the generic player-units-near trigger centred on the station (actor tag
 * ForgeStation, 100 m), which also fires when any counted target already died, e.g. to artillery from outside the ring.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UForgeStationSightedTrigger : public UPlayerUnitsNearMissionTrigger
{
	GENERATED_BODY()

public:
	UForgeStationSightedTrigger();

protected:
	virtual bool CheckTrigger_Implementation() const override;
};
