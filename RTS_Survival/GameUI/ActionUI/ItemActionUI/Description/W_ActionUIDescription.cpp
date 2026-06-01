// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ActionUIDescription.h"

#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_ActionUIDescription::SetCostsForAbility(const TMap<ERTSResourceType, int32>& ResourceCosts) const
{
	if (not EnsureIsValidCostDisplay())
	{
		return;
	}
	if (ResourceCosts.IsEmpty())
	{
		M_CostDisplay->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_CostDisplay->SetVisibility(ESlateVisibility::Visible);
	M_CostDisplay->SetupCost(ResourceCosts);
}

void UW_ActionUIDescription::SetupDescription(const EAbilityID Ability, const int32 CustomType,
                                              const FText& DataTableText_Title, const FText& DataTableText_Description,
                                              UTexture* DataTable_Icon)
{
	if (not M_ActionItemImage || not M_AbilityDescription || not M_AbilityTitle)
	{
		return;
	}
	// todo catch ability == tech then call the override description for tech.
	SetDataToTextAndImage(
		DataTableText_Title,
		DataTableText_Description,
		DataTable_Icon);
}


bool UW_ActionUIDescription::EnsureIsValidCostDisplay() const
{
	if (IsValid(M_CostDisplay))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CostDisplay"),
		TEXT("UW_ActionUIDescription::EnsureIsValidCostDisplay"),
		this
	);
	return false;
}

void UW_ActionUIDescription::SetDataToTextAndImage(const FText& DataTableText_Title,
                                                   const FText& DataTableText_Description, UTexture* DataTable_Icon) const
{
	if (not IsValid(DataTable_Icon))
	{
		RTSFunctionLibrary::PrintString("Invalid texture provided for action UI description; ignoring.");
	}
	else
	{
		UTexture2D* DataTable_Icon_Cast = Cast<UTexture2D>(DataTable_Icon);
		if (not IsValid(DataTable_Icon_Cast))
		{
			RTSFunctionLibrary::PrintString(
				"Texture provided for action UI description is not a UTexture2D; (CAST FAILURE) ignoring.");
		}
		else
		{
			M_ActionItemImage->SetBrushFromTexture(DataTable_Icon_Cast);
		}
	}
	M_AbilityDescription->SetText(DataTableText_Description);
	M_AbilityTitle->SetText(DataTableText_Title);
}

void UW_ActionUIDescription::OnOverrideDescriptionForTechnology(const EAbilityID Ability, const int32 CustomType,
                                                                const FText& DataTableText_Title,
                                                                const FText& DataTableText_Description,
                                                                TObjectPtr<UTexture2D> DataTable_Icon)
{
}
