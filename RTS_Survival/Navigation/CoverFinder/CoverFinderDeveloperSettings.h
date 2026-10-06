#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CoverFinderDeveloperSettings.generated.h"

/**
 * @brief Exposes the small set of accuracy, cadence, and budget controls needed to tune infantry cover discovery.
 * Geometry classification details remain internal so maps share one predictable definition of cover.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Infantry Cover Finder"))
class RTS_SURVIVAL_API URTSCoverFinderDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSCoverFinderDeveloperSettings();

	UPROPERTY(Config, EditAnywhere, Category="Cover Search")
	bool bM_EnableCoverSearch = true;

	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="1.0", UIMin="1.0", UIMax="60.0", Units="s"))
	float M_RescanIntervalSeconds = 5.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="60.0", UIMin="60.0", UIMax="300.0", Units="cm"))
	float M_SearchGridSpacing = 200.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="0.05", UIMin="0.05", UIMax="2.0", Units="ms"))
	float M_GameThreadBudgetMilliseconds = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="40.0", ClampMax="143.0", UIMin="40.0", UIMax="143.0", Units="cm"))
	float M_MinimumCrouchCoverHeight = 90.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="60.0", UIMin="60.0", UIMax="300.0", Units="cm"))
	float M_MaximumCoverSearchDistance = 175.0f;

	// Minimum edge-to-edge opening that must fit the full standing capsule before a side peek is published.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="80.0", UIMin="80.0", UIMax="250.0", Units="cm"))
	float M_StandingPeekGapWidth = 110.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="50.0", UIMin="50.0", UIMax="250.0", Units="cm"))
	float M_CoverPointSpacing = 90.0f;

	UPROPERTY(Config, EditAnywhere, Category="Debug")
	bool bM_DrawDetectedCover = true;

	UPROPERTY(Config, EditAnywhere, Category="Debug", meta=(ClampMin="1.0", UIMin="1.0", UIMax="60.0", Units="s", EditCondition="bM_DrawDetectedCover"))
	float M_DebugDrawDurationSeconds = 10.0f;

	static const URTSCoverFinderDeveloperSettings* Get();
};
