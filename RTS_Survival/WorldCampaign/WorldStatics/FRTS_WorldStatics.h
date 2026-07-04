#pragma once

#include "CoreMinimal.h"


struct FWorldCampaignState;
class AWorldPlayerController;
class UPlayerResourceManager;

/**
 * @brief Use this stateless helper when world-campaign code needs saved map positions.
 */
struct FRTS_WorldStatics
{
	static FVector GetPlayerHQWorldLocation(AWorldPlayerController* Controller);
	static FVector GetEnemyHQWorldLocation(AWorldPlayerController* Controller);
	static UPlayerResourceManager* GetPlayerResourceManager(const UObject* WorldContextObject);

private:
	static const AWorldPlayerController* GetWorldPlayerController(const UObject* WorldContextObject);
	static const FWorldCampaignState* GetWorldState(const AWorldPlayerController* Controller, bool& bIsValid);
};
