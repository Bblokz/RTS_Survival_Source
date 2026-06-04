#include "W_MissionWidgetManager.h"

#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UW_Mission* UW_MissionWidgetManager::GetFreeMissionWidget()
{
	for (UW_Mission* EachWidget : M_MissionWidgets)
	{
		if (not EnsureMissionWidgetIsValid(EachWidget))
		{
			continue;
		}

		if (EachWidget->GetIsFreeWidget())
		{
			return EachWidget;
		}
	}
	return nullptr;
}

void UW_MissionWidgetManager::InitMissionWidgetReferences(const TArray<UW_Mission*> MissionWidgets)
{
	M_MissionWidgets = MissionWidgets;
	for (UW_Mission* EachWidget : M_MissionWidgets)
	{
		if (not EnsureMissionWidgetIsValid(EachWidget))
		{
			continue;
		}

		EachWidget->MarkWidgetAsFree();
	}
}

bool UW_MissionWidgetManager::EnsureMissionWidgetIsValid(const UW_Mission* MissionWidget) const
{
	if (IsValid(MissionWidget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Invalid mission widget on mission widget manager.");
	return false;
}
