// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnitVariantTypes.h"

#include "UnitVariantSettings.generated.h"

class UUnitVariantDataAsset;

/**
 * @brief Project Settings > Unit Variants: points the game to the data asset that lists which variants each
 * German unit implements, and holds a debug list to force variants before the selection UI exists.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Unit Variants"))
class RTS_SURVIVAL_API UUnitVariantSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UUnitVariantSettings* Get()
	{
		return GetDefault<UUnitVariantSettings>();
	}

	/** @return The picked asset, or the class defaults when none is picked, so every unit resolves to Base. */
	const UUnitVariantDataAsset* LoadVariantDataAsset() const;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Variants")
	TSoftObjectPtr<UUnitVariantDataAsset> VariantDataAsset;

	// Development aid until the selection UI exists: applied over the saved selection when the manager initialises.
	// Ignored in Shipping builds. Clear it before committing.
	UPROPERTY(Config, EditAnywhere, Category = "Unit Variants|Debug", meta = (TitleProperty = "Unit"))
	TArray<FUnitVariantSelection> DebugForcedVariants;
};
