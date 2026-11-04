// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SurvivalModeLauncher.generated.h"

struct FSurvivalMapChoice;

/**
 * @brief Main menu entry for survival: the map, then faction, variant and difficulty selection, then the survival map.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalModeLauncher : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief The SURVIVAL button: goes straight to the given map, or lets the player pick one when none is given.
	 * @param SurvivalMap The survival level to play; empty offers the survival maps.
	 */
	UFUNCTION(BlueprintCallable, Category = "Survival", meta = (WorldContext = "WorldContextObject"))
	static void LaunchSurvivalMode(const UObject* WorldContextObject, TSoftObjectPtr<UWorld> SurvivalMap);

	/**
	 * @brief Stores the survival map as the pre-game destination, turns on the variant selection and opens the
	 * faction selection map.
	 * @param SurvivalMap The survival level to play.
	 */
	static void LaunchSurvivalOnMap(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld>& SurvivalMap);

	/** @return The survival maps offered in the menu. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	static TArray<TSoftObjectPtr<UWorld>> GetSurvivalMaps();

private:
	static TArray<FSurvivalMapChoice> GetSurvivalMapChoices();
	static void ShowMapPicker(const UObject* WorldContextObject);
};
