// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSCombatPingSettings.generated.h"

/**
 * @brief Project settings (Game > Combat Pings) for the ripple drawn on the minimap and tactical map where enemies
 * hit player units, plus the "Reinforcements" circle. URTSCombatPingSubsystem reads the detection values, UW_MiniMap
 * the look.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Combat Pings"))
class RTS_SURVIVAL_API URTSCombatPingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSCombatPingSettings();

	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	bool bEnableCombatPings = true;

	/**
	 * A hit only pings when the unit was not hit during the quiet time below, so a unit under steady fire pings
	 * once. Off: every hit on a player unit may ping, still throttled by the limits below.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	bool bOnlyPingWhenUnitComesUnderFire = true;

	/** Time a unit must go without hits before a new hit counts as coming under fire again. */
	UPROPERTY(Config, EditAnywhere, Category = "Detection",
		meta = (ClampMin = "0.0", Units = "s", EditCondition = "bOnlyPingWhenUnitComesUnderFire"))
	float UnitQuietSecondsBeforeNewPing = 6.f;

	/** No new ping within this distance (cm) of a recent ping, so one fight shows one ping. */
	UPROPERTY(Config, EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0", Units = "cm"))
	float AreaSuppressionRadius = 3000.f;

	/** How long a ping keeps its area quiet. */
	UPROPERTY(Config, EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0", Units = "s"))
	float AreaSuppressionSeconds = 15.f;

	/** Minimum time between any two pings, anywhere on the map. */
	UPROPERTY(Config, EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0", Units = "s"))
	float GlobalMinIntervalSeconds = 1.f;

	/** Pings visible at the same time; further hits are ignored until one fades. */
	UPROPERTY(Config, EditAnywhere, Category = "Detection", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxVisiblePings = 5;

	/** How long one ping stays on the map. */
	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "0.5", ClampMax = "15.0", Units = "s"))
	float PingLifetimeSeconds = 3.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Look")
	FLinearColor PingColor = FLinearColor(1.f, 0.16f, 0.08f, 1.f);

	/** Largest ripple radius on the small minimap, in UI units. */
	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "4.0", ClampMax = "128.0"))
	float MiniMapRadius = 20.f;

	/** Largest ripple radius on the tactical (full screen) map, in UI units. */
	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "4.0", ClampMax = "256.0"))
	float TacticalMapRadius = 38.f;

	/** Expanding rings that leave the centre one after another. */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Ripples", meta = (ClampMin = "0", ClampMax = "4"))
	int32 RippleCount = 2;

	/** Time one ring takes to expand to the full radius. */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Ripples", meta = (ClampMin = "0.2", ClampMax = "5.0", Units = "s"))
	float RipplePeriodSeconds = 1.2f;

	UPROPERTY(Config, EditAnywhere, Category = "Look|Ripples", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float RingThickness = 2.f;

	/** Four arcs that snap in around the spot (lock-on) and slowly turn. */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Brackets")
	bool bShowBrackets = true;

	UPROPERTY(Config, EditAnywhere, Category = "Look|Brackets", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float BracketSpinDegreesPerSecond = 45.f;

	/** Brightness of the soft disc behind the rings (0 hides it). */
	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlowOpacity = 0.22f;

	/** Circle with "Reinforcements" written in it, shown where player reinforcements arrive. */
	UPROPERTY(Config, EditAnywhere, Category = "Reinforcements")
	FLinearColor ReinforcementsColor = FLinearColor(0.25f, 0.85f, 1.f, 1.f);

	/** Smallest circle radius on the small minimap, in UI units; grows when the text needs more room. */
	UPROPERTY(Config, EditAnywhere, Category = "Reinforcements", meta = (ClampMin = "4.0", ClampMax = "128.0"))
	float ReinforcementsMiniMapRadius = 22.f;

	/** Smallest circle radius on the tactical (full screen) map, in UI units. */
	UPROPERTY(Config, EditAnywhere, Category = "Reinforcements", meta = (ClampMin = "4.0", ClampMax = "256.0"))
	float ReinforcementsTacticalMapRadius = 40.f;

	UPROPERTY(Config, EditAnywhere, Category = "Reinforcements", meta = (ClampMin = "4", ClampMax = "32"))
	int32 ReinforcementsMiniMapTextSize = 7;

	UPROPERTY(Config, EditAnywhere, Category = "Reinforcements", meta = (ClampMin = "4", ClampMax = "48"))
	int32 ReinforcementsTacticalMapTextSize = 11;

	UPROPERTY(Config, EditAnywhere, Category = "Reinforcements", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float ReinforcementsRingThickness = 2.f;
};
