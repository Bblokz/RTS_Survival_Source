// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeckBuilderTypes.h"
#include "UObject/Object.h"

#include "DeckCardCatalog.generated.h"

class UDataTable;

/**
 * @brief Created by the deck builder menu to read the card data table once. The table's row struct is a Blueprint
 * struct, so the columns are found by name and read by reflection; the old card menu learned the same data from a
 * Blueprint card widget per card.
 */
UCLASS()
class RTS_SURVIVAL_API UDeckCardCatalog : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Reads every row whose name is an ERTSCard value; rows of any other name are reported and skipped.
	 * @param CardTable DT_RTSCards; kept by this object so the card pictures stay loaded.
	 * @return The number of cards read.
	 */
	int32 LoadFromTable(const UDataTable* CardTable);

	const TMap<ERTSCard, FDeckCardDefinition>& GetDefinitions() const { return M_Definitions; }

	/** @return Null for a card the table has no row for. */
	const FDeckCardDefinition* FindDefinition(const ERTSCard Card) const;

	/** @return The training card of this unit (keyed as variants key units), null when no card trains it. */
	const FDeckCardDefinition* FindDefinitionOfUnit(const FTrainingOption& VariantUnit) const;

private:
	UPROPERTY()
	TMap<ERTSCard, FDeckCardDefinition> M_Definitions;

	UPROPERTY()
	TObjectPtr<const UDataTable> M_CardTable;
};
