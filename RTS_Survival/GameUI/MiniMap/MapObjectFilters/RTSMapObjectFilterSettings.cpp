// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMapObjectFilterSettings.h"

namespace MapObjectFilterSettingsDefaults
{
	// Colours of the DT_Resources_RichText rows the resource icons are tinted with.
	const FLinearColor RadixiteColor(0.171441f, 0.982251f, 0.122139f, 1.f);
	const FLinearColor MetalColor(0.254152f, 0.89627f, 1.f, 1.f);
	const FLinearColor VehiclePartsColor(0.372549f, 0.054902f, 0.f, 1.f);
	// Vehicle parts red is dark on the terrain; a light ring keeps the scavenge dots readable.
	const FLinearColor ScavengeableOutlineColor(1.f, 0.82f, 0.7f, 0.75f);
	constexpr float ScavengeableOutlineWidth = 1.f;
}

URTSMapObjectFilterSettings::URTSMapObjectFilterSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Map Object Filters");

	using namespace MapObjectFilterSettingsDefaults;
	// Radixite fields are many and small, metal fewer and a bit larger, scavenge objects the largest.
	RadixiteLook.Color = RadixiteColor;
	RadixiteLook.MiniMapDiameter = 2.5f;
	RadixiteLook.TacticalMapDiameter = 4.f;

	MetalLook.Color = MetalColor;
	MetalLook.MiniMapDiameter = 4.f;
	MetalLook.TacticalMapDiameter = 6.5f;

	ScavengeableLook.Color = VehiclePartsColor;
	ScavengeableLook.MiniMapDiameter = 4.5f;
	ScavengeableLook.TacticalMapDiameter = 7.5f;
	ScavengeableLook.OutlineColor = ScavengeableOutlineColor;
	ScavengeableLook.OutlineWidth = ScavengeableOutlineWidth;
}

const FRTSMapObjectFilterLook& URTSMapObjectFilterSettings::GetLook(const ERTSMapObjectFilter Filter) const
{
	switch (Filter)
	{
	case ERTSMapObjectFilter::Metal:
		return MetalLook;
	case ERTSMapObjectFilter::Scavengeables:
		return ScavengeableLook;
	case ERTSMapObjectFilter::Radixite:
	default:
		return RadixiteLook;
	}
}
