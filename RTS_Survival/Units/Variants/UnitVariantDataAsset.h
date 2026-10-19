// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "UnitVariantTypes.h"

#include "UnitVariantDataAsset.generated.h"

class FDataValidationContext;
class UTexture2D;

/** One selectable variant as the later UI will show it. */
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
};

/** The variants one unit type has. Index 0 is always the base unit; the array length is the implemented count. */
USTRUCT(BlueprintType)
struct FUnitVariantEntry
{
	GENERATED_BODY()

	// Element 0 describes the base unit, 1 is VariantA, 2 is VariantB. Length 1 means "no choice to offer".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "DisplayName"))
	TArray<FUnitVariantDefinition> Variants;
};

/**
 * @brief Make one asset of this class and pick it in Project Settings > Unit Variants. It lists, per German unit
 * subtype, which variants are implemented in that unit's Blueprint; the variant UI only offers a choice for
 * entries with more than one variant and the runtime clamps saved choices to what is listed here.
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

	/** @return Every unit with more than one implemented variant, for the later UI. */
	TArray<FTrainingOption> GetUnitsWithChoice() const;

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
#endif

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Tanks", meta = (AllowPrivateAccess = "true"))
	TMap<ETankSubtype, FUnitVariantEntry> M_TankVariants;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Squads", meta = (AllowPrivateAccess = "true"))
	TMap<ESquadSubtype, FUnitVariantEntry> M_SquadVariants;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Building Expansions", meta = (AllowPrivateAccess = "true"))
	TMap<EBuildingExpansionType, FUnitVariantEntry> M_BuildingExpansionVariants;
};
