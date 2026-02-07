// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NomadicLayoutBuildingType.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/ICardHolder/CardHolder.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "W_NomadicLayoutBuilding.generated.h"

class UW_CardMenu;
class UScrollBox;
class UW_RTSCard;
struct FNomadicBuildingLayoutData;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_NomadicLayoutBuilding : public UUserWidget, public ICardHolder
{
	GENERATED_BODY()

public:
	void InitNomadicLayoutBuilding(const FNomadicBuildingLayoutData& BuildingData, TSubclassOf<UW_RTSCard> InCardClass,
	                               TObjectPtr<UW_CardMenu> CardMenu, const bool bIsLeftSide);

	virtual TSet<ECardType> GetCardTypesAllowed() const override final;
	virtual bool GetIsScrollBox() const override;

	virtual TArray<TPair<ERTSCard, ECardType>> GetNonEmptyCardsHeld() override final;
	virtual TArray<UW_RTSCard*> GetSelectedCards() override final;
	inline int32 GetMaxCardsToHold() const { return M_Slots; } ;
	inline ECardType GetCardType()const;

	FNomadicBuildingLayoutData GetBuildingLayoutData() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UScrollBox* CardScrollBox;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CardsScale = 1.2;

	UFUNCTION(BlueprintImplementableEvent, Category = "NomadicLayoutBuilding")
	void OnSetupNOmadicBuildingLayout(const ENomadicLayoutBuildingType BuildingType);

private:
	UPROPERTY()
	TArray<TObjectPtr<UW_RTSCard>> M_CardsInScrollBox = {};

	UPROPERTY()
	int32 M_Slots = 0;

	UPROPERTY()
	ENomadicLayoutBuildingType M_BuildingType = ENomadicLayoutBuildingType::Building_None;
	


	bool GetIsValidCardScrollBox() const;
	/**
	 * Creates a card in the scroll box and adds it to the array of cards.
	 * @param Type The type of card to create. 
	 * @param InCardClass The class bp derived, that is used to create the card.
	 * @param InCardMenu
	 * @param bIsLeftSide
	 * @pre The card scroll box is valid and so is the card class.
	 */
	void CreateCardOfType(ERTSCard Type, TSubclassOf<UW_RTSCard> InCardClass, TObjectPtr<UW_CardMenu> InCardMenu,
	                      const bool bIsLeftSide);

	UPROPERTY()
	TSet<ECardType> M_AllowedCardTypes;

	void SetupAllowedCardsFromBuildingType(const ENomadicLayoutBuildingType BuildingType);
};
