#include "RTSNavQueryFilterSettings.h"
#include "Engine/AssetManager.h"

URTSNavQueryFilterSettings::URTSNavQueryFilterSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTS Navigation Filters");
}

const URTSNavQueryFilterSettings* URTSNavQueryFilterSettings::Get()
{
	return GetDefault<URTSNavQueryFilterSettings>();
}

TSubclassOf<UNavigationQueryFilter> URTSNavQueryFilterSettings::ResolveFilterClassForAgent(const ERTSNavAgents Agent) const
{
	// Prefer explicit mapping, otherwise fallback.
	const TSoftClassPtr<UNavigationQueryFilter>* const FoundSoft = NavFiltersByAgent.Find(Agent);
	const TSoftClassPtr<UNavigationQueryFilter> SoftClassPtr = FoundSoft ? *FoundSoft : DefaultFilterClass;

	if (SoftClassPtr.IsNull())
	{
		return nullptr;
	}

	// Load synchronously once; in practice these are light classes.
	UClass* const Loaded = SoftClassPtr.LoadSynchronous();
	if (not Loaded)
	{
		RTSFunctionLibrary::ReportError(TEXT("URTSNavQueryFilterSettings::ResolveFilterClassForAgent: Failed to load filter class for agent."));
		return nullptr;
	}

	return Loaded;
}
