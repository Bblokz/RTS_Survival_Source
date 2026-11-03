// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SurvivalModeLauncher.generated.h"

/**
 * @brief Main menu entry for survival: faction, variant and difficulty selection, then the survival map.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalModeLauncher : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Stores the survival map as the pre-game destination, turns on the variant selection and opens the
	 * faction selection map.
	 * @param SurvivalMap The survival level to play; empty uses the first survival map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Survival", meta = (WorldContext = "WorldContextObject"))
	static void LaunchSurvivalMode(const UObject* WorldContextObject, TSoftObjectPtr<UWorld> SurvivalMap);

	/** @return The survival maps offered in the menu. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	static TArray<TSoftObjectPtr<UWorld>> GetSurvivalMaps();
};
