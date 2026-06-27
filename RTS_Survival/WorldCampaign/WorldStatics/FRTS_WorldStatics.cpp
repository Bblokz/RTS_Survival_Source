#include "FRTS_WorldStatics.h"

#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace
{
	FVector GetHQWorldLocationByAnchorKey(const FWorldCampaignState& WorldState, const FGuid& HQAnchorKey)
	{
		if (not HQAnchorKey.IsValid())
		{
			return FVector::ZeroVector;
		}

		for (const FWorldCampaignAnchorSaveData& AnchorSaveData : WorldState.Anchors)
		{
			if (AnchorSaveData.AnchorKey == HQAnchorKey)
			{
				return AnchorSaveData.Transform.GetLocation();
			}
		}

		return FVector::ZeroVector;
	}
}

FVector FRTS_WorldStatics::GetPlayerHQWorldLocation(AWorldPlayerController* Controller)
{
	bool bIsValidWorldState = false;
	const FWorldCampaignState* WorldState = GetWorldState(Controller, bIsValidWorldState);
	if (not bIsValidWorldState)
	{
		return FVector::ZeroVector;
	}

	return GetHQWorldLocationByAnchorKey(*WorldState, WorldState->PlayerHQAnchorKey);
}

FVector FRTS_WorldStatics::GetEnemyHQWorldLocation(AWorldPlayerController* Controller)
{
	bool bIsValidWorldState = false;
	const FWorldCampaignState* WorldState = GetWorldState(Controller, bIsValidWorldState);
	if (not bIsValidWorldState)
	{
		return FVector::ZeroVector;
	}

	return GetHQWorldLocationByAnchorKey(*WorldState, WorldState->EnemyHQAnchorKey);
}

const FWorldCampaignState* FRTS_WorldStatics::GetWorldState(const AWorldPlayerController* Controller, bool& bIsValid)
{
	bIsValid = false;
	if (not IsValid(Controller))
	{
		return nullptr;
	}

	const UWorldStateAndSaveManager* Manager = Controller->GetWorldStateAndSaveManager();
	if (not IsValid(Manager))
	{
		return nullptr;
	}

	const FWorldCampaignState* WorldState = &(Manager->GetCachedWorldCampaignState());
	bIsValid = WorldState != nullptr;
	return WorldState;
}
