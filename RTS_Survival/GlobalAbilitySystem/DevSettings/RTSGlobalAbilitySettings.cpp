#include "RTSGlobalAbilitySettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


TSoftObjectPtr<URTSGlobalAbilitySettings> URTSGlobalAbilitySettings::GetGlobalAbilityOfType(const EGlobalAbility Type)
{
	if (not M_AbilityTemplatesByType.Contains(Type))
	{
		RTSFunctionLibrary::ReportError("Type is not contained in global ability template map: " +UEnum::GetValueAsString(Type))	;
		return nullptr;
	}
	return URTSGlobalAbilitySettings::GetGlobalAbilityOfType(Type);
}
