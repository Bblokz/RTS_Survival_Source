#include "W_OptionBuildingExpansion.h"

#include "BxpOptionHoverDescription/W_BxpOptionHoverDescription.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/BottomRightUIPanelState/BottomRightUIPanelState.h"

void UW_OptionBuildingExpansion::InitW_OptionBuildingExpansion(UMainGameUI* NewMainGameU, const int NewIndex)
{
	M_MainGameUI = NewMainGameU;
	M_WidgetScrollbarIndex = NewIndex;
	// Error check.
	void (EnsureMainGameUIIsValid());
}

void UW_OptionBuildingExpansion::SetHoverDescription(UW_BxpOptionHoverDescription* NewHoverDescription)
{
	HoverDescription = NewHoverDescription;
}

void UW_OptionBuildingExpansion::UpdateOptionBuildingExpansion(
	const FBxpOptionData& BxpOptionWidgetState,
	const bool bHasFreeSocketsRemaining,
	const bool bAlreadyHasBxpItemOfType)
{
	M_BxpOptionWidgetState = BxpOptionWidgetState;

	bool bIsBlockedByNoSocketsRemaining = false;
	bool bIsBlockedByNotUnique = false;
	bool bShouldBeDisabled = false;

	if (BxpOptionWidgetState.BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket)
	{
		bIsBlockedByNoSocketsRemaining = !bHasFreeSocketsRemaining;
		bShouldBeDisabled = bIsBlockedByNoSocketsRemaining;
	}

	if (BxpOptionWidgetState.BxpConstructionRules.ConstructionType == EBxpConstructionType::AtBuildingOrigin)
	{
		bIsBlockedByNotUnique = bAlreadyHasBxpItemOfType;
		bShouldBeDisabled = bIsBlockedByNotUnique;
	}

	// Update visuals (blueprint side)
	UpdateVisibleButtonState(BxpOptionWidgetState.ExpansionType);

	// Save hover data so the tooltip still shows why it's blocked
	M_OptionHoverData = {
		bIsBlockedByNoSocketsRemaining,
		bIsBlockedByNotUnique,
		BxpOptionWidgetState.BxpConstructionRules.ConstructionType
	};

	//  darken / gray out the image to show disabled state
	if (IsValid(M_BxpOptionImage))
	{
		const float Opacity = bShouldBeDisabled ? 0.35f : 1.0f;
		M_BxpOptionImage->SetOpacity(Opacity);
	}
	bM_IsDisabled = bShouldBeDisabled;
}

void UW_OptionBuildingExpansion::OnClickedBuildingExpansionOption()
{
	if (not EnsureMainGameUIIsValid())
	{
		return;
	}

	// Prevent clicks when disabled (gray-out state)
	if (bM_IsDisabled)
	{
		return;
	}

	M_MainGameUI->ClickedOptionBuildingExpansion(M_BxpOptionWidgetState, M_WidgetScrollbarIndex);
}

void UW_OptionBuildingExpansion::OnHoverBxpOption(const bool bHover)
{
	if (not EnsureMainGameUIIsValid())
	{
		return;
	}
	UpdateHoverDescription(bHover);
	if (bHover)
	{
		M_MainGameUI->SetBottomUIPanel(EShowBottomRightUIPanel::Show_BxpDescription,
		                               M_BxpOptionWidgetState.ExpansionType,
		                               false);
		return;
	}
	M_MainGameUI->SetBottomUIPanel(EShowBottomRightUIPanel::Show_ActionUI);
}

void UW_OptionBuildingExpansion::UpdateButtonWithGlobalSlateStyle()
{
	if (not ButtonStyleAsset)
	{
		RTSFunctionLibrary::ReportError("ButtonStyle null."
			"\n at widget: " + GetName() +
			"\n Forgot to set style reference in UW_TrainingItem::UpdateButtonWithGlobalSlateStyle?");
		return;
	}
	const FButtonStyle* ButtonStyle = ButtonStyleAsset->GetStyle<FButtonStyle>();
	if (ButtonStyle && M_BxpOptionButton)
	{
		M_BxpOptionButton->SetStyle(*ButtonStyle);
	}
}

bool UW_OptionBuildingExpansion::EnsureMainGameUIIsValid() const
{
	if (not IsValid(M_MainGameUI))
	{
		RTSFunctionLibrary::ReportError("MainGameUI is not valid in OnHoverBxpOption"
			"\n at function UW_OptionBuildingExpansion::EnsureMainGameUIIsValid");
		return false;
	}
	return true;
}

void UW_OptionBuildingExpansion::UpdateHoverDescription(const bool bHover)
{
	if (not EnsureIsValidHoverDescriptionWidget())
	{
		return;
	}
	const ESlateVisibility NewVisibility = bHover ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (HoverDescription->GetVisibility() != NewVisibility)
	{
		HoverDescription->SetVisibility(NewVisibility);
	}
	if (M_OptionHoverData.bIsBlockedByNoSockets)
	{
		HoverDescription->SetText("<Text_Error>No free sockets to place Expansion</>");
		return;
	}
	if (M_OptionHoverData.bIsBlockedByNotUnique)
	{
		HoverDescription->SetText("<Text_Error>Can only have one</>");
		return;
	}
	switch (M_OptionHoverData.ConstructionType)
	{
	case EBxpConstructionType::None:
		break;
	case EBxpConstructionType::Free:
		HoverDescription->SetText("<Text_Armor>Place freely around Building</>");
		return;
	case EBxpConstructionType::Socket:
		HoverDescription->SetText("<Text_Armor>Attach to building socket</>");;
		return;
	case EBxpConstructionType::AtBuildingOrigin:
		HoverDescription->SetText("<Text_Armor>Building enhancement</>");;
	}
}

bool UW_OptionBuildingExpansion::EnsureIsValidHoverDescriptionWidget()
{
	if (not IsValid(HoverDescription))
	{
		RTSFunctionLibrary::ReportError("Invalid hover description widget, for bxp option widget");
		return false;
	}
	return true;
}
