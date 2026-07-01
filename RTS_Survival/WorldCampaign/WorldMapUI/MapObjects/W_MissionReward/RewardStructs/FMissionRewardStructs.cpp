#include "FMissionRewardStructs.h"

const TArray<ERTSCard>& FPrimaryReward::GetCardRewardsForFaction(const ERTSFaction PlayerFaction) const
{
	for (const FFactionCardRewards& FactionCardRewards : CardRewards)
	{
		if (FactionCardRewards.Faction == PlayerFaction)
		{
			return FactionCardRewards.Cards;
		}
	}

	static const TArray<ERTSCard> EmptyCardRewards;
	return EmptyCardRewards;
}
