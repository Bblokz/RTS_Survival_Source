// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_SelectedUnitInfo.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnitDescriptionItem/W_SelectedUnitDescription.h"

void UW_SelectedUnitInfo::InitSelectedUnitInfo(UW_SelectedUnitDescription* UnitDesc, UActionUIManager* ActionUIManager)
{
	UnitDescription = UnitDesc;
	M_ActionUIManager = ActionUIManager;
	(void)EnsureIsValidActionUIManager();
	(void)EnsureIsValidUnitDescription();
	bM_IsInitialized = true;
}

void UW_SelectedUnitInfo::SetupUnitDescriptionForNewUnit(const AActor* SelectedActor,
                                                         const EAllUnitType PrimaryUnitType,
                                                         const ENomadicSubtype NomadicSubtype,
                                                         const ETankSubtype TankSubtype,
                                                         const ESquadSubtype SquadSubtype, const EBuildingExpansionType BxpSubtype
) const
{
	if (not IsValid(SelectedActor) || not EnsureIsValidUnitDescription())
	{
		return;
	}
	UnitDescription->SetupUnitDescription(SelectedActor,
	                                      PrimaryUnitType,
	                                      NomadicSubtype,
	                                      TankSubtype,
	                                      SquadSubtype, BxpSubtype
	);
}

void UW_SelectedUnitInfo::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if(not bM_IsInitialized)
	{
		return;
	}
	

	// Bail out if we don't have what we need
	if (!EnsureIsValidUnitDescription() || not EnsureIsValidActionUIManager())
	{
		return;
	}

	// Flip the description’s visibility based on whether the mouse is over the border
	if (UnitInfoBorder->IsHovered())
	{
		UnitDescription->SetVisibility(ESlateVisibility::Visible);
		M_ActionUIManager->OnHoverSelectedUnitInfo(true);
	}
	else
	{
		M_ActionUIManager->OnHoverSelectedUnitInfo(false);
		UnitDescription->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UW_SelectedUnitInfo::EnsureIsValidUnitDescription() const
{
	if (not IsValid(UnitDescription))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "UnitDescription(Widget)",
		                                                      "UW_SelectedUnitInfo::InitializeUnitArmorDescription");
		return false;
	}
	return true;
}

bool UW_SelectedUnitInfo::EnsureIsValidActionUIManager() const
{
	if(M_ActionUIManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "ActionUIManager",
	                                                      "UW_SelectedUnitInfo::GetIsValidActionUIManager");
	return false;
	
}
