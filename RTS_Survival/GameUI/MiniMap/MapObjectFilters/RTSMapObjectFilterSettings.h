// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterTypes.h"
#include "RTSMapObjectFilterSettings.generated.h"

/**
 * @brief Project settings (Game > Map Object Filters) for the discovered radixite, metal and scavenge object dots
 * the tactical map toggles. URTSMapObjectFilterSubsystem reads the discovery values, UW_MiniMap the looks.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Map Object Filters"))
class RTS_SURVIVAL_API URTSMapObjectFilterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSMapObjectFilterSettings();

	const FRTSMapObjectFilterLook& GetLook(const ERTSMapObjectFilter Filter) const;

	/** Same green as the radixite resource icon (DT_Resources_RichText row Radixite). */
	UPROPERTY(Config, EditAnywhere, Category = "Radixite", meta = (ShowOnlyInnerProperties))
	FRTSMapObjectFilterLook RadixiteLook;

	/** Same blue as the metal resource icon (DT_Resources_RichText row Metal). */
	UPROPERTY(Config, EditAnywhere, Category = "Metal", meta = (ShowOnlyInnerProperties))
	FRTSMapObjectFilterLook MetalLook;

	/** Same colour as the vehicle parts resource (DT_Resources_RichText row Vehicle). */
	UPROPERTY(Config, EditAnywhere, Category = "Scavenge Objects", meta = (ShowOnlyInnerProperties))
	FRTSMapObjectFilterLook ScavengeableLook;

	/**
	 * How often discovery is re-read from the fog of war while a filter is shown. Only runs while a filter is
	 * visible on an open map; with every filter off nothing runs.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery", meta = (ClampMin = "0.1", ClampMax = "10.0", Units = "s"))
	float DiscoveryRefreshSeconds = 1.f;

	/** Explored-fog value (0..1) from which a spot counts as discovered. */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float DiscoveredThreshold = 0.2f;

	/** Fog texels around an object that are checked too, so objects at the edge of explored land count. */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery", meta = (ClampMin = "0", ClampMax = "8"))
	int32 DiscoverySampleRadiusTexels = 1;
};
