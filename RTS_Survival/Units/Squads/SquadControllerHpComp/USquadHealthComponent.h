// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "USquadHealthComponent.generated.h"

class USlateBrushAsset;
class USquadUnitHealthComponent;

/**
 * @brief Aggregate health component that represents the total HP of a squad.
 * Tracks per-member snapshots and mirrors their sum as this component's Max/Current HP.
 *
 * @note This component does not receive engine damage; squad members forward deltas here.
 * @note Call flows:
 *       - Units call SquadTakeDamage / SquadHeal for raw deltas.
 *       - Units call OnUnitStateChanged to refresh their snapshot (with or without applying the delta again).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API USquadHealthComponent : public UHealthComponent
{
	GENERATED_BODY()

public:
	USquadHealthComponent() = default;

	// ---- existing public API ----
	void SquadTakeDamage(float Damage);
	void SquadHeal(float HealAmount);

	void UpdateSquadWeaponIcon(USlateBrushAsset* NewWeaponIconAsset);

	/**
	 * @brief Update a member's snapshot and adjust aggregates by the delta.
	 * @param UnitHealth           The unit HP component that changed (key).
	 * @param NewUnitMax           New Max HP of that unit.
	 * @param NewUnitCurrent       New Current HP of that unit.
	 * @param bAffectSquadCurrent  If true, apply the Current delta to the squad too.
	 * @param bAffectSquadMax      If true, the unit's max delta affects the full-squad max.
	 *                             Set false for reinforcement that refills an already-accounted slot.
	 */
	void OnUnitStateChanged(USquadUnitHealthComponent* UnitHealth,
	                        float NewUnitMax,
	                        float NewUnitCurrent,
	                        bool bAffectSquadCurrent,
	                        bool bAffectSquadMax = true);

	/**
	 * @brief Removes a member from aggregation and subtracts only its current HP.
	 * Max HP stays constant (full-squad max).
	 */
	void UnregisterUnit(USquadUnitHealthComponent* UnitHealth);

	/** @brief Clears all per-unit snapshots AND resets the cached full-squad max. */
	void ResetUnitSnapshots();

	/** @brief Seeds/overwrites a unit snapshot without applying deltas. */
	void SeedUnitSnapshot(USquadUnitHealthComponent* UnitHealth,
	                      float UnitMax,
	                      float UnitCurrent);

	/**
	 * @brief Initializes the max HP of a *full* squad (constant while units die).
	 * Call once after seeding all initial members.
	 */
	void InitializeFullSquadMax(float FullMaxHealth);

	/** @return Max HP of a full squad. */
	float GetFullSquadMaxHealth() const { return M_FullSquadMaxHealth; }

	// Enforce constant max while full-squad max is initialized.
	virtual void SetMaxHealth(const float NewMaxHealth) override;

protected:
	UPROPERTY()
	TMap<TWeakObjectPtr<USquadUnitHealthComponent>, FVector2D> M_UnitToMaxCur;

	virtual void OnWidgetInitialized() override;

private:
	// Cached max of a full squad. Does not decrease on deaths.
	UPROPERTY()
	float M_FullSquadMaxHealth = 0.f;

	bool bM_OnWidgetInitLoadBrushAsset = false;
	UPROPERTY()
	USlateBrushAsset* M_SquadWeaponIconAssetToSet = nullptr;

	UPROPERTY()
	bool bM_HasFullMaxInitialized = false;

	void ApplyDeltaToTotals(float DeltaMax, float DeltaCur, bool bApplyCur, bool bApplyMax);
	void ApplyDeltaToCurrentOnly(float DeltaCur);
	void SetFullSquadMax_Internal(float NewFullMax);
};
