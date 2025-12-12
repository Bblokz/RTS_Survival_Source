#include "RTS_Survival/RTSGamePlayTags/FRTSGamePlayTagsBPLibrary.h"

#include "Components/ActorComponent.h"
#include "GameplayTagAssetInterface.h"

FGameplayTag UFRTSGamePlayTagsBPLibrary::GetScavengeMeshComponentTag()
{
	return FRTSGamePlayTags::GetScavengeMeshComponentTag();
}

void UFRTSGamePlayTagsBPLibrary::AddScavengeTagToComponent(UActorComponent* Component)
{
	if (not IsValid(Component))
	{
		return;
	}

	const FGameplayTag DesiredTag = FRTSGamePlayTags::GetScavengeMeshComponentTag();

	// There is no generic "add" via IGameplayTagAssetInterface; it's read-only from here.
	// We therefore add the tag name to ComponentTags so it’s visible & editable in Details.
	const FName TagName = DesiredTag.GetTagName();
	if (Component->ComponentTags.Contains(TagName))
	{
		return; // already present
	}
	Component->ComponentTags.Add(TagName);
}

bool FRTSGamePlayTagsHelpers::DoesComponentHaveScavengeTag(const UActorComponent* Component)
{
	if (not IsValid(Component))
	{
		return false;
	}

	const FGameplayTag Desired = FRTSGamePlayTags::GetScavengeMeshComponentTag();

	// 1) Prefer IGameplayTagAssetInterface if available
	if (const IGameplayTagAssetInterface* const TagAsset = Cast<IGameplayTagAssetInterface>(Component))
	{
		FGameplayTagContainer OwnedTags;
		TagAsset->GetOwnedGameplayTags(OwnedTags);

		if (OwnedTags.HasTag(Desired))
		{
			return true;
		}
	}

	// 2) Fallback: match against ComponentTags (FName).
	const FName GameplayTagName = Desired.GetTagName();
	if (Component->ComponentTags.Contains(GameplayTagName))
	{
		return true;
	}

	// Optional tolerance: exact string match if someone pasted the full tag string.
	const FString GameplayTagString = Desired.ToString();
	for (const FName& NameTag : Component->ComponentTags)
	{
		if (NameTag.ToString().Equals(GameplayTagString, ESearchCase::CaseSensitive))
		{
			return true;
		}
	}

	return false;
}
