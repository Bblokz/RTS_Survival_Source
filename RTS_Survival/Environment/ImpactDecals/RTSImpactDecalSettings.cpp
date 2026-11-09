// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSImpactDecalSettings.h"

#include "Materials/MaterialInterface.h"

namespace RTSImpactDecalSettingsPrivate
{
	struct FDefaultBand
	{
		float MaxCalibreMm;
		float DesignCalibreMm;
		float DesignDiameterCm;
		float ProjectionDepthCm;
	};

	// 20 mm autocannon up to railway guns; each band has two atlas cells (2b, 2b + 1).
	constexpr FDefaultBand DefaultBands[] = {
		{25.f, 20.f, 40.f, 40.f},
		{40.f, 34.f, 70.f, 50.f},
		{60.f, 50.f, 110.f, 60.f},
		// 75 mm and up: field guns and howitzers dig real craters, so these are designed large.
		{80.f, 75.f, 240.f, 110.f},
		{100.f, 88.f, 330.f, 130.f},
		{130.f, 112.f, 450.f, 160.f},
		{180.f, 150.f, 600.f, 200.f},
		{100000.f, 240.f, 850.f, 260.f},
	};
	constexpr int32 CellsPerBand = 2;

	struct FDefaultBombBand
	{
		float MaxTntGrams;
		float DesignTntGrams;
		float DesignDiameterCm;
		float ProjectionDepthCm;
	};

	// Aircraft bombs and missiles by explosive weight; each band has four cells (4k .. 4k + 3) in the bomb atlas.
	constexpr FDefaultBombBand DefaultBombBands[] = {
		{300.f, 250.f, 900.f, 250.f},
		{600.f, 500.f, 1200.f, 300.f},
		{1200.f, 1000.f, 1600.f, 350.f},
		{1000000.f, 2500.f, 2200.f, 450.f},
	};
	constexpr int32 CellsPerBombBand = 4;
}

URTSImpactDecalSettings::URTSImpactDecalSettings()
{
	DecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/MI_LandscapeImpactDecal.MI_LandscapeImpactDecal")));
	int32 BandIndex = 0;
	for (const RTSImpactDecalSettingsPrivate::FDefaultBand& Default : RTSImpactDecalSettingsPrivate::DefaultBands)
	{
		FRTSImpactDecalBand& Band = Bands.AddDefaulted_GetRef();
		Band.MaxCalibreMm = Default.MaxCalibreMm;
		Band.DesignCalibreMm = Default.DesignCalibreMm;
		Band.DesignDiameterCm = Default.DesignDiameterCm;
		Band.ProjectionDepthCm = Default.ProjectionDepthCm;
		Band.FirstAtlasCell = BandIndex * RTSImpactDecalSettingsPrivate::CellsPerBand;
		Band.AtlasCellCount = RTSImpactDecalSettingsPrivate::CellsPerBand;
		++BandIndex;
	}

	BombDecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/MI_BombImpactDecal.MI_BombImpactDecal")));
	int32 BombBandIndex = 0;
	for (const RTSImpactDecalSettingsPrivate::FDefaultBombBand& Default : RTSImpactDecalSettingsPrivate::DefaultBombBands)
	{
		FRTSBombCraterBand& Band = BombBands.AddDefaulted_GetRef();
		Band.MaxTntGrams = Default.MaxTntGrams;
		Band.DesignTntGrams = Default.DesignTntGrams;
		Band.DesignDiameterCm = Default.DesignDiameterCm;
		Band.ProjectionDepthCm = Default.ProjectionDepthCm;
		Band.FirstAtlasCell = BombBandIndex * RTSImpactDecalSettingsPrivate::CellsPerBombBand;
		Band.AtlasCellCount = RTSImpactDecalSettingsPrivate::CellsPerBombBand;
		++BombBandIndex;
	}
}

const URTSImpactDecalSettings* URTSImpactDecalSettings::Get()
{
	return GetDefault<URTSImpactDecalSettings>();
}

const FRTSImpactDecalBand* URTSImpactDecalSettings::FindBand(const float CalibreMm, int32& OutBandIndex) const
{
	OutBandIndex = INDEX_NONE;
	if (CalibreMm <= MinCalibreMm || Bands.IsEmpty())
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < Bands.Num(); ++Index)
	{
		if (CalibreMm <= Bands[Index].MaxCalibreMm)
		{
			OutBandIndex = Index;
			return &Bands[Index];
		}
	}
	OutBandIndex = Bands.Num() - 1;
	return &Bands.Last();
}

const FRTSBombCraterBand* URTSImpactDecalSettings::FindBombBand(const float TntGrams, int32& OutBandIndex) const
{
	OutBandIndex = INDEX_NONE;
	if (TntGrams <= 0.f || BombBands.IsEmpty())
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < BombBands.Num(); ++Index)
	{
		if (TntGrams <= BombBands[Index].MaxTntGrams)
		{
			OutBandIndex = Index;
			return &BombBands[Index];
		}
	}
	OutBandIndex = BombBands.Num() - 1;
	return &BombBands.Last();
}
