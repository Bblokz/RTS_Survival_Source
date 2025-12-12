#include "FActionUIContainer.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/SlateWrapperTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FActionUIContainer::SetActionUIVisibility(const bool bVisible) const
{
	if(not EnsureActionUIIsValid())
	{
		return;
	}
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	M_ActionUIBorder->SetVisibility(NewVisibility);
	M_ActionUIHorizontalBox->SetVisibility(NewVisibility);
}


bool FActionUIContainer::EnsureActionUIIsValid() const
{
	if (not M_ActionUIBorder || not M_ActionUIHorizontalBox)
	{
		const FString ActionUIBorderValidity = M_ActionUIBorder ? "valid" : "invalid";
		const FString ActionUIHorizontalBoxValidity = M_ActionUIHorizontalBox ? "valid" : "invalid";
		RTSFunctionLibrary::ReportError("Action Ui container not initialized!"
			"\n ActionUIBorder is " + ActionUIBorderValidity +
			"\n ActionUIHorizontalBox is " + ActionUIHorizontalBoxValidity);
		return false;
	}
	return true;
}
