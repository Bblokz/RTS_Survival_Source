#include "ScriptedTriggerAreaSubsystem.h"

#include "RTS_Survival/Missions/TriggerAreas/Settings/MissionTriggerAreaSettings.h"
#include "RTS_Survival/ScriptedTriggers/Settings/ScriptedTriggerAreaSettings.h"

void UScriptedTriggerAreaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UScriptedTriggerAreaSettings* TriggerAreaSettings = GetDefault<UScriptedTriggerAreaSettings>();
	if (IsValid(TriggerAreaSettings))
	{
		M_RectangleTriggerAreaClass = TriggerAreaSettings->M_RectangleTriggerAreaClass;
		M_SphereTriggerAreaClass = TriggerAreaSettings->M_SphereTriggerAreaClass;
	}

	const bool bHasRectangleClass = M_RectangleTriggerAreaClass != nullptr;
	const bool bHasSphereClass = M_SphereTriggerAreaClass != nullptr;
	if (bHasRectangleClass && bHasSphereClass)
	{
		return;
	}

	const UMissionTriggerAreaSettings* MissionTriggerAreaSettings = GetDefault<UMissionTriggerAreaSettings>();
	if (not IsValid(MissionTriggerAreaSettings))
	{
		return;
	}

	if (not bHasRectangleClass)
	{
		M_RectangleTriggerAreaClass = MissionTriggerAreaSettings->M_RectangleTriggerAreaClass;
	}

	if (not bHasSphereClass)
	{
		M_SphereTriggerAreaClass = MissionTriggerAreaSettings->M_SphereTriggerAreaClass;
	}
}

TSubclassOf<ATriggerArea> UScriptedTriggerAreaSubsystem::GetRectangleTriggerAreaClass() const
{
	return M_RectangleTriggerAreaClass;
}

TSubclassOf<ATriggerArea> UScriptedTriggerAreaSubsystem::GetSphereTriggerAreaClass() const
{
	return M_SphereTriggerAreaClass;
}
