// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_SelectedUnitInfo.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/StandaloneTurret/StandaloneTurret.h"
#include "UnitDescriptionItem/W_SelectedUnitDescription.h"

#define LOCTEXT_NAMESPACE "SelectedUnitInfo"

namespace SelectedUnitInfoModeButtons
{
	/** The button's icon is its content, or the first image inside a panel used as content. */
	UImage* FindButtonImage(const UButton* Button)
	{
		if (not IsValid(Button))
		{
			return nullptr;
		}
		UWidget* Content = Button->GetContent();
		if (UImage* ContentImage = Cast<UImage>(Content))
		{
			return ContentImage;
		}
		const UPanelWidget* ContentPanel = Cast<UPanelWidget>(Content);
		if (not IsValid(ContentPanel))
		{
			return nullptr;
		}
		for (int32 ChildIndex = 0; ChildIndex < ContentPanel->GetChildrenCount(); ++ChildIndex)
		{
			if (UImage* ChildImage = Cast<UImage>(ContentPanel->GetChildAt(ChildIndex)))
			{
				return ChildImage;
			}
		}
		return nullptr;
	}

	FText GetTargetPreferenceName(const ETargetPreference TargetPreference)
	{
		switch (TargetPreference)
		{
		case ETargetPreference::Infantry:
			return LOCTEXT("PreferInfantryName", "Infantry");
		case ETargetPreference::Tank:
			return LOCTEXT("PreferVehiclesName", "Vehicles");
		case ETargetPreference::Building:
			return LOCTEXT("PreferStructuresName", "Structures");
		case ETargetPreference::Aircraft:
			return LOCTEXT("PreferAircraftName", "Aircraft");
		default:
			return LOCTEXT("PreferNoneName", "Closest enemy");
		}
	}
}

ETargetPreference UW_SelectedUnitInfo::GetSquadOrTeamWeaponTargetPreference(ASquadController* SquadController)
{
	// A crewed gun shows its gun's preference (a Flak is anti-air) rather than the crew's rifles.
	const ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(SquadController);
	if (IsValid(TeamWeaponController))
	{
		const ATeamWeapon* TeamWeapon = TeamWeaponController->GetControlledTeamWeaponNoReport();
		if (IsValid(TeamWeapon))
		{
			return TeamWeapon->GetTargetPreference();
		}
	}
	return SquadController->GetSquadTargetPreference();
}

void UW_SelectedUnitInfo::SetupTargetPrefAndAgroStanceForNewUnit(AActor* SelectedActor)
{
	// Units without a stance or weapons fall through to None so the buttons never show the previous unit's mode.
	ETargetPreference TargetPreference = ETargetPreference::None;
	ERTSAggroBehaviour AggroStance = ERTSAggroBehaviour::Stance_None;
	if (ATankMaster* Tank = Cast<ATankMaster>(SelectedActor); IsValid(Tank))
	{
		TargetPreference = Tank->GetTargetPreference();
		AggroStance = Tank->GetEngagementStance();
	}
	else if (const AAircraftMaster* Aircraft = Cast<AAircraftMaster>(SelectedActor); IsValid(Aircraft))
	{
		// Not GetTargetPreference: that is the aircraft's own target type, always Aircraft.
		TargetPreference = Aircraft->GetAircraftTargetPreference();
		AggroStance = Aircraft->GetEngagementStance();
	}
	else if (ASquadController* SquadController = Cast<ASquadController>(SelectedActor); IsValid(SquadController))
	{
		TargetPreference = GetSquadOrTeamWeaponTargetPreference(SquadController);
		AggroStance = SquadController->GetEngagementStance();
	}
	else if (const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(SelectedActor); IsValid(Expansion))
	{
		TargetPreference = Expansion->GetTargetPreference();
	}
	else if (const AStandaloneTurret* StandaloneTurret = Cast<AStandaloneTurret>(SelectedActor);
		IsValid(StandaloneTurret))
	{
		TargetPreference = StandaloneTurret->GetTargetPreference();
	}
	UpdateTargetPreference(TargetPreference);
	UpdateAggroStance(AggroStance);
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
	// SetupUnitInfoForNewUnit (Blueprint) has set the portrait by now; NativeTick catches a portrait set later.
	UnitDescription->SetupTacticalIcons(GetPortraitResource());
}

