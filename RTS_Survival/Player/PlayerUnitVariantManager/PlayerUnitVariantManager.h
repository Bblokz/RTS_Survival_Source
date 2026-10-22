// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"

#include "PlayerUnitVariantManager.generated.h"

class ARTSAsyncSpawner;
class UUnitVariantDataAsset;

/**
 * @brief Owned by the player controller; stores the variant the player selected per unit type for this mission.
 * The profile loader calls InitFromProfile then ApplySelectionsToSpawner before starting units spawn; from then on
 * the async spawner spawns the variant classes and SetSelectedVariant keeps the spawner in sync.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerUnitVariantManager : public UActorComponent
{
	GENERATED_BODY()

	// Automation tests inject a data asset without loading project settings.
	friend struct FUnitVariantTestAccess;

public:
	UPlayerUnitVariantManager();

	/**
	 * @brief Replaces the selection with the saved one plus the asset's debug forced list, drops entries for
	 * unsupported unit types and clamps every variant the data asset does not implement to Base, reporting each.
	 * @param SavedSelections The selections stored in the player profile.
	 */
	void InitFromProfile(const TArray<FUnitVariantSelection>& SavedSelections);

	/**
	 * @brief Pushes the stored selection into the spawner's class maps and remembers the spawner so later
	 * SetSelectedVariant calls (cheats, future UI) reach it too.
	 * @param AsyncSpawner The mission's spawner; must already hold its base mappings and the director overrides.
	 */
	void ApplySelectionsToSpawner(ARTSAsyncSpawner* AsyncSpawner);

	/** @return The selected variant for the unit type, Base when nothing was selected. */
	EUnitVariant GetSelectedVariant(const FTrainingOption& Unit) const;

	/**
	 * @brief Changes one selection at runtime (cheat manager, future UI). Clamps like InitFromProfile and, once a
	 * spawner is known, swaps its class so units requested afterwards are the new variant.
	 * @param Unit The unit type the choice is for.
	 * @param Variant The variant to select; Base removes the stored choice and restores the base class.
	 * @return False when the unit type is unsupported, the variant is not implemented or has no class.
	 */
	bool SetSelectedVariant(const FTrainingOption& Unit, const EUnitVariant Variant);

	/** @return What the training menu shows for the unit; all overrides off while the player keeps Base. */
	FUnitVariantTrainingPresentation GetTrainingPresentation(const FTrainingOption& Unit) const;

	TArray<FUnitVariantSelection> GetSelectionsForSave() const;

	/** @return The asset picked in project settings; never null because the settings fall back to class defaults. */
	const UUnitVariantDataAsset* GetVariantDataAsset() const;

	static bool GetIsUnitTypeSupported(const EAllUnitType UnitType);

	/** @return The key the spawner uses for the unit: the training menu lists harvesters as UNType_Harvester. */
	static FTrainingOption GetSpawnerKey(const FTrainingOption& Unit);

protected:
	virtual void BeginPlay() override;

private:
	void BeginPlay_LoadVariantDataAsset();

	// Debug list on the data asset; clamped like saved selections and logged so a forgotten entry is visible.
	void ApplyDebugForcedVariants();

	/**
	 * @brief Stores one selection when the asset implements it, otherwise reports it once and keeps Base.
	 * @param Selection The unit and variant to store.
	 * @return False when the unit type is unsupported or the variant was clamped to Base.
	 */
	bool StoreSelectionClamped(const FUnitVariantSelection& Selection);

	// Caches the asset for the non-const paths; the const getter loads without caching.
	void CacheVariantDataAsset();
	bool GetIsValidVariantDataAsset() const;

	// No report: outside a mission (menus, automation tests) selections are only stored.
	bool GetIsValidAsyncSpawnerNoReport() const;

	// Same declaration shape as UTrainingLayoutSubsystem::M_TrainingLayout. Also the strong owner that keeps the
	// asset's training brushes alive while the training menu shows them.
	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	// Only units with a non-Base choice are stored, keyed as the spawner keys them; a missing key means Base.
	TMap<FTrainingOption, EUnitVariant> M_SelectedVariants;

	UPROPERTY()
	TWeakObjectPtr<ARTSAsyncSpawner> M_AsyncSpawner;
};
