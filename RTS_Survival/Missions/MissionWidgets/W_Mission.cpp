#include "W_Mission.h"

#include "Components/RichTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_Mission::InitMissionWidget(const FMissionWidgetState& WidgetState,
                                   UMissionBase* AssociatedMission,
                                   const bool bUseNextAsExpanded,
                                   const bool bStartAsCollapsedWidget)
{
	if (not EnsureAssociatedMissionIsValid(AssociatedMission) || not GetIsValidRichTextBlocks())
	{
		return;
	}

	EMissionWidgetState WidgetType = bStartAsCollapsedWidget
		                                 ? EMissionWidgetState::OMS_Minimized
		                                 : EMissionWidgetState::OMS_Expanded;

	bM_UseNextAsExpandedWidget = bUseNextAsExpanded;
	if (bM_UseNextAsExpandedWidget && WidgetType == EMissionWidgetState::OMS_Expanded)
	{
		WidgetType = EMissionWidgetState::OMS_ExpandedNext;
	}

	M_AssociatedMission = AssociatedMission;
	bM_IsInUse = true;
	M_WidgetState = WidgetType;
	M_RichTitle->SetText(WidgetState.MissionTitle);
	M_RichDescription->SetText(WidgetState.MissionDescription);
	M_NextButtonType = WidgetState.NextButtonType;

	// Respect global-hide and pooling state.
	ApplyVisibilityPolicy();

	OnChangeWidgetState(WidgetType, WidgetState.TextSpeed);
}

void UW_Mission::MarkWidgetAsFree()
{
	M_WidgetState = EMissionWidgetState::OMS_Minimized;
	OnChangeWidgetState(EMissionWidgetState::OMS_Minimized, EMissionTextSpeed::TS_None);
	SetVisibility(ESlateVisibility::Collapsed);
	bM_IsInUse = false;
	M_AssociatedMission.Reset();
}

void UW_Mission::OnHideAllGameUI(const bool bHide)
{
	if (bHide)
	{
		if (bM_WasHiddenByAllGameUI)
		{
			return;
		}
		bM_WasHiddenByAllGameUI = true;
		ApplyVisibilityPolicy();
		return;
	}

	// Un-hide
	if (not bM_WasHiddenByAllGameUI)
	{
		return;
	}
	bM_WasHiddenByAllGameUI = false;
	ApplyVisibilityPolicy();
}


void UW_Mission::OnClickedNextMission()
{
	if (M_WidgetState != EMissionWidgetState::OMS_ExpandedNext)
	{
		RTSFunctionLibrary::ReportError("The player clicked on next for a mission but that mission was not set to the "
			"expanded next state!");
	}
	if (not EnsureAssociatedMissionIsValid(M_AssociatedMission))
	{
		return;
	}
	M_AssociatedMission->OnMissionComplete();
}

void UW_Mission::SetWidgetState(const EMissionWidgetState NewState)
{
	M_WidgetState = NewState;
}

bool UW_Mission::GetIsNextButtonExpandedWidget() const
{
	return bM_UseNextAsExpandedWidget;
}

EMissionWidgetNextButton UW_Mission::GetNextButtonType() const
{
	return M_NextButtonType;
}


bool UW_Mission::GetIsValidRichTextBlocks() const
{
	if (IsValid(M_RichDescription) && IsValid(M_RichTitle))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("One of the rich text blocks is not valid on mission widget: " + GetName());
	return false;
}

bool UW_Mission::EnsureAssociatedMissionIsValid(const TWeakObjectPtr<UMissionBase> MissionBase) const
{
	if (not MissionBase.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid mission base on mission widget: " + GetName());
		return false;
	}
	return true;
}

void UW_Mission::ApplyVisibilityPolicy()
{
	// If not in use, keep collapsed regardless of global hide.
	if (not bM_IsInUse)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Global hide wins; otherwise visible.
	if (bM_WasHiddenByAllGameUI)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
}
