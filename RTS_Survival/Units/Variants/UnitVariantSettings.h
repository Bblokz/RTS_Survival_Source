// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "UnitVariantSettings.generated.h"

class UUnitVariantDataAsset;

/**
 * @brief Project Settings > Unit Variants: pick the UUnitVariantDataAsset the game uses. All variant data, including
 * the debug forced-variant list, is on that asset; add new variant properties there, not here.
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
};
