// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ControlGroups.h"

#include "ControlGroupImage/W_ControlGroupImage.h"
#include "Engine/LocalPlayer.h"
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
	constexpr int32 ExpectedControlGroupCount = 10;
	if (ControlGroupWidgets.Num() != ExpectedControlGroupCount)
	{
		RTSFunctionLibrary::ReportError(
			"Not enough control groups provided for the UW_ControlGroups widget! Expected " +
			FString::FromInt(ExpectedControlGroupCount) + ", got " + FString::FromInt(ControlGroupWidgets.Num()) +
			" in function: UW_ControlGroups::InitControlGroups");
		return;
	}

	ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();
	if (not IsValid(OwningLocalPlayer))
	{
		RTSFunctionLibrary::ReportError("UW_ControlGroups could not resolve an owning local player.");
		return;
	}

	M_ControlGroupWidgets = ControlGroupWidgets;
	for (int32 GroupIndex = 0; GroupIndex < M_ControlGroupWidgets.Num(); ++GroupIndex)
	{
		if (not IsValid(M_ControlGroupWidgets[GroupIndex]))
		{
			RTSFunctionLibrary::ReportError(
				"UW_ControlGroups found a null control group widget at index " + FString::FromInt(GroupIndex));
			continue;
		}

		M_ControlGroupWidgets[GroupIndex]->InitControlGroupImage(OwningLocalPlayer, GroupIndex);
	}
}
