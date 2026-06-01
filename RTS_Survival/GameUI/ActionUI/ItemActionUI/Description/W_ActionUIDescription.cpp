// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ActionUIDescription.h"

#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ResearchTechnologyAbilityComponent/ResearchTechnologyAbilityComp.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
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

void UW_ActionUIDescription::InitActionUIDescription(UActionUIManager* ActionUIManager)
{
	M_ActionUIManager = ActionUIManager;
	(void)EnsureIsValidActionUIManager();
}


void UW_ActionUIDescription::SetupDescription(const EAbilityID Ability, const int32 CustomType,
                                              const FText& DataTableText_Title, const FText& DataTableText_Description,
                                              UTexture* DataTable_Icon)
{
	if (not M_ActionItemImage || not M_AbilityDescription || not M_AbilityTitle)
	{
		return;
	}
	if (Ability == EAbilityID::IdResearchTechnology)
	{
		OnOverrideDescriptionForTechnology(
			Ability,
			CustomType,
			DataTableText_Title,
			DataTableText_Description,
			Cast<UTexture2D>(DataTable_Icon));
		return;
	}

	SetDataToTextAndImage(
		DataTableText_Title,
		DataTableText_Description,
		DataTable_Icon);
}


bool UW_ActionUIDescription::EnsureIsValidActionUIManager() const
{
	if(not M_ActionUIManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ActionUIManager"),
			TEXT("UW_ActionUIDescription::EnsureIsValidActionUIManager"),
			this
		);
		return false;
	}
	return true;
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
	if (Ability != EAbilityID::IdResearchTechnology)
	{
		SetDataToTextAndImage(DataTableText_Title, DataTableText_Description, DataTable_Icon);
		return;
	}

	if (not EnsureIsValidActionUIManager())
	{
		SetDataToTextAndImage(DataTableText_Title, DataTableText_Description, DataTable_Icon);
		return;
	}

	TWeakInterfacePtr<ICommands> PrimarySelectedUnit = M_ActionUIManager->GetPrimarySelectedICommands();
	if (not PrimarySelectedUnit.IsValid())
	{
		RTSFunctionLibrary::ReportError("No valid primary selected unit in OnOverrideDescriptionForTechnology");
		SetDataToTextAndImage(DataTableText_Title, DataTableText_Description, DataTable_Icon);
		return;
	}

	AActor* PrimarySelectedActor = PrimarySelectedUnit->GetOwnerActor();
	const ETechnology Technology = static_cast<ETechnology>(CustomType);
	const UResearchTechnologyAbilityComp* ResearchTechnologyComp =
		FAbilityHelpers::GetResearchTechnologyAbilityCompOfType(Technology, PrimarySelectedActor);
	if (not IsValid(ResearchTechnologyComp))
	{
		SetDataToTextAndImage(DataTableText_Title, DataTableText_Description, DataTable_Icon);
		return;
	}

	UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(PrimarySelectedActor);
	if (not IsValid(PlayerTechManager))
	{
		SetDataToTextAndImage(DataTableText_Title, DataTableText_Description, DataTable_Icon);
		return;
	}

	const TArray<ETechnology> MissingRequiredTechnologies =
		PlayerTechManager->GetMissingRequiredTechnologies(ResearchTechnologyComp->GetRequiredTechnologies());

	if (MissingRequiredTechnologies.IsEmpty())
	{
		SetDataToTextAndImage(DataTableText_Title, DataTableText_Description, DataTable_Icon);
		return;
	}

	FString OverrideDescription = "<Text_sBad>Required:</>";
	for (const ETechnology RequiredTechnology : ResearchTechnologyComp->GetRequiredTechnologies())
	{
		OverrideDescription += "\n";
		OverrideDescription += FRTSRichTextConverter::MakeRTSRich(
			Global_GetTechDisplayName(RequiredTechnology),
			ERTSRichText::Text_Armor);
	}

	SetDataToTextAndImage(DataTableText_Title, FText::FromString(OverrideDescription), DataTable_Icon);
}
