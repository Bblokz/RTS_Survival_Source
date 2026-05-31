// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSMinimapDeveloperSettings.generated.h"

class UMinimapIconDataAsset;

/**
 * @brief Project settings used by the minimap to resolve custom always-visible icon textures.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "RTS Minimap"))
class RTS_SURVIVAL_API URTSMinimapDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "MiniMap")
	TSoftObjectPtr<UMinimapIconDataAsset> M_MinimapIconDataAsset;
};
