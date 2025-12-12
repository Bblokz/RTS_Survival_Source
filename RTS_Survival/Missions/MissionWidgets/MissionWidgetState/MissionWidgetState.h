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
};
