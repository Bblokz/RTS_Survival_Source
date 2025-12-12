// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UT_ErrorElement.h"

#include "Components/Button.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBoxSlot.h"
#include "RTS_Survival/UnitTests/UT_ErrorReporting/W_UT_ErrorReport/W_UT_ErrorReport.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_UT_ErrorElement::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (MoveToButton)
	{
		// Safe binding; actual move is relayed through a weak owning report pointer.
		MoveToButton->OnClicked.AddUniqueDynamic(this, &UW_UT_ErrorElement::OnMoveToClicked);
	}
}

void UW_UT_ErrorElement::SetOwningReport(UW_UT_ErrorReport* InReport)
{
	M_OwningReport = InReport;
}

bool UW_UT_ErrorElement::GetIsValidOwningReport() const
{
	if (M_OwningReport.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_OwningReport"),
	                                                      TEXT("GetIsValidOwningReport") );
	return false;
}

void UW_UT_ErrorElement::InitErrorElement(const FString& InMessage, const FVector& InWorldLocation)
{
	M_ErrorLocation = InWorldLocation;

	if (not ErrorRichText)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_UT_ErrorElement has no ErrorRichText bound."));
		return;
	}

	ErrorRichText->SetText(FText::FromString(InMessage));
}

void UW_UT_ErrorElement::ApplyPlacementRulesToParentSlot() const
{
	const UPanelSlot* PanelSlot = Slot;
	if (not PanelSlot)
	{
		// Can't apply rules if no slot yet (should be added to ScrollBox first).
		return;
	}

	if (UScrollBoxSlot* const ScrollSlot = Cast<UScrollBoxSlot>(const_cast<UPanelSlot*>(PanelSlot)))
	{
		ScrollSlot->SetPadding(M_PlacementRules.Padding);
		ScrollSlot->SetHorizontalAlignment(M_PlacementRules.HAlign);
		ScrollSlot->SetVerticalAlignment(M_PlacementRules.VAlign);
	}
}

void UW_UT_ErrorElement::OnMoveToClicked()
{
	if (not GetIsValidOwningReport())
	{
		return;
	}

	// Relay the request; the report will forward to the manager which moves the camera.
	M_OwningReport->MoveTo(M_ErrorLocation);
}
