// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/ICardHolder/CardHolder.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "W_CardScrollBox.generated.h"

class UBorder;
class UCanvasPanelSlot;
class USizeBox;
class UW_CardMenu;
class UHorizontalBox;
class UW_RTSCard;
enum class ERTSCard : uint8;
class UScrollBox;
/**
 * @brief Displays and manages the scrollable list of card widgets for selection.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CardScrollBox : public UUserWidget, public ICardHolder
{
	GENERATED_BODY()

public:
	void InitCardScrollBox(const TArray<ERTSCard>& Cards, TSubclassOf<UUserWidget> InCardClass,
	                       TObjectPtr<UW_CardMenu> InCardMenu, const float NewScrollBoxHeightMlt);

	virtual TSet<ECardType>	GetCardTypesAllowed() const override final;

	virtual bool GetIsScrollBox() const override;

	void AddCardToScrollBoxPreferEmptySlot(const ERTSCard CardType);

	virtual TArray<TPair<ERTSCard, ECardType>> GetNonEmptyCardsHeld() override;
	TArray<ERTSCard> GetCardsOfType(const ECardType CardType);

	

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UScrollBox* CardScrollBox;


	UPROPERTY(EditAnywhere, Category = "CardScrollBox")
	float ExpansionMultiplier = 3.0f;


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Clicked")
	void OnSortByRarity();


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Clicked")
	void OnSortByType();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* ScrollBoxBorder;

	virtual void NativeConstruct() override;
	virtual void NativeOnInitialized() override;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UW_RTSCard>> M_CardsInScrollBox;

	UPROPERTY()
	TArray<TObjectPtr<UHorizontalBox>> M_HorizontalBoxes;

	// Current index in the type sorting order
	int32 CurrentTypeSortIndex = 0;

	// Array of card types to define the sorting order
	TArray<ECardType> SortTypeOrder;

	/** @return Either the last horzontl box added that has not yet reached the maximum amount of cards, or a new horizontal box
	 * which is added to the scroll box*/
	UHorizontalBox* GetVacantHorizontalBox(int32& OutIndexInHzBox);

	bool GetIsValidScrollBox() const;
	bool GetIsValidCardMenu() const;
	bool GetIsValidScrollBoxBorder() const;

	void Debug(UW_RTSCard* CreatedCard, ERTSCard CardType);
	void UpdateCardWidgetsInScrollBox();
	void ResizeBorderScrollBox(const bool bSetToOriginalSize);

	UPROPERTY()
	TObjectPtr<UW_CardMenu> M_CardMenu;

	UPROPERTY()
	TSubclassOf<UUserWidget> M_BlueprintCardClass;

	UPROPERTY()
	bool bM_RaritySortAscending = false;

	UPROPERTY()
	bool bM_TypeSortAscending = false;

	UPROPERTY()
	float M_OriginalScrollBoxHeight = 0.0f;

	UPROPERTY()
	float M_ScrollBoxHeightMlt = 3.0f;

	void EnsureCardArrayIsValid();

	void CreateNewCardWidget(const ERTSCard CardType);

	

};
