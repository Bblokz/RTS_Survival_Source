// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"

#include "WeaponTargetingData.generated.h"

class AActor;

/** 
 * @brief Explicit target mode used by FWeaponTargetingData.
 * Prevents mistaking "Ground" for "None" or "cleared target".
 */
UENUM()
enum class ETargetMode : uint8
{
	None   = 0,
	Actor  = 1,
	Ground = 2
};

/**
 * @brief Lightweight, cache-friendly targeting core shared by all weapons.
 * - Supports Actor/Ground/None via ETargetMode.
 * - Caches exactly 4 local aim offsets once when a new Actor target is set.
 * - Exposes a cached world-space ActiveTargetLocation for hot paths.
 * - Provides TickAimSelection() to rotate the selected aim point after N requests.
 * @note InitTargetStruct: call whenever/after owning player is known (or changes).
 * @note SetTargetActor / SetTargetGround: set the active target; struct caches offsets/world loc.
 */
USTRUCT()
struct FWeaponTargetingData
{
	GENERATED_BODY()

public:
	FWeaponTargetingData();

	/**
     * @brief Check if the given killed actor equals the current actor target.
     * @param KilledActor The actor that was reported as killed.
     * @return true if Mode==Actor and the current target actor equals KilledActor, false otherwise.
     */
    bool WasKilledActorCurrentTarget(const AActor* KilledActor) const;

	/** @brief Initialize or re-initialize with owning player index and (re)seed switching threshold. */
	void InitTargetStruct(const int32 OwningPlayer);

	/** @brief Sets the active target to an Actor. Caches 4 local aim offsets (or fallbacks). */
	void SetTargetActor(AActor* NewTarget);

	/** @brief Sets the active target to a ground world location (no actor). */
	void SetTargetGround(const FVector& GroundWorldLocation);

	/** @brief Clears target and resets state to None. */
	void ResetTarget();

	/**
	 * @brief Returns whether the current target is valid for engagement.
	 * @return true if Mode==Ground; or Actor visible & valid for owning player; false otherwise.
	 */
	bool GetIsTargetValid() const;

	/**
	 * @brief Mutating call used by hot paths to rotate the selected aim offset.
	 * Call once per “use” (e.g., before reading ActiveTargetLocation each tick/loop).
	 * Rotates to next aim offset when internal counter exceeds randomized threshold.
	 */
	void TickAimSelection();

	/** @return Cached world-space target location for aiming. */
	const FVector& GetActiveTargetLocation() const { return M_ActiveTargetLocation; }

	/** @return Raw actor pointer if Mode==Actor and still valid; otherwise nullptr. */
	AActor* GetTargetActor() const { return M_TargetActor.Get(); }

	/** @return Current explicit mode. */
	ETargetMode GetMode() const { return M_TargetMode; }

	/** @brief Optional deterministic debug draw of aim points and selection. */
	void DebugDraw(UWorld* World) const;

private:
	void LoadAimOffsetsFromActor(AActor* Target);
	void UseFallbackAimOffsets();
	void RecomputeActiveWorldLocation();
	inline void RecomputeActorTargetLocationWithCurrentAimOffset();
	void RerollSwitchThreshold();
	bool GetIsValidTargetActor() const;

	/** Owning player index used for visibility/validity checks. */
	UPROPERTY()
	int32 M_OwningPlayer = -1;

	/** Explicit target mode. */
	UPROPERTY()
	ETargetMode M_TargetMode = ETargetMode::None;

	/** Target actor (weak). Null if Mode != Actor. */
	UPROPERTY()
	TObjectPtr<AActor> M_TargetActor;

	/** Cached ground world location if Mode==Ground. */
	UPROPERTY()
	FVector M_GroundWorldLocation = FVector::ZeroVector;

	/** Cached world-space aim point for hot reads. */
	UPROPERTY()
	FVector M_ActiveTargetLocation = FVector::ZeroVector;

	// Four local-space aim offsets (cached once when setting Actor target).
	using FOffsetsArray = TArray<FVector, TInlineAllocator<4>>;
	FOffsetsArray M_LocalAimOffsets;

	// Fixed size & indices for 4 points
	static constexpr int32 NumAimPoints = 4;

	// Fallback local offsets (applied at actor origin). Tuned to feel natural on biped/vehicle.
	static const FVector M_FallbackOffsets[NumAimPoints];

	// Selection bookkeeping
	int32 M_SelectedIndex = 0;
	int32 M_SwitchCounter = 0;
	int32 M_TargetCallCountSwitchAim = 30; // randomized in InitTargetStruct()

	FVector FallBackOffsetInvalidIndex = FVector(0,0,150);

	// Overrun guard for very long sessions
	static constexpr int32 M_MaxCounterBeforeReset = 1000000;
};
