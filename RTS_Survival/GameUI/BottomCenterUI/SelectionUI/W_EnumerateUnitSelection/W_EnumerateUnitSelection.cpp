// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_EnumerateUnitSelection.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectionPanel/W_SelectionPanel.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_EnumerateUnitSelection::InitOwnerAndIndex(UW_SelectionPanel* InOwner, const int32 InPageIndex)
{
	if (not IsValid(InOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("EnumerateUnitSelection::InitOwnerAndIndex owner invalid."));
		return;
	}
	M_OwnerPanel = InOwner;
	M_PageIndex = InPageIndex;
}

void UW_EnumerateUnitSelection::SetLabel(const FText& InText) const
{
	if (not IsValid(EnumerationText))
	{
		return;
	}
	EnumerationText->SetText(InText);
}

void UW_EnumerateUnitSelection::OnClickedEnumerate()
{
	if (not M_OwnerPanel.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Enumerate click: panel invalid."));
		return;
	}
	M_OwnerPanel.Get()->HandleEnumerateClicked(M_PageIndex);
}
