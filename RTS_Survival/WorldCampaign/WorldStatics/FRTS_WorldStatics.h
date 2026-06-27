#pragma once

#include "CoreMinimal.h"


struct FWorldCampaignState;
class AWorldPlayerController;

/**
 * @brief Use this stateless helper when world-campaign code needs saved map positions.
 */
struct FRTS_WorldStatics
{
	static FVector GetPlayerHQWorldLocation(AWorldPlayerController* Controller);
	static FVector GetEnemyHQWorldLocation(AWorldPlayerController* Controller);

private:
	static const FWorldCampaignState* GetWorldState(const AWorldPlayerController* Controller, bool& bIsValid);
};
