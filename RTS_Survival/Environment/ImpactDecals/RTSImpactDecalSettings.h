// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSImpactDecalSettings.generated.h"

class UMaterialInterface;

/**
 * @brief One calibre band of shell craters. Every band has its own designed atlas cells, so a 150 mm crater is never a
 * 20 mm scuff blown up: within a band the decal only scales with the calibre around the band's design calibre.
 */
USTRUCT(BlueprintType)
struct FRTSImpactDecalBand
{
	GENERATED_BODY()

	// Largest calibre in this band, in mm; a shell belongs to the first band whose maximum it does not exceed.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="0"))
	float MaxCalibreMm = 25.f;

	// The calibre the band's art was designed for, in mm; this calibre gets exactly DesignDiameterCm.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1"))
	float DesignCalibreMm = 20.f;

	// Width of the decal box at the design calibre, in cm.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1"))
	float DesignDiameterCm = 40.f;

	// Depth of the decal box along the impact normal, in cm. Thin, so a unit standing on the spot catches little.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1"))
	float ProjectionDepthCm = 60.f;

	// First cell of this band in the 4x4 atlas of the decal material (0..15); the band uses CellCount cells from here.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="0", ClampMax="15"))
	int32 FirstAtlasCell = 0;

	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1", ClampMax="16"))
	int32 AtlasCellCount = 2;
};

/**
 * @brief One explosive-weight band of bomb craters (aircraft bombs, bomb rockets, missiles). Bombs bury before going
 * off and dig far larger, earthier craters than shells, so they have their own atlas instead of a shell band.
 * Within a band the crater scales with the cube root of the explosive weight (crater size follows charge^(1/3)).
 */
USTRUCT(BlueprintType)
struct FRTSBombCraterBand
{
	GENERATED_BODY()

	// Heaviest charge in this band, as the weapon's TNTExplosiveGrams (250 for the "250Gr Bomb" up to 5000 for the
	// largest bomb rocket); a bomb belongs to the first band whose maximum it does not exceed.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="0"))
	float MaxTntGrams = 300.f;

	// The charge the band's art was designed for; this charge gets exactly DesignDiameterCm.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1"))
	float DesignTntGrams = 250.f;

	// Width of the decal box at the design charge, in cm.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1"))
	float DesignDiameterCm = 900.f;

	// Depth of the decal box along the impact normal, in cm.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1"))
	float ProjectionDepthCm = 250.f;

	// First cell of this band in the 4x4 bomb atlas (0..15); the band uses AtlasCellCount cells from here.
	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="0", ClampMax="15"))
	int32 FirstAtlasCell = 0;

	UPROPERTY(EditAnywhere, Config, Category="Band", meta=(ClampMin="1", ClampMax="16"))
	int32 AtlasCellCount = 4;
};

/**
 * @brief Shell craters on the landscape, drawn by ULandscapeImpactDecalSubsystem on every map with a landscape.
 * Every crater uses the one decal material below; the Decal Color of each decal picks its atlas cell
 * (R = (cell + 0.5) / 16), a variation value (G) and its opacity (A), so no decal needs a material instance.
 * Edit in Project Settings -> Game -> RTS Impact Decals. The player picks the count (Off / 50 / 100 / 200 / 400) in the
 * graphics settings (console variable rts.ImpactDecals).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Impact Decals"))
class RTS_SURVIVAL_API URTSImpactDecalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSImpactDecalSettings();

	static const URTSImpactDecalSettings* Get();

	// The band a calibre belongs to, or nullptr when the shell is too small for a crater.
	const FRTSImpactDecalBand* FindBand(float CalibreMm, int32& OutBandIndex) const;

	// The band a bomb's explosive weight belongs to, or nullptr for no charge.
	const FRTSBombCraterBand* FindBombBand(float TntGrams, int32& OutBandIndex) const;

	// The one deferred decal material of every shell crater (4x4 atlas, cell picked by Decal Color R).
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals")
	TSoftObjectPtr<UMaterialInterface> DecalMaterial;

	// Bomb craters: same master material with the bomb atlas (4x4, cell picked by Decal Color R).
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals|Bombs")
	TSoftObjectPtr<UMaterialInterface> BombDecalMaterial;

	// Four bands by explosive weight, ordered by MaxTntGrams; the last band takes every heavier charge.
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals|Bombs")
	TArray<FRTSBombCraterBand> BombBands;

	// How far a charge may scale its band's design size (cube root of charge / design charge is clamped to this).
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals|Bombs")
	FFloatInterval BombScaleClamp = FFloatInterval(0.8f, 1.3f);

	// Only shells larger than this leave a crater, in mm (small arms and heavy machine guns do not).
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals", meta=(ClampMin="0"))
	float MinCalibreMm = 15.f;

	// Eight bands, ordered by MaxCalibreMm; the last band takes every larger calibre.
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals")
	TArray<FRTSImpactDecalBand> Bands;

	// How far a calibre may scale its band's design size (calibre / design calibre is clamped to this).
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals")
	FFloatInterval CalibreScaleClamp = FFloatInterval(0.8f, 1.25f);

	// Random size spread on top of the calibre scale.
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals")
	FFloatInterval RandomSizeScale = FFloatInterval(0.92f, 1.08f);

	/**
	 * A shell that explodes on its flight timer (arcing artillery is timed to land at its target, so its last trace may
	 * not have reached the ground) still leaves a crater when landscape is within this distance below it, in cm.
	 * Higher air bursts leave nothing.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals", meta=(ClampMin="0"))
	float MaxTimedExplosionGroundGapCm = 150.f;

	// Scales every crater; 1 is the band design size.
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals", meta=(ClampMin="0.1"))
	float GlobalSizeMultiplier = 1.f;

	/**
	 * A new crater whose centre lies within this fraction of an existing crater's radius, and that is not larger than
	 * that crater, refreshes it instead of adding a decal on top (barrages on one spot): no overdraw and the older
	 * craters elsewhere are not pushed out. 0 always adds.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals|Performance", meta=(ClampMin="0", ClampMax="1"))
	float MergeRadiusFraction = 0.35f;

	// Decals smaller than this on screen fade out (UDecalComponent FadeScreenSize); 0 never fades.
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals|Performance", meta=(ClampMin="0"))
	float FadeScreenSize = 0.f;

	// Sort order of the oldest crater; every new crater gets the next value so newer hits draw over older ones, and
	// all of them over map placed ground decals (sort order 0).
	UPROPERTY(EditAnywhere, Config, Category="Impact Decals")
	int32 SortOrder = 1;
};
