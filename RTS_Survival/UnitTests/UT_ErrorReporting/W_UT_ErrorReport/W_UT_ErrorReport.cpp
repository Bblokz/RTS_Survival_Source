// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UT_ErrorReport.h"

#include "Components/ScrollBox.h"
#include "RTS_Survival/UnitTests/UT_ErrorReporting/UT_ErrorReportManager/UT_ErrorReportManager.h"
#include "RTS_Survival/UnitTests/UT_ErrorReporting/W_UT_ErrorElement/W_UT_ErrorElement.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_UT_ErrorReport::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UW_UT_ErrorReport::SetErrorReportManager(AUT_ErrorReportManager* InManager)
{
	M_ErrorReportManager = InManager;
}

bool UW_UT_ErrorReport::GetIsValidErrorReportManager() const
{
	if (M_ErrorReportManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_ErrorReportManager"),
	                                                      TEXT("GetIsValidErrorReportManager") );
	return false;
}

void UW_UT_ErrorReport::AddError(const FString& Message, const FVector& WorldLocation)
{
	if (not ErrorScrolBox)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_UT_ErrorReport has no ErrorScrolBox bound."));
		return;
	}
	if (not M_ErrorElementClass)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_UT_ErrorReport missing M_ErrorElementClass on defaults."));
		return;
	}

	UWorld* const World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_UT_ErrorReport has no valid UWorld."));
		return;
	}

	UW_UT_ErrorElement* const NewElement =
		CreateWidget<UW_UT_ErrorElement>(World, M_ErrorElementClass);
	if (not IsValid(NewElement))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create UW_UT_ErrorElement."));
		return;
	}

	NewElement->SetOwningReport(this);

	// Add to ScrollBox before applying layout rules (needs slot).
	ErrorScrolBox->AddChild(NewElement);

	// Apply placement rules and optional auto-scroll.
	FinalizeNewElement(NewElement);

	// Initialize the element content (message + location).
	NewElement->InitErrorElement(Message, WorldLocation);
}

void UW_UT_ErrorReport::FinalizeNewElement(UW_UT_ErrorElement* NewElement)
{
	if (not IsValid(NewElement))
	{
		return;
	}

	NewElement->ApplyPlacementRulesToParentSlot();

	// Auto-scroll to end as a sensible default (the element may have asked for it in its rules).
	if (ErrorScrolBox)
	{
		ErrorScrolBox->ScrollToEnd();
	}
}

void UW_UT_ErrorReport::MoveTo(const FVector& WorldLocation) const
{
	if (not GetIsValidErrorReportManager())
	{
		return;
	}
	M_ErrorReportManager->MoveTo(WorldLocation);
}
