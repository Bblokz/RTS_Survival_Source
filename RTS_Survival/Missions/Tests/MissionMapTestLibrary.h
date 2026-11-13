// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/Abilities.h"

#include "MissionMapTestLibrary.generated.h"

class ANomadicVehicle;
enum class EBuildingExpansionType : uint8;

/**
 * @brief Lets headless mission tests (python in a throwaway editor) play the player's part through the same code the
 * UI runs: a training click on a trainer's menu item, a building expansion option click and its placement click.
 * Every function does nothing and returns false in builds without map tests (Shipping).
 */
UCLASS()
class RTS_SURVIVAL_API UMissionMapTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Clicks a training option on the trainer's menu as the player would (mission locks and costs apply).
	 * @param Trainer A player nomadic with a trainer, e.g. the HQ.
	 * @param Option The option to train.
	 * @return 1 queued, 2 shown as locked by the mission (not queued), 3 shown but not queued, 0 not on the menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static int32 MapTest_ClickTrainingOption(ANomadicVehicle* Trainer, FTrainingOption Option);

	/**
	 * @brief Clicks a building expansion option of the owner as the player would; the expansion then loads and waits
	 * for its placement click (MapTest_PlaceLoadedBxp).
	 * @param ExpansionOwner The player nomadic building that offers the expansion.
	 * @param ExpansionType The expansion to build.
	 * @return False when the owner does not offer it or has no free slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static bool MapTest_StartBxpPlacement(ANomadicVehicle* ExpansionOwner, EBuildingExpansionType ExpansionType);

	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static bool MapTest_IsBxpWaitingForPlacement(const UObject* WorldContextObject);

	/**
	 * @brief The placement click: snaps like a real click (sockets included), pays and starts the construction.
	 * @return False when nothing waits for placement or the player cannot pay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static bool MapTest_PlaceLoadedBxp(const UObject* WorldContextObject, FVector ClickLocation);

	/**
	 * @brief The player converts a nomadic truck: selects it, presses convert and clicks the preview at Location.
	 * @param NomadicTruck A player nomadic in truck mode.
	 * @param Location Ground location of the placement click.
	 * @return 1 ordered, 2 not a truck, 3 no nomadic preview started, 4 the spot was rejected, 5 the click was not
	 * taken, 0 invalid input; +10 when the preview demanded a build radius (never for the registered player HQ).
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static int32 MapTest_ConvertNomadicAt(ANomadicVehicle* NomadicTruck, FVector Location);

	/** @return True when the unit's command card holds the ability. */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static bool MapTest_HasAbility(AActor* Unit, EAbilityID Ability);
};
