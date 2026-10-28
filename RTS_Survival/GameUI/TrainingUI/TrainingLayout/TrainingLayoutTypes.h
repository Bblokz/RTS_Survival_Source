// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "TrainingLayoutTypes.generated.h"

class UTexture2D;

namespace TrainingLayoutConstants
{
	inline constexpr int32 ColumnsPerSubSection = 3;
	inline constexpr int32 RowsPerSubSection = 3;
	inline constexpr int32 SlotsPerSubSection = ColumnsPerSubSection * RowsPerSubSection;
	inline constexpr int32 MaxSubSectionsPerBuilding = 4;
}

/**
 * @brief One training slot of a sub section as a designer fills it in on the training layout data asset.
 * Only the subtype that belongs to the unit type is used; a unit type of none leaves the slot empty.
 */
USTRUCT(BlueprintType)
struct FTrainingLayoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	EAllUnitType UnitType = EAllUnitType::UNType_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout",
		meta = (EditCondition =
			"UnitType == EAllUnitType::UNType_Tank || UnitType == EAllUnitType::UNType_Harvester", EditConditionHides))
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout",
		meta = (EditCondition = "UnitType == EAllUnitType::UNType_Squad", EditConditionHides))
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout",
		meta = (EditCondition = "UnitType == EAllUnitType::UNType_Nomadic", EditConditionHides))
	ENomadicSubtype NomadicSubtype = ENomadicSubtype::Nomadic_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout",
		meta = (EditCondition = "UnitType == EAllUnitType::UNType_Aircraft", EditConditionHides))
	EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None;

	// A field defense: the player places it and the trainer's queue builds it at that spot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout",
		meta = (EditCondition = "UnitType == EAllUnitType::UNType_BuildingExpansion", EditConditionHides))
	EBuildingExpansionType FieldDefenseType = EBuildingExpansionType::BXT_Invalid;

	FTrainingOption ToTrainingOption() const;

	static FTrainingLayoutSlot Empty();
	static FTrainingLayoutSlot Tank(const ETankSubtype InTankSubtype);
	static FTrainingLayoutSlot Squad(const ESquadSubtype InSquadSubtype);
	static FTrainingLayoutSlot Nomadic(const ENomadicSubtype InNomadicSubtype);
	static FTrainingLayoutSlot Aircraft(const EAircraftSubtype InAircraftSubtype);
	static FTrainingLayoutSlot FieldDefense(const EBuildingExpansionType InFieldDefenseType);
};

/**
 * @brief A named group of training slots that the player picks with a sub section tab.
 * Slots fill the training grid from left to right and from top to bottom, three in a row.
 */
USTRUCT(BlueprintType)
struct FTrainingLayoutSubSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout", meta = (TitleProperty = "UnitType"))
	TArray<FTrainingLayoutSlot> Slots;
};

/**
 * @brief Everything one trainer building offers when the player has every unit of the faction unlocked.
 */
USTRUCT(BlueprintType)
struct FTrainingLayoutBuilding
{
	GENERATED_BODY()

	// Buildings like the HQ offer trucks and harvesters for which no unit cards exist; those turn this off
	// so that their options never depend on the cards in the player profile.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	bool bRequiresUnitCards = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout", meta = (TitleProperty = "DisplayName"))
	TArray<FTrainingLayoutSubSection> SubSections;
};

/**
 * @brief The training layouts of all trainer buildings of one player sub faction.
 */
USTRUCT(BlueprintType)
struct FTrainingLayoutFaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	TMap<ENomadicSubtype, FTrainingLayoutBuilding> Buildings;
};

/**
 * @brief One button of the quick navigation panel, which selects the player's first building of this type.
 */
USTRUCT(BlueprintType)
struct FTrainingQuickNavEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	ENomadicSubtype Building = ENomadicSubtype::Nomadic_None;

	// Leave empty to use the icon of the building's row in the training options data table.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	TSoftObjectPtr<UTexture2D> IconOverride;
};

/**
 * @brief A sub section as a trainer uses it at runtime: locked options are already emptied out.
 */
USTRUCT()
struct FTrainingSubSectionState
{
	GENERATED_BODY()

	UPROPERTY()
	FText DisplayName;

	// Always in slot order; an empty slot holds a training option of type none.
	UPROPERTY()
	TArray<FTrainingOption> Options;

	// Where this sub section starts in the trainer's flat array of training options. Queue items store the
	// flat index, so that the clock finds its widget whichever sub section is shown.
	UPROPERTY()
	int32 FirstFlatOptionIndex = 0;
};
