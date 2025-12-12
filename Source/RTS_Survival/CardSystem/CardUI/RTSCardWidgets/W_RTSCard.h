// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "RTS_Survival/CardSystem/CardUI/ICardHolder/CardHolder.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "W_RTSCard.generated.h"

class ICardHolder;
class UW_CardMenu;
class UW_CardHolder;
class UW_CardScrollBox;
enum class ERTSCard : uint8;
enum class ECardType : uint8;

USTRUCT(Blueprintable)
struct FRTSCardData
{
	GENERATED_BODY()
	FRTSCardData();

	UPROPERTY(BlueprintReadWrite)
	FUnitCost Resources;
	UPROPERTY(BlueprintReadWrite)
	ETechnology Technology;
	// This training option is used in async spawner at game start to spawn units.
	UPROPERTY(BlueprintReadWrite)
	FTrainingOption UnitToCreate;
	UPROPERTY(BlueprintReadWrite)
	FTrainingOption TrainingOption;
	UPROPERTY(BlueprintReadWrite)
	ECardType CardType;
	UPROPERTY(BlueprintReadWrite)
	ERTSCardRarity CardRarity;
};

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_RTSCard : public UUserWidget
{
	GENERATED_BODY()

public:
	FUnitCost GetResources() const { return M_CardData.Resources; }

	ETechnology GetTechnology() const { return M_CardData.Technology; }

	FTrainingOption GetUnitToCreate() const { return M_CardData.UnitToCreate; }
	bool GetIsLeftSide() const { return bIsLeftSide; }

	FTrainingOption GetTrainngOption() const { return M_CardData.TrainingOption; }

	ECardType GetCardType() const { return M_CardData.CardType; }

	ERTSCardRarity GetCardRarity() const { return M_CardData.CardRarity; }

	void SetCardHolder(ICardHolder* NewCardHolder) { M_CardHolder = NewCardHolder; }
	TWeakInterfacePtr<ICardHolder> GetCardHolder() const { return M_CardHolder; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSCard")
	ERTSCard GetCard() const { return M_RTSCard; }

	void SetCard(const ERTSCard NewCard) { M_RTSCard = NewCard; }

	// Updates visuals in derived blueprint.
	void InitializeCard(const ERTSCard NewCard);

	/** Set whether this card uses the left or right side cardvierwer. */
	void SetIsLeftSide(const bool bIsLeft) { bIsLeftSide = bIsLeft; }

	void SetCardMenu(const TObjectPtr<UW_CardMenu>& NewCardMenu) ;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "RTSCard")
	void OnSetupCard();

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void BP_InitDragWidget(const ERTSCard RTSCard);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="CardDrop")
	void OnDropRTSCard(UW_RTSCard* Other, const ERTSCard DroppedCard);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTSCard")
	void SetDataTableData(const FRTSCardData& NewData);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "RTSCard")
	UW_CardMenu* GetCardMenu() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTSCard")
	void OnRightClickCard();

private:
	void OnHovered(const bool bIsHovering);

	UPROPERTY()
	ERTSCard M_RTSCard;

	// Initialized with data from the datatable after InitializeCard is called.
	UPROPERTY()
	FRTSCardData M_CardData;

	UPROPERTY()
	bool bIsLeftSide;

	UPROPERTY()
	TWeakObjectPtr<UW_CardMenu> M_CardMenu;

	bool GetIsValidCardMenu() const;

	bool GetIsDropAllowed(const ERTSCard DroppedCard, UW_RTSCard* Other) const;

	TWeakInterfacePtr<ICardHolder> M_CardHolder;

	bool CheckIsDroppingIntoScrollBox(UW_RTSCard* Other) const;
};
