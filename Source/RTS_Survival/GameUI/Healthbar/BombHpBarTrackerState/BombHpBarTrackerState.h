// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "BombHpBarTrackerState.generated.h"

class UMaterialInterface;
class UWidgetComponent;
class AActor;
class UBombComponent;

/**
 * @brief Initialisation settings for the Bomb UI tracker.
 *        Provides the icon material and UV slice ratio for the bomb icon.
 */
USTRUCT(BlueprintType)
struct FBombTrackerInitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalSliceUVRatio = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* AmmoIconMaterial = nullptr;

	/** @return true if values look usable; errors are reported internally when invalid. */
	bool EnsureIsValid() const;
};

/**
 * @brief Tracks an aircraft's bombs (via UBombComponent) and binds a UW_AmmoHealthBar on an actor.
 *        Binding is order-independent: set the widget actor or bomb component in any order.
 *
 * @note SetActorWithAmmoWidget: call when you know which actor carries the WidgetComponents.
 * @note SetBombComponent: call when the UBombComponent becomes available.
 * @note VerifyTrackingActive: optional diagnostic; re-attempts binding if lost.
 */
USTRUCT()
struct FBombHpBarTrackerState
{
	GENERATED_BODY()

	/** Enables/disables this tracker’s logic from the outside if needed. */
	bool bIsUsingBombAmmoTracker = false;

	/** Must be filled before or shortly after BeginPlay (order doesn’t matter). */
	FBombTrackerInitSettings BombTrackerInitSettings;

	/**
	 * @brief Provide the bomb component to track.
	 * @param InBombComponent The UBombComponent whose OnMagConsumed delegate drives the UI.
	 */
	void SetBombComponent(UBombComponent* InBombComponent);

	/**
	 * @brief Provide the actor that owns one or more WidgetComponents hosting UW_AmmoHealthBar.
	 * @param HPAmmoWidgetActor Actor that contains WidgetComponents with the ammo/bomb widget.
	 */
	void SetActorWithAmmoWidget(AActor* HPAmmoWidgetActor);

	/**
	 * @brief Verifies delegate binding is still alive; if not, attempts to re-bind the widget.
	 *        Useful for dynamically spawned actors/components.
	 */
	void VerifyTrackingActive();

	/** Optional periodic check; unused by default but kept for parity with the weapon tracker. */
	FTimerHandle VerifyBindingTimer;
	float VerifyBindingDelay = 3.0f;

private:
	// ——— Validity helpers (report errors + keep code readable) ———
	bool EnsureActorToCheckIsValid() const;
	bool EnsureTrackBombComponentIsValid() const;

	// Called once both the bomb component and the actor are set.
	void SetupTrackingLogic();

	/**
	 * @brief Try to locate/build the widget on a widget component and bind the bomb tracker.
	 * @param WidgetComp Candidate widget component (may be invalid).
	 * @return true if binding succeeded immediately, false otherwise (may schedule a deferred bind).
	 */
	bool TryBindAmmoBarOnWidgetComponent(UWidgetComponent* WidgetComp);

	/** @brief Schedule a one-shot retry on next tick to bind once the widget exists/built. */
	void ScheduleDeferredAmmoBindNextTick();

	void Debug(const FString& DebugMessage, const FColor Color) const;

	// ——— State ———

	// Actor on which we search for UWidgetComponent(s) that host UW_AmmoHealthBar.
	TWeakObjectPtr<AActor> M_ActorToCheckForAmmoHpBarWidget = nullptr;

	// The bomb component we track (source of OnMagConsumed).
	TWeakObjectPtr<UBombComponent> M_TrackBombComponent = nullptr;
};
