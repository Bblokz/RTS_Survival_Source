#pragma once


#include "MissionWidgetState.generated.h"

UENUM(BlueprintType)
enum class EMissionWidgetState : uint8
{
	OMS_None UMETA(DisplayName = "None"),
	OMS_Minimized UMETA(DisplayName = "Minimized"),
	OMS_Expanded UMETA(DisplayName = "Expanded"),
	// Has a next button instead of a minimze button, clicking will result in the next mission.
	OMS_ExpandedNext UMETA(DisplayName = "ExpandedNext"),
};

UENUM(BlueprintType)
enum class EMissionTextSpeed : uint8
{
	TS_None,
	TS_2Seconds,
	TS_3Seconds,
};

UENUM(BlueprintType)
enum class EMissionWidgetNextButton :uint8
{
	Btn_Next,
	Btn_OK
};

/**
 * @brief How the mission panel presents a mission: major objectives must be done, bonus objectives are optional.
 * Major is entry 0 so every mission saved before this existed loads as major.
 */
UENUM(BlueprintType)
enum class EMissionObjectiveType : uint8
{
	Major UMETA(DisplayName = "Major Objective"),
	Bonus UMETA(DisplayName = "Bonus Objective"),
};

USTRUCT(BlueprintType)
struct FMissionWidgetState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText MissionTitle = FText::FromString("Mission Title");

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText MissionDescription = FText::FromString("Mission Description");

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionTextSpeed TextSpeed = EMissionTextSpeed::TS_None;

	// The button used in the expanded version of the widget that will complete the current mission when clicked.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionWidgetNextButton NextButtonType = EMissionWidgetNextButton::Btn_Next;

	// Blue shield + star for major objectives, green shield + diamond for optional bonus objectives.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionObjectiveType ObjectiveType = EMissionObjectiveType::Major;
};
