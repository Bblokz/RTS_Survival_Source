// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSCombatPingSubsystem.generated.h"

class URTSCombatPingSettings;
class URTSComponent;

struct FRTSCombatPing
{
	FVector M_WorldLocation = FVector::ZeroVector;
	// Real time, so pings last as long on screen at any game speed.
	double M_SpawnRealTimeSeconds = 0.0;
};

/**
 * @brief Collects hits on player units and turns them into a few map pings: hits near a recent ping, too soon after
 * the last one or beyond the visible cap are dropped. UHealthComponent reports hits; UW_MiniMap draws the pings.
 */
UCLASS()
class RTS_SURVIVAL_API URTSCombatPingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Called on every hit a unit takes; only player units under the throttle limits create a ping. */
	void ReportUnitHit(const URTSComponent& HitUnitRTSComponent);

	/** @return True if a ping was created; false when the throttle dropped it. */
	bool TryAddPing(const FVector& WorldLocation);

	/**
	 * @brief Review tooling (UI snapshots): places a ping past the throttle so a capture shows a chosen moment.
	 * @param WorldLocation Where the ping shows.
	 * @param AgeNowSeconds Age the ping has right now; negative starts it later.
	 */
	void AddPreviewPing(const FVector& WorldLocation, const float AgeNowSeconds);

	// Holds pings still visible and ones that only keep their area quiet; check the age before drawing.
	const TArray<FRTSCombatPing>& GetPings() const { return M_Pings; }

	double GetRealTimeSeconds() const;

private:
	TArray<FRTSCombatPing> M_Pings;

	double M_LastPingRealTimeSeconds = -DBL_MAX;

	// Real time of each player unit's last hit. Kept apart from the unit's in-combat flag, which firing also sets,
	// so a unit that shoots back still pings when it comes under fire.
	TMap<TWeakObjectPtr<const URTSComponent>, double> M_LastHitSecondsPerUnit;

	bool GetWasUnitHitRecently(const URTSComponent& HitUnitRTSComponent, const double NowSeconds,
	                           const URTSCombatPingSettings& Settings) const;

	void RemoveStaleUnitHitRecords(const double NowSeconds, const URTSCombatPingSettings& Settings);

	void RemoveExpiredPings(const double NowSeconds, const URTSCombatPingSettings& Settings);

	bool GetIsAreaSuppressed(const FVector& WorldLocation, const double NowSeconds,
	                         const URTSCombatPingSettings& Settings) const;

	int32 GetVisiblePingCount(const double NowSeconds, const URTSCombatPingSettings& Settings) const;
};
