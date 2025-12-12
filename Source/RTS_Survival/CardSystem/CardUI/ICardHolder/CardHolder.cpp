// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "CardHolder.h"


// Add default functionality here for any ICardHolder functions that are not pure virtual.
bool ICardHolder::CheckIsCardTypeAllowed(const ECardType& CardType) const
{
	return GetCardTypesAllowed().Contains(CardType);
}

TArray<TPair<ERTSCard, ECardType>> ICardHolder::GetNonEmptyCardsHeld()
{
	return {}; 
}

TArray<UW_RTSCard*> ICardHolder::GetSelectedCards()
{
	return {};
}
