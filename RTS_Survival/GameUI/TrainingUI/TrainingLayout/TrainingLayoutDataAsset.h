// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "TrainingLayoutTypes.h"
#include "TrainingLayoutDataAsset.generated.h"

class UDataTable;

/**
 * @brief Make a data asset of this class and pick it in Project Settings > Training Layout.
 * It lists, per player sub faction, which unit sits in which training slot of each trainer building
 * when the player has everything unlocked; unit cards then decide which of those slots are filled.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UTrainingLayoutDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UTrainingLayoutDataAsset();

	/**
	 * @brief Falls back to the strike division for factions without a layout of their own, so that a new
	 * faction can train units before a designer has filled in its layout.
	 * @param Faction The sub faction of the player.
	 * @param Building The trainer building to find the layout of.
	 * @return Null when neither the faction nor the fallback faction has a layout for this building.
	 */
	const FTrainingLayoutBuilding* FindBuildingLayout(const ERTSFaction Faction, const ENomadicSubtype Building) const;

	// On: every slot of the layout is available from the start of the game, whatever cards the player owns.
	// Off: a slot is only filled when the player profile holds the training card of that unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlocks")
	bool bUnlockAllUnitsImmediately = true;

	// The amount of sub sections of a building is expected to be the same for every faction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training Layout")
	TMap<ERTSFaction, FTrainingLayoutFaction> FactionLayouts;

	// The buttons of the quick navigation panel, from left to right.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Navigation", meta = (TitleProperty = "Building"))
	TArray<FTrainingQuickNavEntry> QuickNavBuildings;

	// The table the training items load their icons from; the quick navigation buttons use the same icons.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Navigation")
	TSoftObjectPtr<UDataTable> TrainingOptionIconTable;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	void InitDefaultQuickNavBuildings();
	void InitDefaultStrikeDivisionLayout();
	void InitDefaultBreakthroughLayout();

	static FTrainingLayoutBuilding MakeDefaultHeadquartersLayout();
	static FTrainingLayoutBuilding MakeDefaultAirbaseLayout();
};
