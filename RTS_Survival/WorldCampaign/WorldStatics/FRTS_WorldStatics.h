#pragma once

#include "CoreMinimal.h"


struct FWorldCampaignState;
class AWorldPlayerController;

struct FRTS_WorldStatics
{
	static 	FVector GetPlayerHQWorldLocation(AWorldPlayerController * Controller);
	static FVector GetEnemyHQWorldLocation(AWorldPlayerController * Controller);
	
private:
	static const FWorldCampaignState* const GetWorldState(const AWorldPlayerController* Controller, bool& bIsValid);
};
