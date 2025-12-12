// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BottomCenter_ChildPanel.h"

#include "RTS_Survival/GameUI/BottomCenterUI/BottomCenterUIPanel/W_BottomCenterUI.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_BottomCenter_ChildPanel::SetParentWidget(UW_BottomCenterUI* NewParent)
{
	M_ParentWidget = NewParent;
}

bool UW_BottomCenter_ChildPanel::EnsureValidParent() const
{
	if (not M_ParentWidget.IsValid())
	{
		RTSFunctionLibrary::ReportError("UW_BottomCenter_ChildPanel has no parent widget set. "
			"Is it needed before thet SetParenetWidget is called?");
		return false;
	}
	return true;
}
