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

	// Budget for the very first scan of a map. Nothing can take cover until that scan is published, so it may
	// cost more per frame than the periodic refreshes that follow it.
	UPROPERTY(Config, EditAnywhere, Category="Cover Search", meta=(ClampMin="0.05", UIMin="0.35", UIMax="10.0", Units="ms"))
	float M_FirstScanGameThreadBudgetMilliseconds = 4.0f;

	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover")
	bool bM_EnableAutomaticCoverUse = true;

	// With only squads selected, shows where each soldier will go under the cursor and sends them exactly there.
	// Off, squads move with the regular formation slots again.
	UPROPERTY(Config, EditAnywhere, Category="Squad Move Preview")
	bool bM_EnableSquadMovePreview = true;

	// Limits how far an otherwise idle or already-in-range infantry unit may reposition itself for cover.
	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover", meta=(ClampMin="100.0", UIMin="100.0", UIMax="2500.0", Units="cm"))
	float M_AutomaticCoverSearchRadius = 1200.0f;

	// Staggers decisions across frames; movement completion itself is still handled immediately by the AI callback.
	UPROPERTY(Config, EditAnywhere, Category="Automatic Cover", meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="32"))
	int32 M_MaximumTacticalUnitUpdatesPerFrame = 8;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="40.0", ClampMax="143.0", UIMin="40.0", UIMax="143.0", Units="cm"))
	float M_MinimumCrouchCoverHeight = 90.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="60.0", UIMin="60.0", UIMax="300.0", Units="cm"))
	float M_MaximumCoverSearchDistance = 175.0f;

	// Minimum edge-to-edge opening that must fit the full standing capsule before a side peek is published.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="80.0", UIMin="80.0", UIMax="250.0", Units="cm"))
	float M_StandingPeekGapWidth = 110.0f;

	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="50.0", UIMin="50.0", UIMax="250.0", Units="cm"))
	float M_CoverPointSpacing = 90.0f;

	// How far inside the end of a high wall a standing point is placed. Must be smaller than the sideways step of
	// the shortest expose animation, or the peeking soldier's muzzle stays behind the wall.
	UPROPERTY(Config, EditAnywhere, Category="Cover Classification", meta=(ClampMin="0.0", ClampMax="80.0", UIMin="0.0", UIMax="80.0", Units="cm"))
	float M_StandingPeekEdgeInset = 25.0f;

	UPROPERTY(Config, EditAnywhere, Category="Debug")
	bool bM_DrawDetectedCover = true;

	UPROPERTY(Config, EditAnywhere, Category="Debug", meta=(ClampMin="1.0", UIMin="1.0", UIMax="60.0", Units="s", EditCondition="bM_DrawDetectedCover"))
	float M_DebugDrawDurationSeconds = 10.0f;

	static const URTSCoverFinderDeveloperSettings* Get();
};
