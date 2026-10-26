// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "UnitVariantTypes.h"

#include "UnitVariantDataAsset.generated.h"

class FDataValidationContext;
class USlateBrushAsset;
class UStaticMesh;
class UTexture2D;

/** One selectable variant: what the spawner spawns for it, what the later UI shows and what the training menu shows. */
USTRUCT(BlueprintType)
struct FUnitVariantDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	// The Blueprint child class spawned for this variant. Must stay null on element 0 (Base): the base class is the
	// one the spawner's own setup holds, so the asset never duplicates it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
	TSoftClassPtr<AActor> UnitClass;

	// Building expansions only: the placement preview mesh of this variant. Null keeps the base preview mesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
	TSoftObjectPtr<UStaticMesh> BxpPreviewMesh;

	// Training UI: one opt-in override per DT_TrainingOptions column (row struct BP_FTrainingUI_OptionSlate).
	// Off shows the base unit's DT_TrainingOptions row value. DisplayName / Description / Icon above are for the
	// later variant selection UI and are not used by the training menu.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingImageBrush = false;

	// Hard reference (not soft): it loads with the data asset and stays alive as long as the asset does, exactly
	// like the brushes the DT_TrainingOptions rows hold, so GC never collects a brush the training menu shows.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingImageBrush"))
	TObjectPtr<USlateBrushAsset> TrainingImageBrush = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingTitle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI", meta = (EditCondition = "bOverrideTrainingTitle"))
	FText TrainingTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingDescription = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingDescription", MultiLine = "true"))
	FText TrainingDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingTooltip = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingTooltip", MultiLine = "true"))
	FText TrainingTooltip;
};

/** The variants one unit type has. Index 0 is always the base unit; the array length is the implemented count. */
USTRUCT(BlueprintType)
struct FUnitVariantEntry
{
	GENERATED_BODY()

	// Element 0 describes the base unit, 1 is VariantA, 2 is VariantB, 3 is VariantC. Length 1 means "no choice to
	// offer".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "DisplayName"))
	TArray<FUnitVariantDefinition> Variants;
};

/**
 * @brief Make one asset of this class and pick it in Project Settings > Unit Variants; it holds all variant data.
 * Per German unit subtype it lists the variant child classes the async spawner swaps in, the texts for the later
 * selection UI and the training menu overrides. Saved and forced selections are clamped to what is listed here.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UUnitVariantDataAsset : public UDataAsset
{
	GENERATED_BODY()

	// Automation tests fill the private maps without an editor-only setter.
	friend struct FUnitVariantTestAccess;

public:
	/** @return 1 when the unit has no entry (base only), otherwise the entry's array length clamped to 1..3. */
	int32 GetImplementedVariantCount(const FTrainingOption& Unit) const;

	/** @return True when Variant's index is below the implemented count of Unit. Base is always implemented. */
	bool GetIsVariantImplemented(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/** @return Null when the unit has no entry or the variant is not implemented. */
	const FUnitVariantDefinition* FindVariantDefinition(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/** @return The class of a non-base variant, or a null soft pointer for Base, a missing entry or a missing class. */
	TSoftClassPtr<AActor> FindVariantClass(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/** @return The preview mesh of a building expansion variant, null when the variant keeps the base preview. */
	TSoftObjectPtr<UStaticMesh> FindBxpVariantPreviewMesh(const EBuildingExpansionType BxpType,
	                                                      const EUnitVariant Variant) const;

	/**
	 * @brief What the training menu shows for this unit while Variant is selected; Variant is always echoed so a
	 * Blueprint can show a variant badge even without overrides.
	 * @param Unit The unit type, keyed as the spawner keys it (harvesters as tanks).
	 * @param Variant The selected variant.
	 * @return All overrides off for Base, a missing entry or an unimplemented variant.
	 */
	FUnitVariantTrainingPresentation GetTrainingPresentation(const FTrainingOption& Unit,
	                                                         const EUnitVariant Variant) const;

	/** @return Every unit with more than one implemented variant, for the later UI. */
	TArray<FTrainingOption> GetUnitsWithChoice() const;

	/** @return The debug forced selections; always empty in Shipping builds. */
	const TArray<FUnitVariantSelection>& GetDebugForcedVariants() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	const FUnitVariantEntry* FindEntry(const FTrainingOption& Unit) const;

#if WITH_EDITOR
	/**
	 * @brief Shared rules for the three maps so a broken entry reads the same whatever unit kind it is.
	 * @param KeyEnum The UEnum of the map key, used for names and the Russian / invalid key checks.
	 * @param KeyValue The numeric key of the entry.
	 * @param bIsInvalidKey True when the key is the _None / BXT_Invalid enumerator.
	 * @param Entry The entry to validate.
	 * @param Context Receives one error per broken rule.
	 * @return False when any rule is broken.
	 */
	static bool ValidateEntry(const UEnum* KeyEnum, const int64 KeyValue, const bool bIsInvalidKey,
	                          const FUnitVariantEntry& Entry, FDataValidationContext& Context);

	static bool GetIsRussianKey(const UEnum* KeyEnum, const int64 KeyValue);

	// A class spawned for two definitions would make two selections indistinguishable on the spawned unit.
	bool ValidateNoClassUsedTwice(FDataValidationContext& Context) const;
#endif

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Tanks", meta = (AllowPrivateAccess = "true"))
	TMap<ETankSubtype, FUnitVariantEntry> M_TankVariants;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Squads", meta = (AllowPrivateAccess = "true"))
	TMap<ESquadSubtype, FUnitVariantEntry> M_SquadVariants;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Building Expansions", meta = (AllowPrivateAccess = "true"))
	TMap<EBuildingExpansionType, FUnitVariantEntry> M_BuildingExpansionVariants;

	// Development aid until the selection UI exists: applied over the saved selection when the manager
	// initialises, clamped exactly like saved selections. Ignored in Shipping builds. Clear it before committing.
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess = "true", TitleProperty = "Unit"))
	TArray<FUnitVariantSelection> M_DebugForcedVariants;
};
