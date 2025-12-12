// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_CardViewer.h"

#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"

void UW_CardViewer::ViewCard(const ERTSCard Card) const
{
	if (RTSCard.IsValid())
	{
		RTSCard->InitializeCard(Card);
	}
}
