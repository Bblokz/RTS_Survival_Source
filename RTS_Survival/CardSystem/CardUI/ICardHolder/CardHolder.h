// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CardHolder.generated.h"

class UW_RTSCard;
enum class ERTSCard : uint8;
enum class ECardType : uint8;
// This class does not need to be modified.
UINTERFACE()
class UCardHolder : public UInterface
{
	GENERATED_BODY()
};

/**
 * The interface determines what card types are allowed to be held by the card holder.
 * for tech and unit card holders; the card menu propagates their allowed card types.
 * for the nomadic layout buildings; the allowed card type is inferred from the building type.
 */
class RTS_SURVIVAL_API ICardHolder
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual TSet<ECardType> GetCardTypesAllowed() const = 0;

	virtual bool GetIsScrollBox() const = 0;

	bool CheckIsCardTypeAllowed(const ECardType& CardType) const;

		virtual TArray<TPair<ERTSCard, ECardType>> GetNonEmptyCardsHeld();
	virtual TArray<UW_RTSCard*> GetSelectedCards();

};
