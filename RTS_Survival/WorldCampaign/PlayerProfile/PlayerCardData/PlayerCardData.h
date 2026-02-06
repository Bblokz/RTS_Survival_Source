#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicBuildingLayoutData/NomadicBuildingLayoutData.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"

#include "PlayerCardData.generated.h"
USTRUCT(BlueprintType)
struct FPlayerCardSaveData
{
	GENERATED_BODY()

	/**
	 * @brief The player’s remaining “inventory” of cards that should appear in the card picker scroll box.
	 *
	 * This is rebuilt from the scroll box on save (GetNonEmptyCardsHeld) and used on load to repopulate the
	 * card picker. Cards that are currently slotted into holders/layouts should not also remain here
	 * (the old ProfileValidator also enforced this for nomadic layouts). Duplicates represent quantity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ERTSCard> PlayerAvailableCards;

	/**
	 * @brief Flattened snapshot of all selected card effects at the moment the profile was saved.
	 *
	 * Built from the currently selected card widgets (units holder + tech/resources holder + nomadic layouts).
	 * This is the data the runtime loader uses to apply gameplay: spawn starting units, grant resource bonuses,
	 * apply researched technologies, and (optionally) update nomadic training options.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FCardSaveData> SelectedCards;

	/**
	 * @brief The current contents of the “Starting Units” card holder in the card menu.
	 *
	 * Used to reconstruct the UI loadout when reopening the card menu. Should contain only non-empty unit cards
	 * and should not exceed MaxCardsInUnits.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ERTSCard> CardsInUnits;

	/**
	 * @brief The current contents of the “Tech / Resources” card holder in the card menu.
	 *
	 * Used to reconstruct the UI loadout when reopening the card menu. Should contain only non-empty tech/resource
	 * cards and should not exceed MaxCardsInTechResources.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ERTSCard> CardsInTechResources;

	/**
	 * @brief Capacity of the “Starting Units” holder.
	 *
	 * Determines how many unit slots exist in the UI and is used when validating whether all slots are filled
	 * (e.g., before starting the game).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 MaxCardsInUnits = 0;

	/**
	 * @brief Capacity of the “Tech / Resources” holder.
	 *
	 * Determines how many tech/resource slots exist in the UI and is used when validating whether all slots are filled
	 * (e.g., before starting the game).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 MaxCardsInTechResources = 0;

	/**
	 * @brief Per-building nomadic layout configuration (which training cards are slotted per building).
	 *
	 * Each entry describes a nomadic building’s layout (building type, slot count, and the cards placed into it).
	 * Used to reconstruct the nomadic layout UI and to validate empty layout slots. Cards listed here are treated
	 * as “consumed by the layout” and should not also exist in PlayerAvailableCards.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, meta=(TitleProperty="BuildingType"))
	TArray<FNomadicBuildingLayoutData> NomadicBuildingLayouts;
};
