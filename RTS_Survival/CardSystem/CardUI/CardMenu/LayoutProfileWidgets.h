#pragma once

#include "CoreMinimal.h"


UENUM(BlueprintType)
enum class ELayoutProfileWidgets : uint8
{
	Widgets_None,
	Widgets_StartUnits,
	Widgets_TechAndResource,
	Widgets_BarracksTrain,
	Widgets_MechanisedDepotTrain,
	Widgets_ForgeTrain,
	Widgets_T2FactoryTrain,
	Widgets_AirbaseTrain,
	Widgets_ExperimentalFactoryTrain,
};