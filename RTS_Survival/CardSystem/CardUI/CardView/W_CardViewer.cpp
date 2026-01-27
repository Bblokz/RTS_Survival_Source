// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_CardViewer.h"

#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_CardViewer::ViewCard(const ERTSCard Card) const
{
	if (not GetIsValidRTSCard())
	{
		return;
	}
	RTSCard->InitializeCard(Card);
}

bool UW_CardViewer::GetIsValidRTSCard() const
{
	if (not RTSCard.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"RTSCard",
			"GetIsValidRTSCard",
			this
		);
		return false;
	}
	return true;
}
