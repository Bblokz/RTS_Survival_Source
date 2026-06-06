// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "FactionBanner.generated.h"

/**
 * @brief Place this actor in the faction selection map so Blueprint banners can react to the current faction.
 */
UCLASS()
class RTS_SURVIVAL_API AFactionBanner : public AActor
{
	GENERATED_BODY()

public:
	AFactionBanner();

	UFUNCTION(BlueprintImplementableEvent, Category = "Faction Selection")
	void BP_OnFactionChanged(ERTSFaction CurrentFaction);
};
