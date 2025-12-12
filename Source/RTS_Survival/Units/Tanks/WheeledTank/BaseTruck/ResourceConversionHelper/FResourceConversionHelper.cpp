#include "FResourceConversionHelper.h"

#include "RTS_Survival/RTSComponents/ResourceConverter/ResourceConverterComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

void FResourceConversionHelper::OnBxpCreatedOrDestroyed(ANomadicVehicle* NomadicVehicle)
{
	if (not IsValidNomadic(NomadicVehicle))
	{
		return;
	}
	const bool bSetEnabled = GetHasConversionEnabled(NomadicVehicle);
	const bool bHasAnyConverters = UpdateAllConvertersWithConversionEnabled(NomadicVehicle, bSetEnabled);
	UpdateConverterAbility(NomadicVehicle, bSetEnabled, bHasAnyConverters);
}

void FResourceConversionHelper::OnNomadicExpanded(ANomadicVehicle* NomadicVehicle, const bool bIsExpanded)
{
	if (not IsValidNomadic(NomadicVehicle))
	{
		return;
	}
	if (not bIsExpanded)
	{
		(void)UpdateAllConvertersWithConversionEnabled(NomadicVehicle, false);
		// Make sure the abilties are no longer on the command card as the nomadic vehicle is not expanded anymore.
		RemoveAllConversionAbilities(NomadicVehicle);
	}
	else if (GetHasConversionEnabled(NomadicVehicle))
	{
		const bool bHasAnyConverters = UpdateAllConvertersWithConversionEnabled(NomadicVehicle, true);
		UpdateConverterAbility(NomadicVehicle, true, bHasAnyConverters);
	}
}

void FResourceConversionHelper::ManuallySetConvertersEnabled(const bool bEnabled, ANomadicVehicle* NomadicVehicle)
{
	if (not IsValidNomadic(NomadicVehicle))
	{
		return;
	}
	SetHasConversionEnabled(NomadicVehicle, bEnabled);
	const bool bHasAnyConverters = UpdateAllConvertersWithConversionEnabled(NomadicVehicle, bEnabled);
	UpdateConverterAbility(NomadicVehicle, bEnabled, bHasAnyConverters);
}

bool FResourceConversionHelper::UpdateAllConvertersWithConversionEnabled(ANomadicVehicle* NomadicVehicle,
                                                                         const bool bConversionEnabled)
{
	bool bHasAnyConverters = false;
	if (UResourceConverterComponent* Converter = GetConverter(NomadicVehicle))
	{
		bHasAnyConverters = true;
		// Note: is idempotent.
		Converter->SetResourceConversionEnabled(bConversionEnabled);
	}
	for (const auto EachBxpItem : NomadicVehicle->GetBuildingExpansions())
	{
		if (not IsValid(EachBxpItem.Expansion))
		{
			continue;
		}
		if (UResourceConverterComponent* Converter = GetConverter(EachBxpItem.Expansion))
		{
			bHasAnyConverters = true;
			// Note: is idempotent.
			Converter->SetResourceConversionEnabled(bConversionEnabled);
		}
	}
	return bHasAnyConverters;
}

void FResourceConversionHelper::UpdateConverterAbility(ANomadicVehicle* NomadicVehicle, const bool bConversionEnabled,
                                                       const bool bHasAnyConverterComponents)
{
	if (not bHasAnyConverterComponents)
	{
		// Make sure that both the disable as well as the enable Abilities are removed.
		RemoveAllConversionAbilities(NomadicVehicle);
		return;
	}
	if (bConversionEnabled)
	{
		// Make sure to not double enable.
		RemoveEnableConversionAbility(NomadicVehicle);

		// Either this is the first time a converter component is added and we need to add the ability
		// Or we are re-enabling after a disable.
		if (not NomadicVehicle->HasAbility(EAbilityID::IdDisableResourceConversion))
		{
			const bool bResult = NomadicVehicle->AddAbility(EAbilityID::IdDisableResourceConversion);
			if (not bResult)
			{
				RTSFunctionLibrary::ReportError("Failed to add disable conversion ability on : " +
					NomadicVehicle->GetName() + " but conversion is enabled!");
			}
		}
	}
	else
	{
		// We have conversion components, but they are set to disabled; make sure we do not have the disable ability and
		// add the enable ability if not already present.
		RemoveDisableConversionAbility(NomadicVehicle);
		if (not NomadicVehicle->HasAbility(EAbilityID::IdEnableResourceConversion))
		{
			const bool bResult = NomadicVehicle->AddAbility(EAbilityID::IdEnableResourceConversion);
			if (not bResult)
			{
				RTSFunctionLibrary::ReportError("Failed to add enable conversion ability on : " +
					NomadicVehicle->GetName() + " but conversion is disabled!");
			}
		}
	}
}

bool FResourceConversionHelper::IsValidNomadic(const ANomadicVehicle* NomadicVehicle)
{
	if (not IsValid(NomadicVehicle))
	{
		return false;
	}
	return true;
}

void FResourceConversionHelper::SetHasConversionEnabled(ANomadicVehicle* Nomadic,
                                                        const bool bHasConversionEnabled)
{
	Nomadic->M_ResourceConversionState.bM_ResourceConversionEnabled = bHasConversionEnabled;
}

bool FResourceConversionHelper::GetHasConversionEnabled(const ANomadicVehicle* Nomadic)
{
	return Nomadic->M_ResourceConversionState.bM_ResourceConversionEnabled;
}

void FResourceConversionHelper::RemoveEnableConversionAbility(ANomadicVehicle* NomadicVehicle)
{
	if(NomadicVehicle->HasAbility(EAbilityID::IdEnableResourceConversion))
	{
		(void)NomadicVehicle->RemoveAbility(EAbilityID::IdEnableResourceConversion);
	}
}

void FResourceConversionHelper::RemoveDisableConversionAbility(ANomadicVehicle* NomadicVehicle)
{
	if(NomadicVehicle->HasAbility(EAbilityID::IdDisableResourceConversion))
	{
		(void)NomadicVehicle->RemoveAbility(EAbilityID::IdDisableResourceConversion);
	}
}

UResourceConverterComponent* FResourceConversionHelper::GetConverter(const AActor* Owner)
{
	if (not Owner)
	{
		return nullptr;
	}
	UResourceConverterComponent* FoundConverter = Owner->FindComponentByClass<UResourceConverterComponent>();
	if (IsValid(FoundConverter))
	{
		return FoundConverter;
	}
	return nullptr;
}

void FResourceConversionHelper::RemoveAllConversionAbilities(ANomadicVehicle* NomadicVehicle)
{
	RemoveEnableConversionAbility(NomadicVehicle);
	RemoveDisableConversionAbility(NomadicVehicle);
}
