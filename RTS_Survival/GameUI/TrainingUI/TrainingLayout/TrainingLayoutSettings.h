// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TrainingLayoutSettings.generated.h"

class UTrainingLayoutDataAsset;

/**
 * @brief Project Settings > Training Layout: points the game to the data asset that lays out the
 * training UI of every trainer building per player sub faction.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Training Layout"))
class RTS_SURVIVAL_API UTrainingLayoutSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UTrainingLayoutSettings* Get()
	{
		return GetDefault<UTrainingLayoutSettings>();
	}

	/**
	 * @return The picked data asset, or the code defaults of the data asset class when none is picked,
	 * so that training keeps working in a project without the asset.
	 */
	const UTrainingLayoutDataAsset* LoadTrainingLayout() const;

	UPROPERTY(Config, EditAnywhere, Category="Training Layout")
	TSoftObjectPtr<UTrainingLayoutDataAsset> TrainingLayout;
};
