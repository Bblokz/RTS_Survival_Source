#include "BehaviourButtonSettings.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIconStyleDataAsset.h"

UBehaviourButtonSettings::UBehaviourButtonSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Behaviour Buttons");
}

const UBehaviourIconStyleDataAsset* UBehaviourButtonSettings::GetBehaviourIconStyleDataAsset() const
{
	if (BehaviourIconStyleDataAsset.IsNull())
	{
		return nullptr;
	}

	return BehaviourIconStyleDataAsset.LoadSynchronous();
}
