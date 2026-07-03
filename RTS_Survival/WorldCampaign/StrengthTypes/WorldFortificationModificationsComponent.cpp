#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"

UWorldFortificationModificationsComponent::UWorldFortificationModificationsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldFortificationModificationsComponent::SetFortificationStrengths(
	const TArray<EWorldFortificationStrength>& FortificationStrengths)
{
	M_FortificationStrengths = FortificationStrengths;
	M_FortificationStrengths.RemoveAll([](const EWorldFortificationStrength FortificationStrength)
	{
		return FortificationStrength == EWorldFortificationStrength::None;
	});
}

void UWorldFortificationModificationsComponent::AddFortificationStrength(
	const EWorldFortificationStrength FortificationStrength)
{
	if (FortificationStrength == EWorldFortificationStrength::None)
	{
		return;
	}

	M_FortificationStrengths.AddUnique(FortificationStrength);
}

void UWorldFortificationModificationsComponent::RemoveFortificationStrength(
	const EWorldFortificationStrength FortificationStrength)
{
	M_FortificationStrengths.Remove(FortificationStrength);
}
