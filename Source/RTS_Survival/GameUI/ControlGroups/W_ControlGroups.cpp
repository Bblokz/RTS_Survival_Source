// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ControlGroups.h"

#include "ControlGroupImage/W_ControlGroupImage.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_ControlGroups::UpdateMostFrequentUnitInGroup(const int32 GroupIndex, const FTrainingOption TrainingOption)
{
	if (not M_ControlGroupWidgets.IsValidIndex(GroupIndex))
	{
		RTSFunctionLibrary::ReportError(
			"Invalid group index provided for the UW_ControlGroups widget! Expected a valid index, got " +
			FString::FromInt(GroupIndex) + " in function: UW_ControlGroups::UpdateMostFrequentUnitInGroup");
		return;
	}
	M_ControlGroupWidgets[GroupIndex]->OnUpdateControlGroup(TrainingOption);
}

void UW_ControlGroups::InitControlGroups(const TArray<UW_ControlGroupImage*> ControlGroupWidgets)
{
	if (ControlGroupWidgets.Num() != 10)
	{
		RTSFunctionLibrary::ReportError(
			"Not enough control groups provided for the UW_ControlGroups widget! Expected 10, got " +
			FString::FromInt(ControlGroupWidgets.Num()) + " in function: UW_ControlGroups::InitControlGroups");
		return;
	}
	M_ControlGroupWidgets = ControlGroupWidgets;
	for(int i = 0; i < M_ControlGroupWidgets.Num(); i++)
	{
		M_ControlGroupWidgets[i]->SetupControlGroupIndex(i);
	}
}
