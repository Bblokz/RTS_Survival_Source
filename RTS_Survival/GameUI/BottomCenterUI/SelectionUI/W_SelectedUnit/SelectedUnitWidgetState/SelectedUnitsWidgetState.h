#pragma once


#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "SelectedUnitsWidgetState.generated.h"

UENUM()
enum class ESelectedWidgetType : uint8
{
	// This widget represents a selected unit of which the type is not primary selected.
	NotPrimary,
	// the primary selected unit has the same Type as the widget representing this unit, but it is not this widget
	// that corresponds to the primary selected unit.
	PrimarySameType,
	// This widget corresponds to the (unique) primary selected unit that drives the UI.
	// There may be more units that are of the same type as the primary selected units and these are marked as PrimarySameType.
	PrimarySelected
};

USTRUCT(Blueprintable)
struct FSelectedUnitsWidgetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FTrainingOption UnitID;

	/** Index into the *player selection arrays*  */
	UPROPERTY(BlueprintReadOnly)
	int32 SelectionArrayIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	ESelectedWidgetType WidgetType = ESelectedWidgetType::NotPrimary;
};