const UObject* UW_SelectedUnitInfo::GetPortraitResource() const
{
	if (not IsValid(M_UnitInfoImage))
	{
		return nullptr;
	}
	return M_UnitInfoImage->GetBrush().GetResourceObject();
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

	// Flip the description’s visibility based on whether the mouse is over the border.
	// A mode button shows its own tooltip, so the description stays closed over it.
	if (UnitInfoBorder->IsHovered() && not GetIsHoveringModeButton())
	{
		UnitDescription->SetupTacticalIcons(GetPortraitResource());
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
	RefreshTargetPreferenceButton(TargetPreference);
	BP_UpdateTargetPreference(TargetPreference);
}

void UW_SelectedUnitInfo::UpdateAggroStance(const ERTSAggroBehaviour CurrentAggroStance)
{
	LastEngagedStance = CurrentAggroStance;
	RefreshStanceButton(CurrentAggroStance);
	BP_UpdateAgroStance(CurrentAggroStance);
}


void UW_SelectedUnitInfo::OnClickedAggroStanceButton()
{
	if (LastEngagedStance == ERTSAggroBehaviour::Stance_None)
	{
		// The primary unit has no target acquisition (wheeled vehicles, buildings); nothing to switch.
		return;
	}
	if (not EnsureIsValidActionUIManager() || not IsValid(M_ActionUIManager->GetPlayerController()))
	{
		return;
	}

	const ERTSAggroBehaviour NewStance = RotateAggroStance(LastEngagedStance);
	M_ActionUIManager->GetPlayerController()->PropagateAggroToAllUnits(NewStance);
	UpdateAggroStance(NewStance);
}

void UW_SelectedUnitInfo::OnClickedTargetPreferenceButton()
{
	if (LastTargetPreference == ETargetPreference::Aircraft)
	{
		// Anti-air weapons stay locked on aircraft (each weapon refuses the change too);
		// the tooltip tells the player why the click does nothing.
		return;
	}
	if (not EnsureIsValidActionUIManager() || not IsValid(M_ActionUIManager->GetPlayerController()))
	{
		return;
	}
	const ETargetPreference NewPreference = RotateTargetPreference(LastTargetPreference);
	// Anti-air units in the selection ignore this in their weapons' SetTargetPreference.
	M_ActionUIManager->GetPlayerController()->PropagateTargetPreferenceToAllUnits(NewPreference);
	UpdateTargetPreference(NewPreference);
}

void UW_SelectedUnitInfo::RefreshStanceButton(const ERTSAggroBehaviour CurrentAggroStance) const
{
	if (not IsValid(ButtonStance))
	{
		return;
	}
	ButtonStance->SetIsEnabled(CurrentAggroStance != ERTSAggroBehaviour::Stance_None);
	ButtonStance->SetToolTipText(GetStanceTooltip(CurrentAggroStance));
	SetButtonIcon(ButtonStance, GetStanceIcon(CurrentAggroStance));
}

void UW_SelectedUnitInfo::RefreshTargetPreferenceButton(const ETargetPreference TargetPreference) const
{
	if (not IsValid(ButtonTargetPreference))
	{
		return;
	}
	ButtonTargetPreference->SetToolTipText(GetTargetPreferenceTooltip(TargetPreference));
	SetButtonIcon(ButtonTargetPreference, GetTargetPreferenceIcon(TargetPreference));
}

FText UW_SelectedUnitInfo::GetStanceTooltip(const ERTSAggroBehaviour CurrentAggroStance) const
{
	switch (CurrentAggroStance)
	{
	case ERTSAggroBehaviour::Stance_Aggressive:
		return LOCTEXT("StanceAggressiveTooltip",
		               "Stance: Aggressive\n"
		               "Moves out to attack enemies it spots just beyond its weapon range.\n"
		               "Click to switch the selection to Hold Position.");
	case ERTSAggroBehaviour::Stance_HoldPosition:
		return LOCTEXT("StanceHoldTooltip",
		               "Stance: Hold Position\n"
		               "Stays in place and only fires at enemies inside its weapon range.\n"
		               "Click to switch the selection to Aggressive.");
	default:
		return LOCTEXT("StanceNoneTooltip",
		               "Stance: none\n"
		               "This unit has no engagement stance to change.");
	}
}

FText UW_SelectedUnitInfo::GetTargetPreferenceTooltip(const ETargetPreference TargetPreference) const
{
	if (TargetPreference == ETargetPreference::Aircraft)
	{
		return LOCTEXT("TargetPreferenceAntiAirTooltip",
		               "Target preference: Aircraft (anti-air)\n"
		               "Anti-air guns always fire at aircraft first and engage ground targets when no aircraft are in range.\n"
		               "This preference can't be changed.");
	}
	const FText NextPreferenceName = SelectedUnitInfoModeButtons::GetTargetPreferenceName(
		RotateTargetPreference(TargetPreference));
	if (TargetPreference == ETargetPreference::Infantry
		|| TargetPreference == ETargetPreference::Tank
		|| TargetPreference == ETargetPreference::Building)
	{
		return FText::Format(LOCTEXT("TargetPreferenceTooltip",
		                             "Target preference: {0}\n"
		                             "Fires at {0} in range first, other enemies only when none are in range.\n"
		                             "Click to make the selection prefer {1}. Anti-air units keep aiming at aircraft."),
		                     SelectedUnitInfoModeButtons::GetTargetPreferenceName(TargetPreference),
		                     NextPreferenceName);
	}
	return FText::Format(LOCTEXT("TargetPreferenceNoneTooltip",
	                             "Target preference: none\n"
	                             "Fires at the closest enemy in range.\n"
	                             "Click to make the selection prefer {0}."),
	                     NextPreferenceName);
}

const TSoftObjectPtr<UObject>& UW_SelectedUnitInfo::GetStanceIcon(const ERTSAggroBehaviour CurrentAggroStance) const
{
	if (CurrentAggroStance == ERTSAggroBehaviour::Stance_Aggressive)
	{
		return ModeIcons.StanceAggressive;
	}
	// No stance shows the hold icon greyed out by the disabled button.
	return ModeIcons.StanceHoldPosition;
}

const TSoftObjectPtr<UObject>& UW_SelectedUnitInfo::GetTargetPreferenceIcon(
	const ETargetPreference TargetPreference) const
{
	static const TSoftObjectPtr<UObject> NoIcon;
	switch (TargetPreference)
	{
	case ETargetPreference::Infantry:
		return ModeIcons.PreferInfantry;
	case ETargetPreference::Tank:
		return ModeIcons.PreferVehicles;
	case ETargetPreference::Building:
		return ModeIcons.PreferStructures;
	case ETargetPreference::Aircraft:
		return ModeIcons.AntiAir;
	default:
		return NoIcon;
	}
}

void UW_SelectedUnitInfo::SetButtonIcon(const UButton* Button, const TSoftObjectPtr<UObject>& Icon)
{
	UImage* ButtonImage = SelectedUnitInfoModeButtons::FindButtonImage(Button);
	if (not IsValid(ButtonImage))
	{
		return;
	}
	UObject* IconResource = Icon.IsNull() ? nullptr : Icon.LoadSynchronous();
	if (not IsValid(IconResource))
	{
		// Hidden keeps the button's size, so the layout does not jump.
		ButtonImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	ButtonImage->SetBrushResourceObject(IconResource);
	ButtonImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UW_SelectedUnitInfo::GetIsHoveringModeButton() const
{
	return (IsValid(ButtonStance) && ButtonStance->IsHovered())
		|| (IsValid(ButtonTargetPreference) && ButtonTargetPreference->IsHovered());
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

#undef LOCTEXT_NAMESPACE
