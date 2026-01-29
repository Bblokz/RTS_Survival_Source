#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "RTSNavQueryFilterSettings.generated.h"

/**
 * @brief Central place to map RTS agent types to default Navigation Query Filters.
 * Designers can assign BP-derived UNavigationQueryFilter classes here.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Navigation Filters"))
class RTS_SURVIVAL_API URTSNavQueryFilterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSNavQueryFilterSettings();

	// If an agent has no explicit entry, this filter is used (may be null = use NavData/AIController defaults).
	UPROPERTY(EditAnywhere, Config, Category="Filters")
	TSoftClassPtr<UNavigationQueryFilter> DefaultFilterClass;

	// Per-agent mapping to filter classes (soft-class so designers can put BP filters here).
	UPROPERTY(EditAnywhere, Config, Category="Filters")
	TMap<ERTSNavAgents, TSoftClassPtr<UNavigationQueryFilter>> NavFiltersByAgent;

	/**
	 * @brief Resolve the filter class for a given agent (synchronously loads soft-classes if needed).
	 * @param Agent Agent enum.
	 * @return TSubclassOf<UNavigationQueryFilter> (can be null if nothing configured).
	 */
	TSubclassOf<UNavigationQueryFilter> ResolveFilterClassForAgent(const ERTSNavAgents Agent) const;

	/** Convenience accessor */
	static const URTSNavQueryFilterSettings* Get();
};
