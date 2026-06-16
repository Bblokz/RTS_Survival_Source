#include "GlobalAbilitySetDataAsset.h"

TObjectPtr<UObject> UGlobalAbilitySetDataAsset::GetGLobalAbilityForTypeAsCopyFromTemplate(const EGlobalAbility Type)
{
	if (not M_AbilityTemplatesByType.Contains(Type))
	{
		RTSFunctionLibrary::ReportError("Type is not contained in global ability template map: " +UEnum::GetValueAsString(Type))	;
		return nullptr;
	}
	UGlobalAbility* Template = M_AbilityTemplatesByType[Type];
	if (not IsValid(Template))
	{
		RTSFunctionLibrary::ReportError("Template for type is not valid in global ability template map: " +UEnum::GetValueAsString(Type))	;
		return nullptr;
	}
	// todo copy uobject safely and return the object Pointer.
	
}
