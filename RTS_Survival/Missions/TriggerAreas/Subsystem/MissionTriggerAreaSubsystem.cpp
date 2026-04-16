#include "MissionTriggerAreaSubsystem.h"

#include "RTS_Survival/Missions/TriggerAreas/Settings/MissionTriggerAreaSettings.h"

void UMissionTriggerAreaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMissionTriggerAreaSettings* TriggerAreaSettings = GetDefault<UMissionTriggerAreaSettings>();
	if (not IsValid(TriggerAreaSettings))
	{
		return;
	}

	M_RectangleTriggerAreaClass = TriggerAreaSettings->M_RectangleTriggerAreaClass;
	M_SphereTriggerAreaClass = TriggerAreaSettings->M_SphereTriggerAreaClass;
}

TSubclassOf<ATriggerArea> UMissionTriggerAreaSubsystem::GetRectangleTriggerAreaClass() const
{
	return M_RectangleTriggerAreaClass;
}

TSubclassOf<ATriggerArea> UMissionTriggerAreaSubsystem::GetSphereTriggerAreaClass() const
{
	return M_SphereTriggerAreaClass;
}
