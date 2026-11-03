// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyTypes.h"
#include "Styling/SlateBrush.h"

#include "SurvivalArmyUiTypes.generated.h"

/** The roster filter tabs; All shows everything, the others map unit types (see GetRosterCategory). */
UENUM()
enum class ESurvivalRosterCategory : uint8
{
	All,
	Infantry,
	Vehicles,
	Nomadic,
	Aircraft
};

/**
 * @brief One roster entry as the army selection screen shows it: the library entry plus everything resolved for
 * display once (icon, variant name, tooltip), so slots and drag visuals never look the data up again.
 */
USTRUCT()
struct FSurvivalRosterItem
{
	GENERATED_BODY()

	UPROPERTY()
	FSurvivalArmyEntry Entry;

	// The training picture of the unit as its selected variant; a brush without resource draws nothing.
	UPROPERTY()
	FSlateBrush Icon;

	// The variant's display name; empty for Base, so the badge hides.
	UPROPERTY()
	FText VariantName;

	// Name, variant, description and a compact stat line.
	UPROPERTY()
	FText Tooltip;

	UPROPERTY()
	ESurvivalRosterCategory Category = ESurvivalRosterCategory::All;

	// Picks the battle wear of the slot; the same unit always gets the same seed.
	UPROPERTY()
	float WearSeed = 0.f;
};

namespace SurvivalArmyUi
{
	/** @return The tab a unit type belongs to; harvesters count as vehicles. */
	inline ESurvivalRosterCategory GetRosterCategory(const EAllUnitType UnitType)
	{
		switch (UnitType)
		{
		case EAllUnitType::UNType_Squad:
			return ESurvivalRosterCategory::Infantry;
		case EAllUnitType::UNType_Tank:
		case EAllUnitType::UNType_Harvester:
			return ESurvivalRosterCategory::Vehicles;
		case EAllUnitType::UNType_Nomadic:
			return ESurvivalRosterCategory::Nomadic;
		case EAllUnitType::UNType_Aircraft:
			return ESurvivalRosterCategory::Aircraft;
		default:
			return ESurvivalRosterCategory::All;
		}
	}

	inline FText GetRosterCategoryLabel(const ESurvivalRosterCategory Category)
	{
		switch (Category)
		{
		case ESurvivalRosterCategory::All:
			return NSLOCTEXT("SurvivalArmySelection", "TabAll", "ALL");
		case ESurvivalRosterCategory::Infantry:
			return NSLOCTEXT("SurvivalArmySelection", "TabInfantry", "INFANTRY");
		case ESurvivalRosterCategory::Vehicles:
			return NSLOCTEXT("SurvivalArmySelection", "TabVehicles", "VEHICLES");
		case ESurvivalRosterCategory::Nomadic:
			return NSLOCTEXT("SurvivalArmySelection", "TabNomadic", "TRUCKS");
		case ESurvivalRosterCategory::Aircraft:
			return NSLOCTEXT("SurvivalArmySelection", "TabAircraft", "AIRCRAFT");
		}
		return FText::GetEmpty();
	}

	/** @return Seconds as m:ss, or h:mm:ss from one hour on. */
	inline FText FormatClock(const float Seconds)
	{
		constexpr int32 SecondsPerMinute = 60;
		constexpr int32 SecondsPerHour = 3600;
		const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(Seconds));
		const int32 Hours = TotalSeconds / SecondsPerHour;
		const int32 Minutes = (TotalSeconds % SecondsPerHour) / SecondsPerMinute;
		const int32 Secs = TotalSeconds % SecondsPerMinute;
		if (Hours > 0)
		{
			return FText::FromString(FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, Secs));
		}
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Secs));
	}

	/**
	 * @brief Eight-way compass word for a wave entering at From and heading to To, for the incoming-wave banner when
	 * the edge zone has no name. World +X is north and +Y east, as the RTS camera looks at the map.
	 * @return "North", "North-East", ... ; empty when the points coincide.
	 */
	inline FText GetCompassDirectionText(const FVector& WaveEntryLocation, const FVector& BaseLocation)
	{
		const FVector2D Offset(WaveEntryLocation.X - BaseLocation.X, WaveEntryLocation.Y - BaseLocation.Y);
		if (Offset.IsNearlyZero())
		{
			return FText::GetEmpty();
		}
		constexpr int32 DirectionCount = 8;
		constexpr float DegreesPerDirection = 360.f / DirectionCount;
		// Atan2(Y, X): 0 degrees along +X (north), 90 along +Y (east).
		const float Degrees = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
		const int32 Index = (FMath::RoundToInt(Degrees / DegreesPerDirection) + DirectionCount) % DirectionCount;
		static const FText Directions[DirectionCount] = {
			NSLOCTEXT("SurvivalHud", "North", "North"), NSLOCTEXT("SurvivalHud", "NorthEast", "North-East"),
			NSLOCTEXT("SurvivalHud", "East", "East"), NSLOCTEXT("SurvivalHud", "SouthEast", "South-East"),
			NSLOCTEXT("SurvivalHud", "South", "South"), NSLOCTEXT("SurvivalHud", "SouthWest", "South-West"),
			NSLOCTEXT("SurvivalHud", "West", "West"), NSLOCTEXT("SurvivalHud", "NorthWest", "North-West")
		};
		return Directions[Index];
	}
}
