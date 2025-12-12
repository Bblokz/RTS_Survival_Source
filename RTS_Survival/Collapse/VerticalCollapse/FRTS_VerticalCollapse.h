// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSVerticalCollapseSettings.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"

#include "FRTS_VerticalCollapse.generated.h"

class AActor;

/**
 * @brief Static utility to sink an Actor vertically into the ground over time, with optional yaw and pitch/roll tilt and FX.
 * Mirrors FRTS_Collapse style: weak owner, shared USTRUCT context, static timer ticks, early-outs.
 */
class RTS_SURVIVAL_API FRTS_VerticalCollapse
{
public:
	/**
	 * @brief Starts a vertical collapse on an actor: Z moves down by Settings.DeltaZ over Settings.CollapseTime.
	 * Optionally rotates towards random yaw and/or pitch/roll (single magnitude controlling both) if enabled.
	 * Plays FX at start.
	 * @param CollapseOwner The actor to collapse (kept as a weak pointer internally).
	 * @param Settings Designer-provided collapse settings.
	 * @param CollapseFX SFX/VFX definition (reuses FCollapseFX).
	 * @param OnFinished Optional completion callback (called only if owner is still valid and destruction is NOT requested).
	 */
	static void StartVerticalCollapse(
		AActor* CollapseOwner,
		const FRTSVerticalCollapseSettings& Settings,
		const FCollapseFX& CollapseFX,
		TFunction<void()>&& OnFinished = TFunction<void()>{});

	

private:
	/** One-time start after validation: cache start/target and kick off timer. */
	static void HandleStart(struct FVerticalCollapseTaskContext& MutableContext);

	/** Per-tick updater: lerps Z and rotation; finishes at alpha=1. */
	static void TickVerticalCollapse(TSharedPtr<struct FVerticalCollapseTaskContext> Context);

	/** Finalization: ensure last transform is applied; clear timer and exit; destroy/callback as needed. */
	static void FinishVerticalCollapse(TSharedPtr<struct FVerticalCollapseTaskContext> Context);

	// Validators (log via RTSFunctionLibrary::ReportError).
	static bool GetIsValidOwner(const TWeakObjectPtr<AActor>& WeakOwner, const TCHAR* Where);
	static bool EnsureValidContext(const TSharedPtr<struct FVerticalCollapseTaskContext>& Context, const TCHAR* Where);

	// Local FX spawners (cannot call FRTS_Collapse’s private helpers).
	static void SpawnCollapseVFX(const FCollapseFX& FX, UWorld* World, const FVector& BaseLocationFX);
	static void SpawnCollapseSFX(const FCollapseFX& FX, UWorld* World, const FVector& BaseLocationFX);
};


/**
 * @brief State shared across timer ticks; mirrors FRTS_Collapse’s context approach.
 */
USTRUCT()
struct FVerticalCollapseTaskContext
{
	GENERATED_BODY()

	// Weak tracking for safety.
	TWeakObjectPtr<AActor> WeakOwner;

	// Immutable inputs (copied for safety).
	UPROPERTY() FRTSVerticalCollapseSettings Settings;
	UPROPERTY() FCollapseFX CollapseFX;

	// Completion callback (copyable). Must not assume owner stays alive.
	TFunction<void()> OnFinished;

	// Cached transforms.
	FVector  StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;

	FVector  TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;

	// Progress.
	float ElapsedSeconds = 0.0f;

	// Timer handle so we can clear it cleanly on finish.
	FTimerHandle TimerHandle;

	FVerticalCollapseTaskContext() = default;

	FVerticalCollapseTaskContext(
		AActor* InOwner,
		const FRTSVerticalCollapseSettings& InSettings,
		const FCollapseFX& InFX,
		TFunction<void()>&& InOnFinished)
		: WeakOwner(InOwner)
		, Settings(InSettings)
		, CollapseFX(InFX)
		, OnFinished(MoveTemp(InOnFinished))
	{
	}
};
