// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
#include "W_NomadicLayout.generated.h"

struct FCardSaveData;
class UScrollBox;
class UHorizontalBox;
class UW_RTSCard;
class UW_NomadicLayoutBuilding;
struct FNomadicBuildingLayoutData;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_NomadicLayout : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitNomadicLayout(const TArray<FNomadicBuildingLayoutData>& BuildingsData,
	                       TSubclassOf<UW_NomadicLayoutBuilding> BuildingLayoutClass,
	                       TSubclassOf<UW_RTSCard> InCardClass, TObjectPtr<UW_CardMenu> InCardMenu);

	bool CheckLayoutsFilled(uint32& OutEmptySlots,
	                        TArray<ECardType>& OutUnfilledLayouts, TArray<UW_RTSCard*>& OutSelectedCards, const TMap<ECardType, TArray<ERTSCard>>&
	                        InCardsLeftPerLayout);

	TArray<FNomadicBuildingLayoutData> GetBuildingLayoutData() const;

	TArray<ECardType> GetAllLayouts() const;
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UScrollBox* BuildingLayoutScrollBox;

private:
	UPROPERTY()
	TArray<TObjectPtr<UW_NomadicLayoutBuilding>> M_BuildingLayoutWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UHorizontalBox>> M_HorizontalBoxes;

	/** @return Either the last horzontl box added that has not yet reached the maximum amount of building layouts, or a new horizontal box
	 * which is added to the scroll box*/
	UHorizontalBox* GetVacantHorizontalBox(int32& OutIndexInHzBox);

	bool GetIsScrollBoxValid() const;
};
