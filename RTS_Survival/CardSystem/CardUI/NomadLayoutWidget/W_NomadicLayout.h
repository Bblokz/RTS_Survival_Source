// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/LayoutProfileWidgets.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicLayoutBuildingType.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "W_NomadicLayout.generated.h"

struct FCardSaveData;
class UHorizontalBox;
class UW_RTSCard;
class UW_NomadicLayoutBuilding;
struct FNomadicBuildingLayoutData;
class UW_CardMenu;
/**
 * @brief Displays one nomadic building layout at a time while retaining data for all layouts.
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

	void SetFocusedLayoutProfile(const ELayoutProfileWidgets NewLayoutProfile);
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UHorizontalBox* BuildingLayoutBox;

private:
	UPROPERTY()
	TObjectPtr<UW_NomadicLayoutBuilding> M_BuildingLayoutWidget;

	UPROPERTY()
	TArray<FNomadicBuildingLayoutData> M_BuildingLayoutsData;

	UPROPERTY()
	TSubclassOf<UW_NomadicLayoutBuilding> M_BuildingLayoutClass;

	UPROPERTY()
	TSubclassOf<UW_RTSCard> M_CardClass;

	UPROPERTY()
	TWeakObjectPtr<UW_CardMenu> M_CardMenu;

	UPROPERTY()
	ELayoutProfileWidgets M_CurrentLayoutProfile = ELayoutProfileWidgets::Widgets_None;

	UPROPERTY()
	ENomadicLayoutBuildingType M_CurrentBuildingType = ENomadicLayoutBuildingType::Building_None;

	void UpdateFocusedBuildingData();

	int32 GetBuildingLayoutIndex(const ENomadicLayoutBuildingType BuildingType) const;

	ENomadicLayoutBuildingType GetBuildingTypeFromLayoutProfile(const ELayoutProfileWidgets LayoutProfile) const;

	ECardType GetCardTypeFromBuildingType(const ENomadicLayoutBuildingType BuildingType) const;

	void AppendSelectedCardsFromLayouts(TArray<UW_RTSCard*>& OutSelectedCards) const;

	bool GetIsScrollBoxValid() const;
};
