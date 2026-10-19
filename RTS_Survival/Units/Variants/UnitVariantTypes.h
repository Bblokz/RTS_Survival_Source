// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "UnitVariantTypes.generated.h"

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
