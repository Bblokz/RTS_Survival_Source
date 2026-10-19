// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"

#include "PlayerUnitVariantManager.generated.h"

class URTSComponent;
class UUnitVariantDataAsset;

/**
 * @brief Owned by the player controller; holds the variant the player selected per unit type for this mission and
 * answers units that pull their variant when they begin play. Initialised from the profile before starting units spawn.
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
	 * @brief Replaces the selection with the saved one, drops entries for unsupported unit types and clamps every
	 * variant the data asset does not implement to Base, reporting each clamp once.
	 * @param SavedSelections The selections stored in the player profile.
	 */
	void InitFromProfile(const TArray<FUnitVariantSelection>& SavedSelections);

	/** @return The selected variant for the unit type, Base when nothing was selected or the asset is missing. */
	EUnitVariant GetSelectedVariant(const FTrainingOption& Unit) const;

	/**
	 * @brief The one entry point units use. Returns Base for every owner except the player and for unit types
	 * without variant support, so callers need no owner check of their own.
	 * @param RTSComponent The unit's RTS component with owner, unit type and subtype already set.
	 * @return The variant the unit must apply.
	 */
	EUnitVariant ResolveVariantForUnit(const URTSComponent* RTSComponent) const;

	/**
	 * @brief Changes one selection at runtime (cheat manager, future UI). Clamps like InitFromProfile; selecting
	 * Base removes the stored choice.
	 * @param Unit The unit type the choice is for.
	 * @param Variant The variant to select.
	 * @return False when the unit type is unsupported or the variant is not implemented.
	 */
	bool SetSelectedVariant(const FTrainingOption& Unit, const EUnitVariant Variant);

	TArray<FUnitVariantSelection> GetSelectionsForSave() const;

	/** @return The asset picked in project settings; never null because the settings fall back to class defaults. */
	const UUnitVariantDataAsset* GetVariantDataAsset() const;

	static bool GetIsUnitTypeSupported(const EAllUnitType UnitType);

protected:
	virtual void BeginPlay() override;

private:
	void BeginPlay_LoadVariantDataAsset();

	// Debug list from project settings; bypasses the asset clamp so unlisted Blueprint variants can be tested.
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

	// Same declaration shape as UTrainingLayoutSubsystem::M_TrainingLayout.
	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	// Only units with a non-Base choice are stored; a missing key means Base.
	TMap<FTrainingOption, EUnitVariant> M_SelectedVariants;
};
