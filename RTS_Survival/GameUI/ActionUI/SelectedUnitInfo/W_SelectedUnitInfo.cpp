// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_SelectedUnitInfo.h"

#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnitDescriptionItem/W_SelectedUnitDescription.h"

void UW_SelectedUnitInfo::SetupTargetPrefAndAgroStanceForNewUnit(AActor* SelectedActor)
{
	if (not IsValid(SelectedActor))
	{
		return;
	}
	if (ATankMaster* Tank = Cast<ATankMaster>(SelectedActor); IsValid(Tank))
	{
		UpdateTargetPreference(Tank->GetTargetPreference());
		UpdateAggroStance(Tank->GetEngagementStance());
	}
	if (AAircraftMaster* Aircraft = Cast<AAircraftMaster>(SelectedActor); IsValid(Aircraft))
	{
		UpdateTargetPreference(Aircraft->GetTargetPreference());
		UpdateAggroStance(Aircraft->GetEngagementStance());
	}
	if (ASquadController* SquadController = Cast<ASquadController>(SelectedActor); IsValid(SquadController))
	{
		UpdateTargetPreference(SquadController->GetSquadTargetPreference());
		UpdateAggroStance(SquadController->GetEngagementStance());
	}
}

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
                                                         const ESquadSubtype SquadSubtype,
                                                         const EBuildingExpansionType BxpSubtype
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

	if (not bM_IsInitialized)
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

void UW_SelectedUnitInfo::UpdateTargetPreference(const ETargetPreference TargetPreference)
{
	LastTargetPreference = TargetPreference;
	BP_UpdateTargetPreference(TargetPreference);
}

void UW_SelectedUnitInfo::UpdateAggroStance(const ERTSAggroBehaviour CurrentAggroStance)
{
	LastEngagedStance = CurrentAggroStance;
	BP_UpdateAgroStance(CurrentAggroStance);
}


void UW_SelectedUnitInfo::OnClickedAggroStanceButton()
{
	if (not EnsureIsValidActionUIManager() || not IsValid(M_ActionUIManager->GetPlayerController()))
	{
		return;
	}
	
	ERTSAggroBehaviour NewStance = RotateAggroStance(LastEngagedStance);
	M_ActionUIManager->GetPlayerController()->PropagateAggroToAllUnits(LastEngagedStance);
	
}

void UW_SelectedUnitInfo::OnClickedTargetPreferenceButton()
{
	if (not EnsureIsValidActionUIManager() || not IsValid(M_ActionUIManager->GetPlayerController()))
	{
		return;
	}
	ETargetPreference NewPreference = RotateTargetPreference(LastTargetPreference);
	if (NewPreference == ETargetPreference::Aircraft)
	{
		// Special case identifying units that can hit air do not propagate this to other units as it may
		// introduce AA units which are not AA.
		return;
	}
	M_ActionUIManager->GetPlayerController()->PropagateTargetPreferenceToAllUnits(NewPreference);
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

ERTSAggroBehaviour UW_SelectedUnitInfo::RotateAggroStance(const ERTSAggroBehaviour CurrentAggroStance) const
{
	if (CurrentAggroStance == ERTSAggroBehaviour::Stance_HoldPosition)
	{
		return ERTSAggroBehaviour::Stance_Aggressive;
	}
	return ERTSAggroBehaviour::Stance_HoldPosition;
}

ETargetPreference UW_SelectedUnitInfo::RotateTargetPreference(const ETargetPreference TargetPreference) const
{
	switch (TargetPreference)
	{
	case ETargetPreference::None:
		return ETargetPreference::Infantry;
	case ETargetPreference::Infantry:
		return ETargetPreference::Tank;
	case ETargetPreference::Tank:
		return ETargetPreference::Building;
	case ETargetPreference::Building:
		return ETargetPreference::Infantry;
	case ETargetPreference::Other:
		return ETargetPreference::Infantry;
	case ETargetPreference::Aircraft:
		return ETargetPreference::Aircraft;
	}
	return ETargetPreference::Infantry;
}

bool UW_SelectedUnitInfo::EnsureIsValidActionUIManager() const
{
	if (M_ActionUIManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "ActionUIManager",
	                                                      "UW_SelectedUnitInfo::GetIsValidActionUIManager");
	return false;
}
