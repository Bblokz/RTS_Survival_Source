// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/ICardHolder/CardHolder.h"
#include "W_CardHolder.generated.h"

class UW_CardMenu;
enum class ERTSCard : uint8;
class UW_RTSCard;
class UScrollBox;
enum class ECardType : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_CardHolder : public UUserWidget, public ICardHolder
{
	GENERATED_BODY()

public:

	virtual TSet<ECardType> GetCardTypesAllowed() const override final;
	virtual bool GetIsScrollBox() const override;
	TArray<TObjectPtr<UW_RTSCard>> GetCards() { return M_Cards; }

	void SetupCardHolder(const TArray<ERTSCard>& Cards, TSubclassOf<UW_RTSCard> InCardClass,
	                     int32 MaxCards, const TSet<ECardType>& AllowedCardTypes,
	                     TObjectPtr<UW_CardMenu> InCardMenu);

	virtual TArray<TPair<ERTSCard, ECardType>> GetNonEmptyCardsHeld() override final;
	int32 GetMaxCardsToHold() const;
	virtual TArray<UW_RTSCard*> GetSelectedCards() override final;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UScrollBox* CardHolder;


private:
	UPROPERTY()
	TArray<TObjectPtr<UW_RTSCard>> M_Cards;

	UPROPERTY()
	TSet<ECardType> M_CardTypeToHold;

	UPROPERTY()
	int32 M_MaxCardsToHold;

	bool GetIsValidCardHolder() const;

	UPROPERTY()
	TSubclassOf<UUserWidget> M_BlueprintCardClass;

	UW_RTSCard* CreateCardWidgetInHolder(const ERTSCard& CardType,
	                                     const TObjectPtr<UW_CardMenu> InCardMenu,
	                                     const bool bIsLeftSide, const TSet<ECardType>& AllowedCardTypes);



};
