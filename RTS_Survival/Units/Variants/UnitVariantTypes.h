// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "UnitVariantTypes.generated.h"

class USlateBrushAsset;

// Shared by every unit on purpose: a Blueprint switch on this enum gives exactly three pins. What VariantA means
// differs per unit; the per-unit display name comes from UUnitVariantDataAsset.
UENUM(BlueprintType)
enum class EUnitVariant : uint8
{
	Base UMETA(DisplayName = "Base"),
	VariantA UMETA(DisplayName = "Variant A"),
	VariantB UMETA(DisplayName = "Variant B"),
};

namespace UnitVariants
{
	// Base plus two alternatives; the enum, the data asset validation and the UI all rely on this cap.
	constexpr int32 MaxVariantsPerUnit = 3;
}

static inline FString Global_GetUnitVariantAsString(const EUnitVariant Variant)
{
	switch (Variant)
	{
	case EUnitVariant::Base:
		return "Base";
	case EUnitVariant::VariantA:
		return "VariantA";
	case EUnitVariant::VariantB:
		return "VariantB";
	}
	return "Unknown";
}

/** The saved variant choice for one unit type; stored in the player profile next to the cards. */
USTRUCT(BlueprintType)
struct FUnitVariantSelection
{
	GENERATED_BODY()

	// Which unit this choice is for. Only Tank, Squad and BuildingExpansion unit types are valid keys.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FTrainingOption Unit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EUnitVariant Variant = EUnitVariant::Base;
};

/**
 * What the training UI shows instead of the base unit's DT_TrainingOptions row while a variant is selected.
 * Mirrors the row struct BP_FTrainingUI_OptionSlate (ImageBrush, Title, Description, Tooltip), one opt-in bool each.
 */
USTRUCT(BlueprintType)
struct FUnitVariantTrainingPresentation
{
	GENERATED_BODY()

	// The selected variant; Base when the player kept the base unit, and then every override below is off.
	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	EUnitVariant Variant = EUnitVariant::Base;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideImageBrush = false;

	// Hard reference on purpose, same type as the DT_TrainingOptions ImageBrush column. Being a UPROPERTY keeps the
	// brush asset (and through it its texture) reachable for GC from every struct copy that is itself a UPROPERTY,
	// such as UW_TrainingItem::M_TrainingItemState. Never turn this into a soft pointer or a by-value FSlateBrush.
	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	TObjectPtr<USlateBrushAsset> ImageBrush = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideTitle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideDescription = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideTooltip = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Tooltip;
};
