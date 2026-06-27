#include "FRTS_WorldStatics.h"

#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

FVector FRTS_WorldStatics::GetPlayerHQWorldLocation(AWorldPlayerController* Controller)
{
	bool bValid  =false;
	const FWorldCampaignState* WorldState = GetWorldState(Controller, bValid);
	if (not bValid)
	{
		return FVector::ZeroVector;
	}
	for (auto each)
}

const FWorldCampaignState* const FRTS_WorldStatics::GetWorldState(const AWorldPlayerController* Controller, bool& bIsValid)
{
		bIsValid = false;
	if (not IsValid(Controller))
	{
		return nullptr;
	}
	UWorldStateAndSaveManager* Manager = Controller->GetWorldStateAndSaveManager();
	if (not IsValid(Manager))
	{
		return nullptr;
	}
	const FWorldCampaignState* const WorldState = &(Manager->GetCachedWorldCampaignState())
	
	bIsValid = WorldState != nullptr;
	return WorldState;
}

