// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SurvivalMapSite.generated.h"

/** @brief What a survival map site may hold; a site picker node reads the sites of one category. */
UENUM(BlueprintType)
enum class ESurvivalMapSiteCategory : uint8
{
	Capture,
	Radixite,
	Metal,
	Wreck,
	VehicleParts,
	Animals
};

/**
 * @brief A spot a designer vouched for on a survival map: flat enough, on the ground, clear of ramps and dressing.
 * The map's content graph picks a seeded part of these per match, so the same terrain gets different content.
 * @note Place on the ground with Category and SiteRadius set; nothing renders in game.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASurvivalMapSite : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalMapSite();

	ESurvivalMapSiteCategory GetCategory() const { return Category; }
	float GetSiteRadius() const { return SiteRadius; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Map Site")
	ESurvivalMapSiteCategory Category = ESurvivalMapSiteCategory::Capture;

	// Everything picked for this site stands inside this radius; the ground in it must suit the category.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Map Site", meta = (ClampMin = "100.0"))
	float SiteRadius = 800.f;
};
